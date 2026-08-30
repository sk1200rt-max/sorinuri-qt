#include "MainWindow.h"
#include "UrlDialog.h"
#include "ProFeaturesWidget.h"
#include "ShortcutOverlay.h"
#include "WhisperWidget.h"
#include "UpscaleWidget.h"
#include "ChapterWidget.h"
#include "MiniPlayerWidget.h"
#include "UpdateChecker.h"
#include "UpdateDialog.h"
#include "AlbumArtExtractor.h"
#include "UiTheme.h"
#include <QApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QEvent>
#include <QMessageBox>
#include <QProgressDialog>
#include <QInputDialog>
#include <QFileDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <QMimeData>
#include <QUrl>
#include <QMenu>
#include <QStandardPaths>
#include <QAction>
#include <QActionGroup>
#include <QCryptographicHash>
#include <QDateTime>
#include <QTableWidget>
#include <QHeaderView>
#include <algorithm>
#include <tuple>

#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#endif

static const QStringList MEDIA_EXTS = {
    "mkv","mp4","avi","mov","wmv","flv","ts","m2ts","m4v","webm","ogv","3gp",
    "mp3","flac","aac","ogg","wav","wma","m4a","opus","dts","ac3","truehd"
};

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), settings_("Sorinuri", "SorinuriPlayer")
{
    setWindowTitle("소리누리");
    // 실제 최소 크기는 loadSettings()에서 현재 논리 화면 크기에 맞춰 계산한다.
    // 여기서는 250% 배율의 작은 논리 해상도를 막지 않는 안전한 하한만 둔다.
    setMinimumSize(640, 420);
    setAcceptDrops(true);
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);

    // yt-dlp 관리자 - 지연 초기화 (시작 직후 네트워크 요청 방지)
    ytdlp_ = nullptr;
    playbackQueue_ = new PlaybackQueue(this);

    // 절전 복귀와 HDMI 장치 변경은 연속해서 발생할 수 있다. 타이머를 재시작해
    // 드라이버가 완전히 복원된 뒤 한 번만 WASAPI 출력을 재협상한다.
    audioOutputRecoveryTimer_ = new QTimer(this);
    audioOutputRecoveryTimer_->setSingleShot(true);
    connect(audioOutputRecoveryTimer_, &QTimer::timeout, this, [this]() {
        if (!mpvWidget_ || !mpvWidget_->core()) return;
        mpvWidget_->core()->restoreAudioOutputAfterDeviceChange();
        qInfo() << "[MainWindow] 절전/장치 변경 후 오디오 출력 복구 완료";
    });

    setupUI();
    setupConnections();
    loadSettings();

    // 무거운 초기화는 창이 완전히 표시된 후 비동기로 실행
    QTimer::singleShot(500, this, [this]() {
        // yt-dlp 관리자 초기화 (500ms 지연)
        ytdlp_ = new YtdlpManager(this);
        connect(ytdlp_, &YtdlpManager::ytdlpReady,
                this, &MainWindow::onYtdlpReady);
        connect(ytdlp_, &YtdlpManager::downloadProgress,
                this, &MainWindow::onYtdlpDownloadProgress);
        connect(ytdlp_, &YtdlpManager::downloadFailed,
                this, &MainWindow::onYtdlpDownloadFailed);
        if (!pendingYouTubeQueue_.isEmpty()) {
            const QList<PlaybackQueue::Entry> queue = pendingYouTubeQueue_;
            const int startIndex = pendingYouTubeQueueStartIndex_;
            pendingYouTubeQueue_.clear();
            playQueue(queue, startIndex);
        }
    });

    // 스마트폰 리모컨 서버 자동 시작 (설정에서 활성화된 경우)
    QTimer::singleShot(1000, this, [this]() {
        if (settings_.value("remote/enabled", false).toBool())
            startRemoteServer();
    });
    // 자동 업데이트 체크 (앱 시작 5초 후 - 시작 직후 부하 방지)
    QTimer::singleShot(5000, this, [this]() {
        auto* updater = new UpdateChecker(this);
        connect(updater, &UpdateChecker::updateAvailable,
                this, [this](const QString& ver, const QString& notes, const QString& url) {
            auto* dlg = new UpdateDialog(ver, notes, url, this);
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            dlg->exec();
        });
        updater->checkForUpdates();
    });
}

MainWindow::~MainWindow() { saveSettings(); }

void MainWindow::scheduleAudioOutputRecovery(int delayMs) {
    if (!audioOutputRecoveryTimer_) return;
    audioOutputRecoveryTimer_->start(qMax(0, delayMs));
}

void MainWindow::setupUI() {
    // 키 이벤트가 항상 MainWindow로 전달되도록 포커스 정책 설정
    setFocusPolicy(Qt::StrongFocus);

    auto* central = new QWidget(this);
    central->setObjectName("mainSurface");
    central->setStyleSheet(QString(
        "QWidget#mainSurface { background: %1; color: %2; font-family: 'Segoe UI'; }")
        .arg(SorinuriUi::Surface, SorinuriUi::Text));
    setCentralWidget(central);

    auto* mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ── 타이틀바 ─────────────────────────────────────────────
    titleBar_ = new TitleBar(this);
    mainLayout->addWidget(titleBar_);

    // 모드 버튼은 ControlBar에 통합됨 (modeBar 제거)

    // ── 메인 스택 (플레이어 / OTT 페이지) ───────────────────────
    mainStack_ = new QStackedWidget(this);
    mainStack_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // ── 페이지 0: MPV 플레이어 ───────────────────────────────────────
    playerPage_ = new QWidget(this);
    auto* playerLayout = new QVBoxLayout(playerPage_);
    playerLayout->setContentsMargins(0, 0, 0, 0);
    playerLayout->setSpacing(0);

    auto* videoContainer = new QWidget(playerPage_);
    videoContainer->setStyleSheet("background: #000000;");
    videoContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto* videoLayout = new QVBoxLayout(videoContainer);
    videoLayout->setContentsMargins(0, 0, 0, 0);
    videoLayout->setSpacing(0);

    mpvWidget_ = new MpvWidget(videoContainer);
    mpvWidget_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    // 클릭 시 MainWindow가 포커스를 유지하도록 - 키 이벤트 정상 전달
    mpvWidget_->setFocusPolicy(Qt::ClickFocus);
    videoLayout->addWidget(mpvWidget_);
    // ── 플레이어 페이지 내부: 영상 vs 음악 스택 ────────────────────────────
    videoPage_ = videoContainer;  // videoPage_ 멤버에 저장 (멀티뷰 전환용)
    playerStack_ = new QStackedWidget(playerPage_);
    playerStack_->addWidget(videoContainer);  // index 0: 영상
    // 음악 화면은 스펙트럼 타이머·EQ 컨트롤을 다수 생성하므로 첫 음악 재생 때까지
    // 빈 플레이스홀더만 유지한다. 영상만 보는 사용자의 첫 창 표시를 빠르게 한다.
    musicPlaceholder_ = new QWidget(playerPage_);
    playerStack_->addWidget(musicPlaceholder_);  // index 1: 지연 생성 음악 화면
    playerLayout->addWidget(playerStack_, 1);
    // HiFi 엔진 초기화
    hifiEngine_ = new HiFiEngine(mpvWidget_->core(), this);
    miniPlayer_ = nullptr;  // 지연 초기화 (M 키 처음 누를 때 생성)
    // 혁신 기능 위젯은 지연 초기화 (P 키 패널 처음 열 때 생성)
    whisperWidget_   = nullptr;
    upscaleWidget_   = nullptr;
    chapterWidget_   = nullptr;


    mainStack_->addWidget(playerPage_);  // index 0

    // ── 페이지 1: OTT (WebView2) ──────────────────────────────────────
    ottPage_ = nullptr;  // 지연 초기화: 처음 클릭 시 생성
    mainStack_->addWidget(new QWidget(this));  // index 1 placeholder

    mainLayout->addWidget(mainStack_, 1);

    // ── 컨트롤바 + 트랙 선택기 통합 ─────────────────────────────────────────
    // 이전의 별도 trackBar 제거 → ControlBar 내부 중앙 영역에 TrackSelector 통합
    trackSelector_ = new TrackSelector(this);
    controlBar_ = new ControlBar(this);
    controlBar_->embedTrackSelector(trackSelector_);
    mainLayout->addWidget(controlBar_);

    // 오디오 정보는 ControlBar에 인라인으로 통합됨 (AudioInfoBar 제거)

    // ── 전문 기능 패널 (첫 사용 시 생성) ─────────────────────────────
    // 다수의 탭·컨트롤을 만드는 패널은 시작 화면을 늦추므로 P 키 또는 관련
    // 단축키를 처음 사용할 때 ensureProFeatures()에서 생성한다.
    proFeatures_ = nullptr;

    // ── 단축키 오버레이 (영상 위에 표시) ─────────────────────────────
    shortcutOverlay_ = new ShortcutOverlay(mpvWidget_);
    shortcutOverlay_->hide();
    // OSD 위젯 (화면 중앙 반투명 표시) - mpvWidget_ 위에 오버레이
    osdWidget_ = new OsdWidget(mpvWidget_);
    osdWidget_->hide();
    // 재생 정보 대시보드 오버레이 - 비최대화 모드에서 표시

    // ── 광고 관리자 초기화 ──────────────────────────────────────────────
    adManager_      = new AdManager(this);
    splashAdWidget_ = new SplashAdWidget(this);
    adManager_->setAppVersion("6.2.0");

    // 앱 시작 5초 후 시작 화면 광고 요청 (비동기, 광고 없으면 아무것도 안 표시)
    QTimer::singleShot(5000, this, [this]() {
        adManager_->fetchAd("splash");
    });
    connect(adManager_, &AdManager::adReady, this, [this](const QString& slot, const QJsonObject& ad) {
        if (ad.isEmpty()) return;
        if (slot == "splash" && splashAdWidget_)
            splashAdWidget_->showAd(ad);
    });
    connect(splashAdWidget_, &SplashAdWidget::clicked, this, [this](int adId, const QString& slot) {
        adManager_->reportClick(adId, slot);
    });
}

void MainWindow::ensureProFeatures() {
    if (proFeatures_) return;

    proFeatures_ = new ProFeaturesWidget(this);
    proFeatures_->hide();
    if (auto* layout = qobject_cast<QVBoxLayout*>(centralWidget()->layout()))
        layout->addWidget(proFeatures_);

    auto* core = mpvWidget_->core();
    proFeatures_->connectMpv(core);
    connect(proFeatures_, &ProFeaturesWidget::closeRequested,
            this, &MainWindow::toggleProFeatures);
}

void MainWindow::ensureMusicPage() {
    if (musicPage_) return;

    auto* core = mpvWidget_->core();
    musicPage_ = new MusicWidget(core, playerPage_);
    const int index = playerStack_->indexOf(musicPlaceholder_);
    playerStack_->insertWidget(index >= 0 ? index : 1, musicPage_);
    if (musicPlaceholder_) {
        playerStack_->removeWidget(musicPlaceholder_);
        musicPlaceholder_->deleteLater();
        musicPlaceholder_ = nullptr;
    }

    connect(musicPage_, &MusicWidget::seekRequested, this, &MainWindow::onMusicSeekRequested);
    connect(musicPage_, &MusicWidget::volumeChanged, this, &MainWindow::onMusicVolumeChanged);
    connect(musicPage_, &MusicWidget::playPauseRequested, core, &MpvCore::togglePause);
    connect(musicPage_, &MusicWidget::prevRequested, [core]() { core->command({"playlist-prev"}); });
    connect(musicPage_, &MusicWidget::nextRequested, [core]() { core->command({"playlist-next"}); });
    connect(musicPage_, &MusicWidget::eqRequested, this, &MainWindow::onSettingsRequested);
    connect(musicPage_, &MusicWidget::settingsRequested, this, &MainWindow::onSettingsRequested);
    connect(core, &MpvCore::positionChanged, musicPage_, [this](double pos) {
        if (musicPage_) musicPage_->updatePosition(pos, totalDuration_);
    });
    connect(core, &MpvCore::playbackStarted, musicPage_, [this]() {
        if (musicPage_) musicPage_->setPlaying(true);
    });
    connect(core, &MpvCore::playbackPaused, musicPage_, [this]() {
        if (musicPage_) musicPage_->setPlaying(false);
    });
    connect(core, &MpvCore::audioFormatChanged, musicPage_,
            [this, core](const QString&, int, int, const QString&) {
        if (!musicPage_ || !isMusicMode_) return;
        const int outRate = core->getProperty("audio-out-params/samplerate").toInt();
        const QString outFmt = core->getProperty("audio-out-params/format").toString();
        const bool exclusive = core->getProperty("audio-exclusive").toBool();
        if (outRate > 0) musicPage_->setOutputInfo(outRate, outFmt, exclusive);
    });
    connect(musicPage_, &MusicWidget::miniModeRequested, this, &MainWindow::toggleMiniPlayer);
    connect(musicPage_, &MusicWidget::compactModeRequested, this, &MainWindow::toggleCompactPlayer);
    connect(musicPage_, &MusicWidget::shuffleToggled, this, [core](bool on) {
        core->setProperty("shuffle", on ? QString("yes") : QString("no"));
    });
    connect(musicPage_, &MusicWidget::repeatToggled, this, [this](bool on) {
        playbackQueue_->setRepeatMode(on ? PlaybackQueue::RepeatMode::One
                                         : PlaybackQueue::RepeatMode::Off);
    });
}

void MainWindow::setupConnections() {
    // MpvWidget의 마우스 이벤트를 MainWindow가 가로체 → UI 자동 숨김/표시 동작
    mpvWidget_->setMouseTracking(true);
    mpvWidget_->installEventFilter(this);
    setMouseTracking(true);
    // 애플리케이션 전체 마우스 이동을 감지해 전체화면 UI 자동 숨김 상태를 안전하게 관리한다.
    qApp->installEventFilter(this);

    // HiDPI 근본 수정: mpvInitialized 시그널 연결
    // initializeGL() 완료 후 pendingStartupFiles_ 자동 처리
    // window.show() 직후 openFiles() 호출 시 initialized_=false로 무시되던 문제 근본 해결
    // Qt::SingleShotConnection: 한 번만 실행 (initializeGL은 한 번만 호출됨)
    connect(mpvWidget_, &MpvWidget::mpvInitialized, this, [this]() {
        // MPV 초기화 완료 후 오디오 설정 재적용
        // loadSettings()는 생성자에서 호출되어 MPV 초기화 전에 실행됨
        // initialized_=false로 setSpdifCodecs 등이 적용 안 됨 → 여기서 재적용
        auto* core = mpvWidget_->core();
        // WASAPI 독점 모드 재적용
        if (settings_.value("audio/exclusive", true).toBool())
            core->setAudioExclusive(true);
        // 패스스루 코덱 재적용 (DD+/DTS/TrueHD)
        if (settings_.value("audio/passthrough", true).toBool()) {
            QStringList codecs;
            if (settings_.value("audio/pt_ac3",    true).toBool()) codecs << "ac3";
            if (settings_.value("audio/pt_eac3",   true).toBool()) codecs << "eac3";
            if (settings_.value("audio/pt_dts",    true).toBool()) codecs << "dts";
            if (settings_.value("audio/pt_dtshd",  true).toBool()) codecs << "dts-hd";
            if (settings_.value("audio/pt_truehd", true).toBool()) codecs << "truehd";
            core->setSpdifCodecs(codecs);
            qInfo() << "[MainWindow] mpvInitialized: 패스스루 코덱 적용:" << codecs;
        }
        // 통합 대기열의 반복 모드는 MPV 초기화가 끝난 뒤 적용한다.
        applyQueueRepeatMode();
        // 시작 파일 처리
        if (!pendingStartupFiles_.isEmpty()) {
            qInfo() << "[MainWindow] mpvInitialized: pendingStartupFiles_ 처리" << pendingStartupFiles_;
            QStringList files = pendingStartupFiles_;
            pendingStartupFiles_.clear();
            openFiles(files);
        }
    }, Qt::SingleShotConnection);

    auto* core = mpvWidget_->core();

    connect(playbackQueue_, &PlaybackQueue::repeatModeChanged, this,
            [this](PlaybackQueue::RepeatMode mode) {
        applyQueueRepeatMode();
        const QString label = mode == PlaybackQueue::RepeatMode::All ? QStringLiteral("전체 반복")
                            : mode == PlaybackQueue::RepeatMode::One ? QStringLiteral("한 곡 반복")
                            : QStringLiteral("반복 끔");
        if (originalsWidget_) originalsWidget_->setRepeatMode(mode);
        if (osdWidget_) osdWidget_->showInfo(QStringLiteral("🔁 %1").arg(label), 1600);
    });

    connect(core, &MpvCore::fileLoaded,         this, &MainWindow::onFileLoaded);
    connect(core, &MpvCore::playbackStarted,    this, &MainWindow::onPlaybackStarted);
    connect(core, &MpvCore::playbackPaused,     this, &MainWindow::onPlaybackPaused);
    connect(core, &MpvCore::playbackEnded,      this, &MainWindow::onPlaybackEnded);
    connect(core, &MpvCore::playbackStopped,    this, &MainWindow::onPlaybackStopped);
    connect(core, &MpvCore::positionChanged,    this, &MainWindow::onPositionChanged);
    connect(core, &MpvCore::durationChanged,    this, &MainWindow::onDurationChanged);
    connect(core, &MpvCore::volumeChanged,      this, &MainWindow::onVolumeChanged);
    connect(core, &MpvCore::audioFormatChanged, this, &MainWindow::onAudioFormatChanged);
    connect(core, &MpvCore::videoInfoChanged,   this, &MainWindow::onVideoInfoChanged);
    connect(core, &MpvCore::tracksChanged,      this, &MainWindow::onTracksChanged);

    // ── SMTC (Windows 잠금 화면 미디어 컨트롤) 초기화 및 연결 ─────────────
    // ISystemMediaTransportControlsInterop::GetForWindow()로 Win32 HWND 기반 초기화
    // 재생/일시정지/이전/다음 버튼 → MPV 명령 연결
    smtcManager_ = new SMTCManager(this);
    // SMTC WinRT/COM 초기화는 첫 프레임 렌더링 뒤에 실행한다. 창이 보이기
    // 전에 시스템 미디어 세션을 만들면 저사양·노트북에서 실행이 무겁게 느껴진다.

    // ScrobbleManager 초기화 (Last.fm 스크로블링)
    scrobbleManager_ = new ScrobbleManager(this);

    // CloudDriveManager 초기화
    cloudDriveManager_ = new CloudDriveManager(this);
    connect(cloudDriveManager_, &CloudDriveManager::downloadUrlReady,
            this, [this](const QString& url, const QString& /*name*/) {
        openFiles({url});
    });
    QTimer::singleShot(1200, this, [this]() {
        if (smtcManager_->initialize(reinterpret_cast<void*>(winId()))) {
            qInfo() << "[MainWindow] SMTC 초기화 성공";
        } else {
            qWarning() << "[MainWindow] SMTC 초기화 실패 (Windows 10+ 필요)";
        }
    });
    // SMTC 버튼 → MPV 명령 연결
    connect(smtcManager_, &SMTCManager::playRequested,
            core, &MpvCore::play);
    connect(smtcManager_, &SMTCManager::pauseRequested,
            core, &MpvCore::pause);
    connect(smtcManager_, &SMTCManager::stopRequested,
            core, &MpvCore::stop);
    connect(smtcManager_, &SMTCManager::nextRequested,
            [core]() { core->command({"playlist-next"}); });
    connect(smtcManager_, &SMTCManager::previousRequested,
            [core]() { core->command({"playlist-prev"}); });
    // 재생 상태 → SMTC 업데이트
    connect(core, &MpvCore::playbackStarted, smtcManager_,
            [this]() { if (smtcManager_) smtcManager_->setPlaying(true); });
    connect(core, &MpvCore::playbackPaused, smtcManager_,
            [this]() { if (smtcManager_) smtcManager_->setPlaying(false); });
    connect(core, &MpvCore::playbackStopped, smtcManager_,
            [this]() { if (smtcManager_) smtcManager_->setStopped(); });
    // 재생 위치 → SMTC 타임라인 업데이트 (positionChanged는 초 단위)
    connect(core, &MpvCore::positionChanged, smtcManager_,
            [this](double pos) {
                if (smtcManager_) smtcManager_->updateTimeline(pos, totalDuration_);
            });

    // 실시간 렌더링 품질 강등/복원 시그널 → OSD로 사용자에게 알림
    // 프레임 드롭 감지 시 자동으로 deband/scale 강등하면 OSD로 표시
    connect(core, &MpvCore::renderQualityDegraded, this, [this](const QString& reason) {
        if (osdWidget_) osdWidget_->showInfo("⚠ 렌더링 자동 최적화: " + reason, 4000);
    });
    connect(core, &MpvCore::renderQualityRestored, this, [this]() {
        if (osdWidget_) osdWidget_->showInfo("✅ 렌더링 품질 복원됨", 3000);
    });

    controlBar_->connectMpv(core);  // AudioInfoBar 대체 - 인라인 오디오 정보
    trackSelector_->connectMpv(core);

    connect(controlBar_, &ControlBar::playPauseClicked,  core, &MpvCore::togglePause);
    connect(controlBar_, &ControlBar::stopClicked,       core, &MpvCore::stop);
    connect(controlBar_, &ControlBar::seeked,            [core](double p) { core->seek(p); });
    connect(controlBar_, &ControlBar::volumeChanged,     core, &MpvCore::setVolume);
    connect(controlBar_, &ControlBar::muteToggled,       core, &MpvCore::setMuted);
    connect(controlBar_, &ControlBar::openFileClicked,   this, &MainWindow::onOpenFile);
    connect(controlBar_, &ControlBar::prevClicked,       [core]() { core->command({"playlist-prev"}); });
    connect(controlBar_, &ControlBar::nextClicked,       [core]() { core->command({"playlist-next"}); });
    connect(controlBar_, &ControlBar::settingsClicked,   this, &MainWindow::onSettingsRequested);

    connect(titleBar_, &TitleBar::minimizeClicked,   this, &QMainWindow::showMinimized);
    connect(titleBar_, &TitleBar::maximizeClicked, this, [this]() {
        if (isMaximized()) {
            showNormal();
        } else {
            showMaximized();
        }
    });
    connect(titleBar_, &TitleBar::fullscreenClicked, this, &MainWindow::toggleFullscreen);
    connect(titleBar_, &TitleBar::closeClicked,      this, &QMainWindow::close);

    // 항상 위에 고정 토글
    connect(titleBar_, &TitleBar::alwaysOnTopToggled, this, [this](bool pinned) {
        Qt::WindowFlags flags = windowFlags();
        if (pinned) {
            flags |= Qt::WindowStaysOnTopHint;
        } else {
            flags &= ~Qt::WindowStaysOnTopHint;
        }
        setWindowFlags(flags);
        show();  // setWindowFlags 후 재표시 필요
        settings_.setValue("window/alwaysOnTop", pinned);
    });

    // 모드 전환 버튼 (ControlBar에 통합)
    connect(controlBar_, &ControlBar::playerModeClicked, this, &MainWindow::switchToPlayerMode);
    connect(controlBar_, &ControlBar::ottModeClicked,    this, &MainWindow::switchToOttMode);

    // OTT 타이틀 변경 시 윈도우 타이틀 업데이트
    // OTT titleChanged 연결은 switchToOttMode에서 처리

    // 음악 화면 연결은 ensureMusicPage()에서 첫 음악 재생 시 생성한다.
    // miniPlayer_, whisperWidget_, chapterWidget_ 연결은 지연 초기화
    // (toggleMiniPlayer, onProFeaturesRequested에서 처음 생성 시 연결)
}

void MainWindow::openFiles(const QStringList& paths) {
    QList<PlaybackQueue::Entry> entries;
    for (const QString& path : paths) {
        if (path.trimmed().isEmpty()) continue;
        PlaybackQueue::Entry entry;
        entry.url = path;
        entry.title = QFileInfo(path).completeBaseName();
        entry.source = path.startsWith("http") ? "stream" : "local";
        entries.append(entry);
    }
    playQueue(entries);
}

void MainWindow::playQueue(const QList<PlaybackQueue::Entry>& entries, int startIndex) {
    if (entries.isEmpty()) return;
    playbackQueue_->replace(entries, startIndex);
    applyQueueRepeatMode();

    const bool containsYouTube = std::any_of(entries.cbegin(), entries.cend(),
        [](const PlaybackQueue::Entry& entry) { return entry.source == QStringLiteral("youtube"); });
    if (containsYouTube && (!ytdlp_ || !ytdlp_->isAvailable())) {
        pendingYouTubeQueue_ = entries;
        pendingYouTubeQueueStartIndex_ = startIndex;
        if (ytdlp_ && !ytdlpProgress_) {
            ytdlpProgress_ = new QProgressDialog("YouTube 재생 준비 중...", "취소", 0, 100, this);
            ytdlpProgress_->setWindowTitle("yt-dlp 설치");
            ytdlpProgress_->setWindowModality(Qt::WindowModal);
            connect(ytdlpProgress_, &QProgressDialog::canceled, this, [this]() {
                pendingYouTubeQueue_.clear();
            });
            ytdlpProgress_->show();
            ytdlp_->downloadOrUpdate();
        }
        return;
    }
    if (containsYouTube && ytdlp_ && ytdlp_->isAvailable()) {
        const QString ytdlpDir = QFileInfo(ytdlp_->ytdlpPath()).absolutePath();
        mpvWidget_->core()->setProperty("ytdl-raw-options", QString("paths=home:%1").arg(ytdlpDir));
    }

    if (!mpvWidget_->isMpvInitialized()) {
        pendingStartupFiles_ = playbackQueue_->urls();
        qInfo() << "[MainWindow] playQueue: MPV 초기화 전 → 대기열 보관" << pendingStartupFiles_;
        return;
    }

    const PlaybackQueue::Entry current = playbackQueue_->currentEntry();
    if (current.url.isEmpty()) return;
    currentFilePath_ = current.url;
    if (HiFiEngine::isMusicFile(current.url)) {
        switchToMusicMode();
        hifiEngine_->applyAll();
    } else {
        switchToVideoMode();
    }

    const QList<PlaybackQueue::Entry> queue = playbackQueue_->entries();
    QTimer::singleShot(150, this, [this, queue, startIndex]() {
        if (!mpvWidget_->isMpvInitialized() || queue.isEmpty()) return;
        const int safeIndex = qBound(0, startIndex, queue.size() - 1);
        mpvWidget_->loadFile(queue.at(safeIndex).url);
        for (int index = safeIndex + 1; index < queue.size(); ++index)
            mpvWidget_->appendFile(queue.at(index).url);
        // 선택 항목 앞의 곡도 동일한 순환 대기열에 포함한다.
        for (int index = 0; index < safeIndex; ++index)
            mpvWidget_->appendFile(queue.at(index).url);
    });
}

void MainWindow::applyQueueRepeatMode() {
    if (!playbackQueue_ || !mpvWidget_ || !mpvWidget_->isMpvInitialized()) return;
    auto* core = mpvWidget_->core();
    switch (playbackQueue_->repeatMode()) {
    case PlaybackQueue::RepeatMode::One:
        core->setProperty("loop-file", "inf");
        core->setProperty("loop-playlist", "no");
        break;
    case PlaybackQueue::RepeatMode::All:
        core->setProperty("loop-file", "no");
        core->setProperty("loop-playlist", "inf");
        break;
    case PlaybackQueue::RepeatMode::Off:
        core->setProperty("loop-file", "no");
        core->setProperty("loop-playlist", "no");
        break;
    }
}

void MainWindow::switchToMusicMode() {
    ensureMusicPage();
    isMusicMode_ = true;
    playerStack_->setCurrentWidget(musicPage_);
    controlBar_->hide();
    // 음악 모드: 타이틀바는 항상 표시 (상단고정/최소화/종료 버튼 보여야 함)
    titleBar_->show();
    // uiVisible_ 재설정: 타이틀바가 보이는 상태로 시작
    uiVisible_ = true;
    // 커서 복원: 이전 모드에서 BlankCursor가 남아있으면 제거
    if (cursor().shape() == Qt::BlankCursor) unsetCursor();
    if (musicPage_) musicPage_->unsetCursor();
    // 음악 모드: 걪리스 재생 항상 활성화 (앨범 연속 재생 시 공백 제거)
    mpvWidget_->core()->setProperty("gapless-audio", QString("yes"));
    // 스펙트럼은 실제 재생 중인 음악 화면에서만 활성화한다. 화면 전환·로딩 중
    // 60fps 폴링과 피크 repaint가 계속되지 않도록 재생 이벤트가 제어한다.
    if (musicPage_) musicPage_->setVisualizationActive(isPlaying_);
    mpvWidget_->core()->setSpectrumEnabled(isPlaying_);
    connect(mpvWidget_->core(), &MpvCore::spectrumReady,
            musicPage_, &MusicWidget::updateSpectrum,
            Qt::UniqueConnection);
}

void MainWindow::switchToVideoMode() {
    isMusicMode_ = false;
    playerStack_->setCurrentIndex(0);
    controlBar_->show();
    // 스펙트럼 비활성화 (영상 모드에서는 불필요)
    mpvWidget_->core()->setSpectrumEnabled(false);
    if (musicPage_) {
        musicPage_->setVisualizationActive(false);
        disconnect(mpvWidget_->core(), &MpvCore::spectrumReady,
                   musicPage_, &MusicWidget::updateSpectrum);
    }
}

void MainWindow::loadMusicMeta(const QString& path) {
    ensureMusicPage();
    auto* core = mpvWidget_->core();
    MusicMeta meta;
    meta.codec      = core->getProperty("audio-codec-name").toString();
    meta.sampleRate = core->getProperty("audio-params/samplerate").toInt();
    // 비트낮이 실측: 샘플 포맷 문자열에서 추출 (s16=16bit, s32=32bit, float=32bit)
    {
        const QString fmt = core->getProperty("audio-params/format").toString();
        if (fmt.contains("16"))                              meta.bitDepth = 16;
        else if (fmt.contains("24"))                         meta.bitDepth = 24;
        else if (fmt.contains("32") || fmt.contains("float")) meta.bitDepth = 32;
        else                                                 meta.bitDepth = 16;
    }
    meta.channels   = core->getProperty("audio-params/channel-count").toInt();
    meta.title      = core->getProperty("media-title").toString();
    meta.artist     = core->getProperty("metadata/by-key/artist").toString();
    meta.album      = core->getProperty("metadata/by-key/album").toString();
    meta.year       = core->getProperty("metadata/by-key/date").toString();
    meta.filePath   = path;  // LRC 가사 탐색용
    // AI 가사 검색에 필요한 duration (LRCLIB /api/get 정확 매칭용)
    // totalDuration_이 아직 0이면 MPV에서 직접 읽음
    double dur = totalDuration_ > 0 ? totalDuration_
                                    : core->getProperty("duration").toDouble();
    if (dur > 0) totalDuration_ = dur;
    // 1) 파일 내장 앨범 아트 추출 (ID3v2 APIC / FLAC PICTURE / MP4 covr)
    meta.albumArt = AlbumArtExtractor::extract(path);
    // 2) 없으면 폴더 내 커버 이미지 탐색
    if (meta.albumArt.isNull()) {
        QFileInfo fi(path);
        QDir dir = fi.dir();
        for (const QString& name : {"cover.jpg", "cover.png", "folder.jpg",
                                     "albumart.jpg", "front.jpg"}) {
            if (dir.exists(name)) { meta.albumArt = QPixmap(dir.filePath(name)); break; }
        }
    }
        // MusicWidget에서 loadForTrack에 duration을 전달하기 위해
    // MusicWidget::loadMeta 호출 전에 duration_을 설정
    musicPage_->updatePosition(0, dur);  // duration 먼저 설정
    musicPage_->loadMeta(meta);
}
void MainWindow::onMusicSeekRequested(double pos) {
    mpvWidget_->core()->seek(pos);
}

void MainWindow::onMusicVolumeChanged(int vol) {
    mpvWidget_->core()->setVolume(vol);
}

void MainWindow::onFileLoaded(const QString& path) {
    // 이전 파일의 재생 위치 저장 (파일 전환 시)
    if (!currentFilePath_.isEmpty() && currentFilePath_ != path)
        saveResumePosition();
    currentFilePath_ = path;
    if (playbackQueue_)
        playbackQueue_->setCurrentUrl(path);
    lastPosition_ = 0.0;
    updateWindowTitle(QFileInfo(path).fileName());
    addToRecentFiles(path);
    tryResumePosition(path);

    // 재생 통계 업데이트 (재생 횟수 + 마지막 재생 시각)
    if (!path.startsWith("http")) {
        QString statsKey = "stats/" + QString::fromUtf8(
            QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Md5).toHex());
        int playCount = settings_.value(statsKey + "/count", 0).toInt();
        settings_.setValue(statsKey + "/count", playCount + 1);
        settings_.setValue(statsKey + "/last_played",
            QDateTime::currentDateTime().toString(Qt::ISODate));
        settings_.setValue(statsKey + "/path", path);
    }
    // ORIGINALS 탭 재생 중 표시 갱신
    if (originalsWidget_)
        originalsWidget_->setCurrentFile(path);

    // ── SMTC 메타데이터 업데이트 ─────────────────────────────────────────────────────────────────────
    // 재생 시작 시 Windows 잠금 화면/알림 센터에 제목·아티스트·앉범아트 표시
    // 200ms 지연: MPV가 메타데이터를 완전히 로드한 후 읽음
    if (smtcManager_ && smtcManager_->isEnabled()) {
        QTimer::singleShot(200, this, [this, path]() {
            auto* core = mpvWidget_->core();
            QString title  = core->getProperty("media-title").toString();
            QString artist = core->getProperty("metadata/by-key/artist").toString();
            QString album  = core->getProperty("metadata/by-key/album").toString();
            if (title.isEmpty()) title = QFileInfo(path).completeBaseName();
            // 앉범아트: 음악 모드에서는 MusicWidget의 상태를 활용
            QPixmap art = AlbumArtExtractor::extract(path);
            smtcManager_->updateMetadata(title, artist, album, art);
            smtcManager_->setPlaying(true);
        });
    }

    if (isMusicMode_) {
        // 음악 모드: 메타데이터 로드 (파일 로드 후 MPV가 태그를 읽은 시점)
        QTimer::singleShot(200, this, [this, path]() { loadMusicMeta(path); });
        musicPage_->setPlaying(true);
    } else {
        controlBar_->setPlaying(true);
    }
    // 챕터 위젯이 열려 있으면 새 파일의 챕터 로드
    if (chapterWidget_) {
        chapterWidget_->setDuration(totalDuration_);
        chapterWidget_->loadChapters();
        chapterWidget_->generateThumbnails(path);
    }
    // 타임라인에 챕터 마커 표시 (200ms 지연 - MPV가 챕터 정보 로드하는 시점 고려)
    QTimer::singleShot(300, this, [this]() {
        auto* core = mpvWidget_->core();
        QVariant chList = core->getProperty("chapter-list");
        double dur = core->getProperty("duration").toDouble();
        if (chList.isValid() && dur > 0) {
            QVector<ChapterMark> marks;
            const auto& list = chList.toList();
            for (const auto& item : list) {
                const auto& map = item.toMap();
                double t = map.value("time").toDouble();
                QString title = map.value("title").toString();
                if (title.isEmpty()) title = QString("챕터 %1").arg(marks.size() + 1);
                marks.append({t, title});
            }
            if (!marks.isEmpty())
                controlBar_->setChapters(marks, dur);
        }
    });
}

namespace {
constexpr int TOP_UI_REVEAL_ZONE = 56;
constexpr int BOTTOM_UI_REVEAL_ZONE = 96;
constexpr int UI_AUTO_HIDE_DELAY_MS = 3000;
}

void MainWindow::showTopUi() {
    if (!titleBar_) return;
    titleBar_->show();
    uiVisible_ = true;

    // 비디오 가장자리에서 UI를 다시 호출할 수 있도록 커서를 복원한다.
    if (cursor().shape() == Qt::BlankCursor) unsetCursor();
    if (mpvWidget_) mpvWidget_->unsetCursor();
    if (musicPage_) musicPage_->unsetCursor();
    if (isPlaying_ && !isMusicMode_ && uiHideTimer_)
        uiHideTimer_->start(UI_AUTO_HIDE_DELAY_MS);
}

void MainWindow::showBottomUi() {
    // 음악 모드는 MusicWidget의 자체 컨트롤을 사용하므로 별도 하단바를 표시하지 않는다.
    if (isMusicMode_ || !controlBar_) return;
    controlBar_->show();
    uiVisible_ = true;

    if (cursor().shape() == Qt::BlankCursor) unsetCursor();
    if (mpvWidget_) mpvWidget_->unsetCursor();
    if (isPlaying_ && uiHideTimer_)
        uiHideTimer_->start(UI_AUTO_HIDE_DELAY_MS);
}

void MainWindow::showUI() {
    // 명시적 동작(일시정지, 팝업, 우클릭, 설정 복귀)에서는 두 영역을 함께 표시한다.
    // 단순 마우스 이동은 revealUiForVideoEdge()만 호출하므로 중앙 영상 영역에서 UI가 나타나지 않는다.
    if (isMusicMode_) {
        showTopUi();
        return;
    }
    showTopUi();
    showBottomUi();
}

void MainWindow::revealUiForVideoEdge(const QPoint& videoPosition) {
    if (isMusicMode_ || !isPlaying_ || !mpvWidget_) return;

    // 중앙 영상 영역의 단순 마우스 이동은 어떠한 UI도 표시하지 않는다.
    if (videoPosition.y() <= TOP_UI_REVEAL_ZONE) {
        // 가장자리 동작은 해당 영역만 표시한다. 중앙 화면과 반대쪽 UI는 가리지 않는다.
        if (controlBar_) controlBar_->hide();
        showTopUi();
    } else if (videoPosition.y() >= mpvWidget_->height() - BOTTOM_UI_REVEAL_ZONE) {
        if (titleBar_) titleBar_->hide();
        showBottomUi();
    }
}

void MainWindow::hideUI() {
    // 음악 모드에서는 타이틀바/커서를 절대 숨기지 않음
    if (isMusicMode_) return;
    // ── 팝업/메뉴가 열려 있으면 절대 숨기지 않음 ──────────────────────────
    // QApplication 레벨 팝업(QMenu, QComboBox 드롭다운 등)이 열려 있으면 숨기지 않음
    if (QApplication::activePopupWidget() != nullptr) {
        if (uiHideTimer_) uiHideTimer_->start(1000);
        return;
    }
    // 전문 기능 패널이 실제로 열려 있으면 UI/커서를 숨기지 않음.
    if (proFeatures_ && proFeatures_->isVisible()) {
        if (uiHideTimer_) uiHideTimer_->start(UI_AUTO_HIDE_DELAY_MS);
        return;
    }
    if (uiVisible_ && isPlaying_) {
        // 전체 화면·창 모드 모두 노출 중인 상단·하단 UI만 숨긴다.
        // 레이아웃을 변경하지 않는 hide()만 사용하므로 영상 프레임과 오버레이는 유지된다.
        if (titleBar_) titleBar_->hide();
        if (controlBar_) controlBar_->hide();
        uiVisible_ = false;
        setCursor(Qt::BlankCursor);
        if (mpvWidget_) mpvWidget_->setCursor(Qt::BlankCursor);
    }
}

void MainWindow::onPlaybackStarted() {
    if (!isMusicMode_) controlBar_->setPlaying(true);
    if (isMusicMode_ && musicPage_) {
        musicPage_->setVisualizationActive(true);
        mpvWidget_->core()->setSpectrumEnabled(true);
    }
    // UI 자동 숨김 타이머 설정 (영상 모드에서만 사용)
    if (!uiHideTimer_) {
        uiHideTimer_ = new QTimer(this);
        uiHideTimer_->setSingleShot(true);
        connect(uiHideTimer_, &QTimer::timeout, this, &MainWindow::hideUI);
    }
    isPlaying_ = true;
    showUI();
    // 자동 숨김 타이머: 영상 모드에서만 시작
    if (!isMusicMode_) uiHideTimer_->start(3000);
#ifdef Q_OS_WIN
    SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED);
#endif
}
void MainWindow::onPlaybackPaused() {
    isPlaying_ = false;
    if (isMusicMode_ && musicPage_) {
        musicPage_->setVisualizationActive(false);
        mpvWidget_->core()->setSpectrumEnabled(false);
    }
    controlBar_->setPlaying(false);
    // 일시정지 시 UI 항상 표시
    if (uiHideTimer_) uiHideTimer_->stop();
    showUI();
    updateTaskbarProgress(lastPosition_, totalDuration_, true, false);
#ifdef Q_OS_WIN
    SetThreadExecutionState(ES_CONTINUOUS);
#endif
}
void MainWindow::onPlaybackEnded() {
    if (isMusicMode_ && musicPage_) {
        musicPage_->setVisualizationActive(false);
        mpvWidget_->core()->setSpectrumEnabled(false);
    }
    // MPV 대기열의 곡 사이 EOF는 즉시 다음 항목으로 전환된다. 이 경우에는
    // 로고·정지 상태를 표시하지 않고, 실제 idle 상태일 때만 종료 UI를 갱신한다.
    if (playbackQueue_ && !playbackQueue_->isEmpty()) {
        QTimer::singleShot(80, this, [this]() {
            if (!mpvWidget_->core()->getProperty("idle-active").toBool()) return;
            isPlaying_ = false;
            clearResumePosition(currentFilePath_);
            if (uiHideTimer_) uiHideTimer_->stop();
            showUI();
            if (!isMusicMode_) {
                controlBar_->setPlaying(false);
                mpvWidget_->showLogo(true);
            } else {
                musicPage_->setPlaying(false);
            }
#ifdef Q_OS_WIN
            SetThreadExecutionState(ES_CONTINUOUS);
#endif
        });
        return;
    }
    isPlaying_ = false;
    clearResumePosition(currentFilePath_);
    if (uiHideTimer_) uiHideTimer_->stop();
    showUI();
    if (!isMusicMode_) {
        controlBar_->setPlaying(false);
        mpvWidget_->showLogo(true);
    } else {
        musicPage_->setPlaying(false);
    }
#ifdef Q_OS_WIN
    SetThreadExecutionState(ES_CONTINUOUS);
#endif
}
void MainWindow::onPlaybackStopped() {
    isPlaying_ = false;
    if (isMusicMode_ && musicPage_) {
        musicPage_->setVisualizationActive(false);
        mpvWidget_->core()->setSpectrumEnabled(false);
    }
    if (uiHideTimer_) uiHideTimer_->stop();
    showUI();
    if (!isMusicMode_) {
        controlBar_->setPlaying(false);
        mpvWidget_->showLogo(true);
    } else {
        musicPage_->setPlaying(false);
    }
    updateWindowTitle();
    updateTaskbarProgress(0, 0, false, true);
#ifdef Q_OS_WIN
    SetThreadExecutionState(ES_CONTINUOUS);
#endif
}
void MainWindow::onPositionChanged(double s) {
    controlBar_->setPosition(s, totalDuration_);
    lastPosition_ = s;  // 이어보기용 현재 위치 추적
    updateTaskbarProgress(s, totalDuration_, false, false);
}

#ifdef Q_OS_WIN
#include <shobjidl.h>
#endif

void MainWindow::initTaskbarList() {
#ifdef Q_OS_WIN
    if (taskbarList_) return;
    ITaskbarList3* tbl = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_ITaskbarList3, (void**)&tbl);
    if (SUCCEEDED(hr) && tbl) {
        tbl->HrInit();
        taskbarList_ = tbl;
    }
#endif
}

void MainWindow::updateTaskbarProgress(double pos, double dur, bool paused, bool stopped) {
#ifdef Q_OS_WIN
    if (!taskbarList_) initTaskbarList();
    if (!taskbarList_) return;
    ITaskbarList3* tbl = static_cast<ITaskbarList3*>(taskbarList_);
    HWND hwnd = (HWND)winId();
    if (stopped || dur <= 0) {
        tbl->SetProgressState(hwnd, TBPF_NOPROGRESS);
        return;
    }
    if (paused) {
        tbl->SetProgressState(hwnd, TBPF_PAUSED);
        tbl->SetProgressValue(hwnd, (ULONGLONG)(pos * 1000), (ULONGLONG)(dur * 1000));
    } else {
        tbl->SetProgressState(hwnd, TBPF_NORMAL);
        tbl->SetProgressValue(hwnd, (ULONGLONG)(pos * 1000), (ULONGLONG)(dur * 1000));
    }
#else
    Q_UNUSED(pos); Q_UNUSED(dur); Q_UNUSED(paused); Q_UNUSED(stopped);
#endif
}

// ─── 이어보기 (재생 위치 저장/복원) ──────────────────────────────
static QString resumeKeyFor(const QString& path) {
    // 파일 경로 해시로 키 생성 (경로에 특수문자 있어도 안전)
    return QStringLiteral("resume/%1").arg(
        QString::number(qHash(path), 16));
}

void MainWindow::saveResumePosition() {
    if (currentFilePath_.isEmpty() || lastPosition_ < 5.0) return;
    if (currentFilePath_.startsWith("http")) return;  // 스트리밍 제외
    if (!settings_.value("general/remember_pos", true).toBool()) return;
    // 95% 이상 재생했으면 다 본 것으로 간주 → 저장 안 함
    if (totalDuration_ > 0 && lastPosition_ / totalDuration_ > 0.95) {
        clearResumePosition(currentFilePath_);
        return;
    }
    settings_.setValue(resumeKeyFor(currentFilePath_), lastPosition_);
}

void MainWindow::clearResumePosition(const QString& path) {
    if (path.isEmpty()) return;
    settings_.remove(resumeKeyFor(path));
}

void MainWindow::tryResumePosition(const QString& path) {
    if (!settings_.value("general/remember_pos", true).toBool()) return;
    const double saved = settings_.value(resumeKeyFor(path), 0.0).toDouble();
    if (saved > 5.0) {
        mpvWidget_->core()->seek(saved);
        // OSD로 이어보기 안내 (팝업 없이)
        const int m = int(saved) / 60, s = int(saved) % 60;
        mpvWidget_->core()->command({"show-text",
            QString("이어보기: %1:%2부터 재생").arg(m).arg(s, 2, 10, QChar('0')), "3000"});
    }
}

// ─── 최근 파일 목록 ──────────────────────────────────────────────
void MainWindow::addToRecentFiles(const QString& path) {
    if (path.startsWith("http")) return;
    QStringList recent = settings_.value("recent/files").toStringList();
    recent.removeAll(path);
    recent.prepend(path);
    while (recent.size() > 10) recent.removeLast();
    settings_.setValue("recent/files", recent);
}
void MainWindow::onDurationChanged(double s) { totalDuration_ = s; controlBar_->setDuration(s); }
void MainWindow::onVolumeChanged(int v) {
    controlBar_->setVolume(v);
    // OSD 볼륨 표시
    if (osdWidget_) {
        bool muted = mpvWidget_->core()->getProperty("mute").toBool();
        osdWidget_->showVolume(v, muted);
    }
}

void MainWindow::onAudioFormatChanged(const QString& codec, int, int, const QString&) {
    // 헤더는 원본 코덱만 간결하게 표시하고, 실제 HDMI 출력은 하단 컨트롤바에 PCM/비트스트림
    // 레이아웃으로 표시한다. 수동 설정을 요구하는 팝업은 열지 않는다.
    titleBar_->setAudioBadge(codec.toUpper());
}

void MainWindow::onVideoInfoChanged(int, int, double, const QString&) {}
void MainWindow::onTracksChanged() { trackSelector_->refresh(); }

void MainWindow::onOpenFile() {
    QStringList paths = QFileDialog::getOpenFileNames(
        this, "파일 열기", {},
        "미디어 파일 (*.mkv *.mp4 *.avi *.mov *.wmv *.flv *.ts *.m2ts "
        "*.m4v *.webm *.mp3 *.flac *.aac *.wav *.dts *.ac3 *.truehd);;"
        "모든 파일 (*.*)");
    if (!paths.isEmpty()) openFiles(paths);
}

// 최근 파일 메뉴 표시 (컨트롤바 파일 열기 버튼 우클릭 또는 단축키)
void MainWindow::showRecentFilesMenu() {
    const QStringList recent = settings_.value("recent/files").toStringList();
    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background:#1a1a1a; color:#e0e0e0; border:1px solid #333; }"
        "QMenu::item { padding:6px 24px; }"
        "QMenu::item:selected { background:#1a3a5c; color:#4fc3f7; }");
    if (recent.isEmpty()) {
        menu.addAction("최근 파일 없음")->setEnabled(false);
    } else {
        for (const QString& p : recent) {
            QFileInfo fi(p);
            QAction* act = menu.addAction(fi.fileName());
            act->setToolTip(p);
            connect(act, &QAction::triggered, this, [this, p]() {
                if (QFileInfo::exists(p)) openFiles({p});
            });
        }
        menu.addSeparator();
        QAction* clearAct = menu.addAction("목록 지우기");
        connect(clearAct, &QAction::triggered, this, [this]() {
            settings_.remove("recent/files");
        });
    }
    menu.exec(QCursor::pos());
}

void MainWindow::onSettingsRequested() {
    SettingsDialog dlg(mpvWidget_->core(), this);
    dlg.exec();
}

void MainWindow::onSubtitleSearch() {
    if (currentFilePath_.isEmpty() || currentFilePath_.startsWith("http")) return;
    SubtitleSearchDialog dlg(currentFilePath_, this);
    if (dlg.exec() == QDialog::Accepted && !dlg.downloadedPath().isEmpty()) {
        // 다운로드된 자막을 MPV에 로드
        mpvWidget_->core()->command({"sub-add", dlg.downloadedPath(), "select"});
        // OSD로 안내
        mpvWidget_->core()->command({"show-text",
            QString("자막 로드: %1").arg(QFileInfo(dlg.downloadedPath()).fileName()),
            "3000"});
    }
}

void MainWindow::toggleFullscreen() {
    if (isFullscreen_) {
        showNormal();
        titleBar_->show();
        titleBar_->setFullscreenMode(false);
        isFullscreen_ = false;
    } else {
        showFullScreen();
        titleBar_->hide();
        isFullscreen_ = true;
        titleBar_->setFullscreenMode(true);
    }
}

void MainWindow::closeEvent(QCloseEvent* e)  {
    saveResumePosition();  // 이어보기: 종료 시 현재 위치 저장
    saveSettings();
    e->accept();
}

void MainWindow::dragEnterEvent(QDragEnterEvent* e) {
    if (e->mimeData()->hasUrls()) e->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* e) {
    QStringList paths;
    for (const QUrl& url : e->mimeData()->urls())
        if (url.isLocalFile()) paths << url.toLocalFile();
    if (!paths.isEmpty()) openFiles(paths);
}

void MainWindow::keyPressEvent(QKeyEvent* e) {
    auto* core = mpvWidget_->core();

    // ── 단축키 커스터마이징 실시간 적용 ──────────────────────────────────────
    // ShortcutConfigWidget에 저장된 커스텀 단축키가 있으면 우선 처리
    if (shortcutConfigWidget_) {
        QKeySequence pressed(static_cast<int>(e->modifiers()) | e->key());
        // 커스텀 단축키 매핑 처리
        const auto& entries = shortcutConfigWidget_->entries();
        for (const auto& entry : entries) {
            QKeySequence activeKey = entry.customKey.isEmpty() ? entry.defaultKey : entry.customKey;
            if (activeKey == pressed) {
                // 커스텀 단축키가 기본 단축키와 다른 경우에만 여기서 처리
                if (!entry.customKey.isEmpty() && entry.customKey != entry.defaultKey) {
                    // 커스텀 단축키로 매핑된 기능 실행
                    if      (entry.id == "play_pause")   core->togglePause();
                    else if (entry.id == "seek_back5")   { core->seek(-5, true); if (osdWidget_) osdWidget_->showSeek(core->position(), totalDuration_); }
                    else if (entry.id == "seek_fwd5")    { core->seek(5, true);  if (osdWidget_) osdWidget_->showSeek(core->position(), totalDuration_); }
                    else if (entry.id == "seek_back60")  { core->seek(-60, true); if (osdWidget_) osdWidget_->showSeek(core->position(), totalDuration_); }
                    else if (entry.id == "seek_fwd60")   { core->seek(60, true);  if (osdWidget_) osdWidget_->showSeek(core->position(), totalDuration_); }
                    else if (entry.id == "vol_up")       { core->setVolume(qMin(core->volume()+5, 200)); if (osdWidget_) osdWidget_->showVolume(core->volume(), false); }
                    else if (entry.id == "vol_down")     { core->setVolume(qMax(core->volume()-5, 0));   if (osdWidget_) osdWidget_->showVolume(core->volume(), false); }
                    else if (entry.id == "fullscreen")   toggleFullscreen();
                    else if (entry.id == "mute")         core->setMuted(!core->getProperty("mute").toBool());
                    else if (entry.id == "pro_panel")    toggleProFeatures();
                    else if (entry.id == "screenshot")   { core->command({"screenshot", "video"}); if (osdWidget_) osdWidget_->showInfo("화면 캡처 저장"); }
                    else if (entry.id == "open_file")    onOpenFile();
                    else if (entry.id == "open_url")     onOpenUrl();
                    else if (entry.id == "settings")     onSettingsRequested();
                    else if (entry.id == "shortcut_help") shortcutOverlay_->toggle();
                    else if (entry.id == "playlist_prev") core->command({"playlist-prev"});
                    else if (entry.id == "playlist_next") core->command({"playlist-next"});
                    else if (entry.id == "speed_up")     { core->command({"add", "speed", "0.1"}); if (osdWidget_) osdWidget_->showSpeed(core->getProperty("speed").toDouble()); }
                    else if (entry.id == "speed_down")   { core->command({"add", "speed", "-0.1"}); if (osdWidget_) osdWidget_->showSpeed(core->getProperty("speed").toDouble()); }
                    else if (entry.id == "screen_record") { if (screenRecorder_) screenRecorder_->toggleRecording(); }
                    e->accept();
                    return;
                }
            }
        }
    }

    const int key = e->key();
    if (key == Qt::Key_A || key == Qt::Key_B || key == Qt::Key_D ||
        key == Qt::Key_Z || key == Qt::Key_Greater || key == Qt::Key_Less)
        ensureProFeatures();

    switch (key) {
    case Qt::Key_Space:  core->togglePause(); break;
    case Qt::Key_Return:
    case Qt::Key_Enter:  core->togglePause(); break;  // Enter로 재생/일시정지
    case Qt::Key_F:
    case Qt::Key_F11:    toggleFullscreen(); break;
    case Qt::Key_Escape: if (isFullscreen_) toggleFullscreen(); break;
    // 이동: 일반 5초, Shift 60초, Ctrl 10초
    case Qt::Key_Left:
        // 좌우 방향키: 일반 5초, Shift 60초, Ctrl 10초
        if (e->modifiers() & Qt::ShiftModifier)       core->seek(-60, true);
        else if (e->modifiers() & Qt::ControlModifier) core->seek(-10, true);
        else                                           core->seek(-5, true);
        if (osdWidget_) osdWidget_->showSeek(core->position(), totalDuration_);
        break;
    case Qt::Key_Right:
        if (e->modifiers() & Qt::ShiftModifier)       core->seek(60, true);
        else if (e->modifiers() & Qt::ControlModifier) core->seek(10, true);
        else                                           core->seek(5, true);
        if (osdWidget_) osdWidget_->showSeek(core->position(), totalDuration_);
        break;
    case Qt::Key_Up:
        core->setVolume(qMin(core->volume() + 5, 200));
        if (osdWidget_) osdWidget_->showVolume(core->volume(), false);
        break;
    case Qt::Key_Down:
        core->setVolume(qMax(core->volume() - 5, 0));
        if (osdWidget_) osdWidget_->showVolume(core->volume(), false);
        break;
    case Qt::Key_PageUp:   core->seek(-300, true); break;  // 5분 앞으로
    case Qt::Key_PageDown: core->seek(300, true);  break;  // 5분 뒤로
    case Qt::Key_Home:     core->seek(0); break;           // 첫 장면으로
    case Qt::Key_End:      core->seek(100, false); break;  // 마지막으로
    case Qt::Key_M:        core->setMuted(!core->getProperty("mute").toBool()); break;
    // 재생목록 이전/다음
    case Qt::Key_N:        core->command({"playlist-next"}); break;
    case Qt::Key_BracketLeft:   // [ 이전 챕터
        core->command({"add", "chapter", "-1"});
        break;
    case Qt::Key_BracketRight:  // ] 다음 챕터
        core->command({"add", "chapter", "1"});
        break;
    case Qt::Key_O:
        if (e->modifiers() & Qt::ControlModifier) onOpenFile();
        break;
    case Qt::Key_I:  // 재생 정보 표시 (show-progress)
        core->command({"show-progress"});
        break;
    case Qt::Key_T:  // 항상 위에 토글
        {
            bool top = windowFlags() & Qt::WindowStaysOnTopHint;
            if (top) setWindowFlags(windowFlags() & ~Qt::WindowStaysOnTopHint);
            else     setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
            show();
            if (osdWidget_) osdWidget_->showInfo(top ? "항상 위에: 해제" : "항상 위에: 활성화");
        }
        break;
    case Qt::Key_W:  // 자막 폰트 크기 증가
        core->command({"add", "sub-scale", "0.1"});
        break;
    case Qt::Key_E:
        // Ctrl+Shift+E: WASAPI 독점/공유 모드 즉시 전환
        if ((e->modifiers() & Qt::ControlModifier) && (e->modifiers() & Qt::ShiftModifier)) {
            if (core) {
                const bool currentExclusive =
                    core->getProperty("audio-exclusive").toString() == "yes";
                const bool newExclusive = !currentExclusive;
                core->setAudioExclusive(newExclusive);
                QSettings s("Sorinuri", "SorinuriPlayer");
                s.setValue("audio/exclusive", newExclusive);
                if (osdWidget_)
                    osdWidget_->showInfo(newExclusive
                        ? "WASAPI 독점 모드 (원본 음질)"
                        : "WASAPI 공유 모드 (다중 앱 동시 재생)");
                qInfo() << "[MainWindow] Ctrl+Shift+E → WASAPI"
                        << (newExclusive ? "독점" : "공유") << "모드 전환";
            }
            e->accept();
            return;
        }
        // E만 누른 경우: 자막 폰트 크기 감소
        if (core) core->command({"add", "sub-scale", "-0.1"});
        break;
    case Qt::Key_Plus:   // 재생 속도 증가
    case Qt::Key_Equal:
        core->command({"add", "speed", "0.1"});
        if (osdWidget_) osdWidget_->showSpeed(core->getProperty("speed").toDouble());
        break;
    case Qt::Key_Minus:  // 재생 속도 감소
        core->command({"add", "speed", "-0.1"});
        if (osdWidget_) osdWidget_->showSpeed(core->getProperty("speed").toDouble());
        break;
    case Qt::Key_0:      // 재생 속도 원래대로
        core->command({"set", "speed", "1.0"});
        if (osdWidget_) osdWidget_->showSpeed(1.0);
        break;
    case Qt::Key_V:  // 자막 트랙 전환
        core->command({"cycle", "sub"});
        break;
    case Qt::Key_J:  // 자막 다음
        core->command({"sub-step", "1"});
        break;
    case Qt::Key_K:  // 자막 이전
        core->command({"sub-step", "-1"});
        break;
    case Qt::Key_R:  // 반복 모드 토글
        {
            QString loop = core->getProperty("loop-file").toString();
            if (loop == "inf") {
                core->command({"set", "loop-file", "no"});
                if (osdWidget_) osdWidget_->showInfo("반복: 해제");
            } else {
                core->command({"set", "loop-file", "inf"});
                if (osdWidget_) osdWidget_->showInfo("반복: 활성화");
            }
        }
        break;
    case Qt::Key_X:  // 셔플 토글
        {
            QString shuffle = core->getProperty("shuffle").toString();
            if (shuffle == "yes") {
                core->command({"set", "shuffle", "no"});
                if (osdWidget_) osdWidget_->showInfo("셔플: 해제");
            } else {
                core->command({"set", "shuffle", "yes"});
                if (osdWidget_) osdWidget_->showInfo("셔플: 활성화");
            }
        }
        break;
    // ── 전문 기능 단축키 ───────────────────────────────────────────────
    case Qt::Key_A:
        if (e->modifiers() & Qt::ControlModifier)
            proFeatures_->clearAbLoop();
        else
            proFeatures_->setAbPointA();
        break;
    case Qt::Key_B:
        proFeatures_->setAbPointB();
        break;
    case Qt::Key_S:  // 스크린샷
        if (e->modifiers() & Qt::ControlModifier) {
            {
            QString dir = settings_.value("general/screenshot_dir", QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)).toString();
            QString fmt = settings_.value("general/screenshot_format", "PNG").toString().toLower();
            core->setProperty("screenshot-dir", dir);
            core->setProperty("screenshot-format", fmt);
            core->setProperty("screenshot-template", "Sorinuri_%F_%p_%n");
            core->command({"screenshot", "video"});
        }
            if (osdWidget_) osdWidget_->showInfo("화면 캡처 저장");
        } else {
            core->command({"stop"});
        }
        break;
    case Qt::Key_D:  // 오디오 딜레이
        if (e->modifiers() & Qt::ShiftModifier)
            proFeatures_->setAudioDelay(proFeatures_->audioDelay() - 100);
        else
            proFeatures_->setAudioDelay(proFeatures_->audioDelay() + 100);
        break;
    case Qt::Key_Z:  // 자막 딜레이
        if (e->modifiers() & Qt::ShiftModifier)
            proFeatures_->setSubDelay(proFeatures_->subDelay() - 100);
        else
            proFeatures_->setSubDelay(proFeatures_->subDelay() + 100);
        break;
    case Qt::Key_Greater:  // > 재생속도 +0.25x
        proFeatures_->setSpeed(qMin(proFeatures_->currentSpeed() + 0.25, 4.0));
        if (osdWidget_) osdWidget_->showSpeed(proFeatures_->currentSpeed());
        break;
    case Qt::Key_Less:     // < 재생속도 -0.25x
        proFeatures_->setSpeed(qMax(proFeatures_->currentSpeed() - 0.25, 0.25));
        if (osdWidget_) osdWidget_->showSpeed(proFeatures_->currentSpeed());
        break;
    case Qt::Key_C:  // 화면 캡처
        {
            QString dir = settings_.value("general/screenshot_dir", QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)).toString();
            QString fmt = settings_.value("general/screenshot_format", "PNG").toString().toLower();
            core->setProperty("screenshot-dir", dir);
            core->setProperty("screenshot-format", fmt);
            core->setProperty("screenshot-template", "Sorinuri_%F_%p_%n");
            core->command({"screenshot", "video"});
        }
        if (osdWidget_) osdWidget_->showInfo("화면 캡처 저장");
        break;
    case Qt::Key_P:
        toggleProFeatures();
        break;
    case Qt::Key_Question:
        shortcutOverlay_->toggle();
        break;
    default: QMainWindow::keyPressEvent(e); break;
    }
}

void MainWindow::toggleProFeatures() {
    ensureProFeatures();
    // 상태 플래그를 추정으로 토글하지 않는다. 사이드 탭·닫기 버튼·단축키가
    // 섞여도 실제 위젯의 표시 상태를 단일 기준으로 사용한다.
    isProFeaturesOpen_ = !proFeatures_->isVisible();
    if (isProFeaturesOpen_) {
        showUI();
        if (uiHideTimer_) uiHideTimer_->stop();
    }

    // 처음 열 때 지연 초기화 위젯 생성 및 탭 추가
    if (isProFeaturesOpen_) {
        auto* core = mpvWidget_->core();

        if (!whisperWidget_) {
            whisperWidget_ = new WhisperWidget(proFeatures_);
            proFeatures_->addTab(whisperWidget_, "AI 자막");
            // AI 자막 → MPV 자막 오버레이 연결
            connect(whisperWidget_, &WhisperWidget::subtitleGenerated,
                    this, [this](const QString& text, double, double, int conf) {
                mpvWidget_->setAiSubtitle(text, conf);
            });
            connect(whisperWidget_, &WhisperWidget::seekToSubtitle,
                    core, [core](double sec) { core->seek(sec); });
        }

        if (!upscaleWidget_) {
            upscaleWidget_ = new UpscaleWidget(core, proFeatures_);
            proFeatures_->addTab(upscaleWidget_, "화질 개선");
        }

        if (!chapterWidget_) {
            chapterWidget_ = new ChapterWidget(core, proFeatures_);
            proFeatures_->addTab(chapterWidget_, "챕터/북마크");
            connect(chapterWidget_, &ChapterWidget::seekRequested,
                    core, [core](double sec) { core->seek(sec); });
            connect(core, &MpvCore::positionChanged,
                    chapterWidget_, &ChapterWidget::onPositionChanged);
            // 현재 파일 정보 즉시 로드
            if (!currentFilePath_.isEmpty()) {
                chapterWidget_->setDuration(totalDuration_);
                chapterWidget_->loadChapters();
                chapterWidget_->generateThumbnails(currentFilePath_);
            }
        }
        // 하이엔드 오디오 고급 기능 (컨볼루션 룸 코렉션 + VST)
        if (!audioAdvancedWidget_) {
            audioAdvancedWidget_ = new AudioAdvancedWidget(core, proFeatures_);
            proFeatures_->addTab(audioAdvancedWidget_, "하이엔드 오디오");
        }
        // 하이엔드 비디오 고급 기능 (3D LUT 케리브레이션)
        if (!videoAdvancedWidget_) {
            videoAdvancedWidget_ = new VideoAdvancedWidget(core, proFeatures_);
            proFeatures_->addTab(videoAdvancedWidget_, "하이엔드 비디오");
        }
        // 네트워크 탐색 (SMB/NAS + 360도 VR + 캐스팅)
        if (!networkBrowserWidget_) {
            networkBrowserWidget_ = new NetworkBrowserWidget(core, proFeatures_);
            proFeatures_->addTab(networkBrowserWidget_, "네트워크");
            // SMB 파일 열기 요청 연결
            connect(networkBrowserWidget_, &NetworkBrowserWidget::openFileRequested,
                    this, [this](const QString& path) { openFiles({path});
    connect(networkBrowserWidget_, &NetworkBrowserWidget::fileRequested,
            this, [this](const QString& url){ openFiles({url}); }); });
        }
        // 스마트 미디어 라이브러리
        if (!mediaLibrary_) {
            mediaLibrary_ = new MediaLibraryWidget(proFeatures_);
            proFeatures_->addTab(mediaLibrary_, "미디어 라이브러리");
            connect(mediaLibrary_, &MediaLibraryWidget::fileRequested,
                    this, [this](const QString& path) { openFiles({path}); });
        }
        if (!subtitleEditor_) {
            subtitleEditor_ = new SubtitleEditorWidget(mpvWidget_->core(), proFeatures_);
            proFeatures_->addTab(subtitleEditor_, "자막 편집기");
        }
        // 재생 통계/최근 감상 화면
        if (!statsWidget_) {
            statsWidget_ = new QWidget(proFeatures_);
            statsWidget_->setStyleSheet("background:#111; color:#ccc;");
            auto* statsLayout = new QVBoxLayout(statsWidget_);
            statsLayout->setContentsMargins(12, 12, 12, 12);
            statsLayout->setSpacing(8);

            auto* statsTitle = new QLabel("최근 감상 기록", statsWidget_);
            statsTitle->setStyleSheet("font-size:14px; font-weight:bold; color:#4fc3f7; background:transparent;");
            statsLayout->addWidget(statsTitle);

            auto* statsTable = new QTableWidget(statsWidget_);
            statsTable->setColumnCount(3);
            statsTable->setHorizontalHeaderLabels({"파일명", "재생 횟수", "마지막 재생"});
            statsTable->setStyleSheet(
                "QTableWidget { background:#1a1a1a; color:#ccc; border:none; gridline-color:#2a2a2a; }"
                "QHeaderView::section { background:#0d0d0d; color:#888; border:none; padding:4px; font-size:11px; }"
                "QTableWidget::item { padding:4px 8px; border-bottom:1px solid #1e1e1e; }"
                "QTableWidget::item:selected { background:#1a3a5c; }");
            statsTable->horizontalHeader()->setStretchLastSection(true);
            statsTable->verticalHeader()->setVisible(false);
            statsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
            statsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
            statsTable->setAlternatingRowColors(false);

            // QSettings에서 stats/ 키 읽어서 표시
            QSettings s("Sorinuri", "SorinuriPlayer");
            s.beginGroup("stats");
            QStringList keys = s.childGroups();
            s.endGroup();

            // 마지막 재생 시각 기준 정렬
            QVector<std::tuple<QString, int, QString, QString>> entries;
            for (const QString& key : keys) {
                QString path = s.value("stats/" + key + "/path").toString();
                int count    = s.value("stats/" + key + "/count", 0).toInt();
                QString last = s.value("stats/" + key + "/last_played").toString();
                if (!path.isEmpty())
                    entries.append({path, count, last, key});
            }
            std::sort(entries.begin(), entries.end(),
                [](const auto& a, const auto& b) { return std::get<2>(a) > std::get<2>(b); });

            statsTable->setRowCount(qMin((int)entries.size(), 100));
            for (int i = 0; i < qMin((int)entries.size(), 100); ++i) {
                const auto& [path, count, last, key] = entries[i];
                statsTable->setItem(i, 0, new QTableWidgetItem(QFileInfo(path).fileName()));
                statsTable->item(i, 0)->setToolTip(path);
                statsTable->setItem(i, 1, new QTableWidgetItem(QString::number(count) + "회"));
                statsTable->setItem(i, 2, new QTableWidgetItem(last.left(16).replace('T', ' ')));
            }

            statsLayout->addWidget(statsTable, 1);

            // ── 요약 통계 행 ─────────────────────────────────────────────────────────────────────
            int totalCount = 0;
            for (const auto& [path, count, last, key] : entries) totalCount += count;
            auto* summaryLabel = new QLabel(
                QString("전체 재생 기록: %1개 파일  |  누적 재생 횟수: %2회")
                    .arg(entries.size()).arg(totalCount),
                statsWidget_);
            summaryLabel->setStyleSheet("color:#666; font-size:11px; background:transparent;");
            statsLayout->addWidget(summaryLabel);

            // ── CSV / JSON 내보내기 버튼 행 ─────────────────────────────────────────────────────────────────────
            auto* exportRow = new QHBoxLayout;
            exportRow->addStretch();
            auto* btnExportCsv = new QPushButton("CSV 내보내기", statsWidget_);
            auto* btnExportJson = new QPushButton("JSON 내보내기", statsWidget_);
            const QString exportBtnStyle =
                "QPushButton { background:#1e1e1e; color:#aaa; border:1px solid #333;"
                "border-radius:4px; padding:4px 14px; font-size:11px; }"
                "QPushButton:hover { background:#2a2a2a; color:#fff; }";
            btnExportCsv->setStyleSheet(exportBtnStyle);
            btnExportJson->setStyleSheet(exportBtnStyle);
            btnExportCsv->setFocusPolicy(Qt::NoFocus);
            btnExportJson->setFocusPolicy(Qt::NoFocus);
            exportRow->addWidget(btnExportCsv);
            exportRow->addWidget(btnExportJson);
            statsLayout->addLayout(exportRow);

            // CSV 내보내기
            connect(btnExportCsv, &QPushButton::clicked, this, [entries]() {
                QString path = QFileDialog::getSaveFileName(
                    nullptr, "재생 통계 CSV 내보내기", "",
                    "CSV 파일 (*.csv)"
                );
                if (path.isEmpty()) return;
                QFile f(path);
                if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return;
                QTextStream out(&f);
                out.setEncoding(QStringConverter::Utf8);
                out << "\"파일명\",\"전체 경로\",\"재생 횟수\",\"마지막 재생\"\n";
                for (const auto& [fpath, count, last, key] : entries) {
                    QString fn   = QFileInfo(fpath).fileName(); fn.replace('"', "''");
                    QString fp   = fpath;                       fp.replace('"', "''");
                    QString dt   = last.left(16);               dt.replace('T', ' ');
                    out << QString("\"%1\",\"%2\",%3,\"%4\"\n")
                           .arg(fn).arg(fp).arg(count).arg(dt);
                }
                f.close();
            });

            // JSON 내보내기
            connect(btnExportJson, &QPushButton::clicked, this, [entries]() {
                QString path = QFileDialog::getSaveFileName(
                    nullptr, "재생 통계 JSON 내보내기", "",
                    "JSON 파일 (*.json)"
                );
                if (path.isEmpty()) return;
                QFile f(path);
                if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return;
                QTextStream out(&f);
                out.setEncoding(QStringConverter::Utf8);
                out << "[\n";
                for (int i = 0; i < entries.size(); ++i) {
                    const auto& [fpath, count, last, key] = entries[i];
                    QString fn2  = QFileInfo(fpath).fileName(); fn2.replace('"', "''");
                    QString fp2  = fpath; fp2.replace('\\', "/"); fp2.replace('"', "''");
                    QString dt2  = last.left(16); dt2.replace('T', ' ');
                    out << QString("  {\"file\": \"%1\", \"path\": \"%2\", \"count\": %3, \"last_played\": \"%4\"}%5\n")
                           .arg(fn2).arg(fp2).arg(count).arg(dt2)
                           .arg(i < entries.size()-1 ? "," : "");
                }
                out << "]\n";
                f.close();
            });

            proFeatures_->addTab(statsWidget_, "재생 통계");
        }
        // SORINURI ORIGINALS 탭
        if (!originalsWidget_) {
            originalsWidget_ = new OriginalsWidget(proFeatures_);
            proFeatures_->addTab(originalsWidget_, "♪ ORIGINALS");
            // 오리지널·YouTube 정렬 결과를 같은 통합 대기열로 재생한다.
            connect(originalsWidget_, &OriginalsWidget::queueRequested,
                    this, [this](const QList<PlaybackQueue::Entry>& entries, int startIndex) {
                playQueue(entries, startIndex);
            });
            connect(originalsWidget_, &OriginalsWidget::savePlaylistRequested,
                    this, [this](const QList<PlaybackQueue::Entry>& entries) {
                bool accepted = false;
                const QString name = QInputDialog::getText(this, "재생목록 저장", "재생목록 이름:",
                                                           QLineEdit::Normal, QString(), &accepted).trimmed();
                if (!accepted || name.isEmpty()) return;
                if (!playbackQueue_->saveNamedPlaylist(name, entries)) return;
                originalsWidget_->setSavedPlaylistNames(playbackQueue_->savedPlaylistNames());
                if (osdWidget_) osdWidget_->showInfo(QString("♪ 재생목록 저장: %1").arg(name), 1800);
            });
            connect(originalsWidget_, &OriginalsWidget::loadSavedPlaylistRequested,
                    this, [this](const QString& name) {
                const QList<PlaybackQueue::Entry> entries = playbackQueue_->loadNamedPlaylist(name);
                if (!entries.isEmpty()) playQueue(entries);
            });
            connect(originalsWidget_, &OriginalsWidget::deleteSavedPlaylistRequested,
                    this, [this](const QString& name) {
                if (!playbackQueue_->removeNamedPlaylist(name)) return;
                originalsWidget_->setSavedPlaylistNames(playbackQueue_->savedPlaylistNames());
                if (osdWidget_) osdWidget_->showInfo(QString("재생목록 삭제: %1").arg(name), 1600);
            });
            connect(originalsWidget_, &OriginalsWidget::repeatModeRequested,
                    this, [this](PlaybackQueue::RepeatMode mode) {
                playbackQueue_->setRepeatMode(mode);
            });
            originalsWidget_->setSavedPlaylistNames(playbackQueue_->savedPlaylistNames());
            originalsWidget_->setRepeatMode(playbackQueue_->repeatMode());
        }
        // 현재 재생 중 파일 전달
        if (originalsWidget_)
            originalsWidget_->setCurrentFile(currentFilePath_);
    }
    // ── 단축키 커스터마이징 탭 ─────────────────────────────────────────────────────────────────────
    if (!shortcutConfigWidget_) {
        shortcutConfigWidget_ = new ShortcutConfigWidget(proFeatures_);
        proFeatures_->addTab(shortcutConfigWidget_, "⌨ 단축키");
        // 단축키 변경 시 MainWindow에 알림
        connect(shortcutConfigWidget_, &ShortcutConfigWidget::shortcutsChanged,
                this, [this]() {
                    if (osdWidget_) osdWidget_->showInfo("⌨ 단축키 설정 저장됨", 2000);
                });
    }
    // ── 화면 녹화 탭 ─────────────────────────────────────────────────────────────────────
    if (!screenRecorder_) {
        screenRecorder_ = new ScreenRecorder(proFeatures_);
        proFeatures_->addTab(screenRecorder_, "🎥 화면 녹화");

        // v6.17.0 신규 탭
        if (!voiceControlWidget_) {
            voiceControlWidget_ = new VoiceControlWidget(proFeatures_);
            // 음성 명령 시그널 연결
            connect(voiceControlWidget_, &VoiceControlWidget::commandPlay,
                    this, [this](){ mpvWidget_->core()->command({"cycle", "pause"}); });
            connect(voiceControlWidget_, &VoiceControlWidget::commandPause,
                    this, [this](){ mpvWidget_->core()->command({"cycle", "pause"}); });
            connect(voiceControlWidget_, &VoiceControlWidget::commandStop,
                    this, [this](){ mpvWidget_->core()->command({"stop"}); });
            connect(voiceControlWidget_, &VoiceControlWidget::commandNext,
                    this, [this](){ mpvWidget_->core()->command({"playlist-next"}); });
            connect(voiceControlWidget_, &VoiceControlWidget::commandPrev,
                    this, [this](){ mpvWidget_->core()->command({"playlist-prev"}); });
            connect(voiceControlWidget_, &VoiceControlWidget::commandVolume,
                    this, [this](int v){ mpvWidget_->core()->setProperty("volume", v); });
            connect(voiceControlWidget_, &VoiceControlWidget::commandMute,
                    this, [this](){ mpvWidget_->core()->command({"cycle", "mute"}); });
            connect(voiceControlWidget_, &VoiceControlWidget::commandSeek,
                    this, [this](double s){ mpvWidget_->core()->seek(s, false); });
            connect(voiceControlWidget_, &VoiceControlWidget::commandFullscreen,
                    this, [this](){ toggleFullscreen(); });
        }
        proFeatures_->addTab(voiceControlWidget_, "🎙 음성 제어");

        // 클라우드 드라이브 UI 탭
        if (!cloudDriveBrowserWidget_) {
            cloudDriveBrowserWidget_ = new CloudDriveBrowserWidget(cloudDriveManager_, proFeatures_);
            connect(cloudDriveBrowserWidget_, &CloudDriveBrowserWidget::fileRequested,
                    this, [this](const QString& url, const QString& /*name*/) {
                openFiles({url});
            });
        }
        proFeatures_->addTab(cloudDriveBrowserWidget_, "☁ 클라우드");
        // 녹화 완료 시 OSD 알림
        connect(screenRecorder_, &ScreenRecorder::recordingStarted,
                this, [this](const QString&) {
                    if (osdWidget_) osdWidget_->showInfo("🔴 화면 녹화 시작", 2000);
                });
        connect(screenRecorder_, &ScreenRecorder::recordingStopped,
                this, [this](const QString& path) {
                    if (osdWidget_) osdWidget_->showInfo("✅ 녹화 완료: " + QFileInfo(path).fileName(), 3000);
                });
    }
    proFeatures_->setVisible(isProFeaturesOpen_);
}

void MainWindow::resizeEvent(QResizeEvent* e) {
    QMainWindow::resizeEvent(e);
    // 단축키 오버레이 크기를 영상 위젯에 맞춤
    if (shortcutOverlay_ && mpvWidget_) {
        shortcutOverlay_->setGeometry(mpvWidget_->rect());
    }
}

// ── 창 크기 조절 ──────────────────────────────────────────────────
int MainWindow::getResizeEdge(const QPoint& pos) const {
    int m = RESIZE_MARGIN, w = width(), h = height();
    bool L = pos.x() < m, R = pos.x() > w-m, T = pos.y() < m, B = pos.y() > h-m;
    if (L&&T) return 5; if (R&&T) return 6;
    if (L&&B) return 7; if (R&&B) return 8;
    if (L) return 1; if (R) return 2;
    if (T) return 3; if (B) return 4;
    return 0;
}

bool MainWindow::nativeEvent(const QByteArray& type, void* msg, qintptr* result) {
#ifdef Q_OS_WIN
    if (type == "windows_generic_MSG") {
        MSG* m = static_cast<MSG*>(msg);

        // ── 절전/화면 잠금 후 복교 시 OpenGL 콘텍스트 갱신 ─────────────
        // Windows Modern Standby 복교 시 D3D11/OpenGL 콘텍스트가 무효화될 수 있음
        // PBT_APMRESUMESUSPEND: 절전 복교 (S3 Sleep 후 재개)
        // PBT_APMRESUMEAUTOMATIC: 자동 절전 복교 (Modern Standby)
        // PBT_APMRESUMECRITICAL: 임계 전원 복교
        // ── 전원 이벤트 통합 처리 (WM_POWERBROADCAST) ───────────────────────
        // 주의: WM_POWERBROADCAST는 반드시 TRUE 반환해야 함 (Windows 문서)
        // 배터리 모드 전환(PBT_APMPOWERSTATUSCHANGE)과 절전 복귀를
        // 하나의 if 블록에서 모두 처리하여 early-return 충돌 방지
        if (m->message == WM_POWERBROADCAST) {
            // ─ 절전 복귀: 렌더링 컨텍스트 갱신 ─────────────────────────────
            if (m->wParam == PBT_APMRESUMESUSPEND ||
                m->wParam == PBT_APMRESUMEAUTOMATIC ||
                m->wParam == PBT_APMRESUMECRITICAL) {
                qInfo() << "[MainWindow] 절전 복귀 감지 → 렌더링·오디오 출력 복구 예약";
                // 렌더링은 빠르게 갱신하되, HDMI/WASAPI 엔드포인트는 드라이버가
                // 완전히 복원된 뒤에만 정책을 재협상한다.
                QTimer::singleShot(500, this, [this]() {
                    if (mpvWidget_) {
                        mpvWidget_->update();
                        qInfo() << "[MainWindow] 절전 복귀 후 렌더링 재시작 완료";
                    }
                });
                scheduleAudioOutputRecovery(1200);
            }
            // 전원 상태 변경에서 timeBeginPeriod(1)을 반복 요청하지 않는다.
            // Windows는 각 요청마다 동일한 timeEndPeriod 호출을 요구하며, libmpv는
            // 자체 재생 수명주기에서 필요한 타이머 해상도를 관리한다.
            // WM_POWERBROADCAST는 반드시 TRUE 반환 (Windows 문서 요구사항)
            *result = TRUE;
            return true;
        }

        // ── 오디오 기기 핫플러그 처리 (WM_DEVICECHANGE) ─────────────
        // 헤드폰/스피커 연결·해제 시 WASAPI 오디오 장치 자동 재초기화
        // DBT_DEVICEARRIVAL(0x8000): 장치 연결
        // DBT_DEVICEREMOVECOMPLETE(0x8004): 장치 해제
        if (m->message == WM_DEVICECHANGE) {
            const WPARAM DBT_DEVICEARRIVAL_W       = 0x8000;
            const WPARAM DBT_DEVICEREMOVECOMPLETE_W = 0x8004;
            if (m->wParam == DBT_DEVICEARRIVAL_W ||
                m->wParam == DBT_DEVICEREMOVECOMPLETE_W) {
                qInfo() << "[MainWindow] 오디오 장치 변경 감지 → 출력 정책 복구 예약";
                // 장치 열거가 끝난 뒤 선택 장치·독점·원본 채널·패스스루 정책을
                // 모두 복원한 다음 AO를 재초기화한다. 절전 복귀 이벤트와 겹치면
                // 타이머가 병합되어 한 번만 실행된다.
                scheduleAudioOutputRecovery(800);
            }
        }

        // ── 디스플레이 구성 변경 처리 (WM_DISPLAYCHANGE) ──────────────
        // 외부 모니터 연결/해제 시 OpenGL 컨텍스트 손실 방지
        // 해상도·색심도 변경 시에도 렌더링 재초기화 수행
        if (m->message == WM_DISPLAYCHANGE) {
            qInfo() << "[MainWindow] 디스플레이 구성 변경 감지 → 렌더링 갱신 예약";
            // 500ms 지연: 드라이버가 새 디스플레이 구성을 완전히 적용할 시간 확보
            QTimer::singleShot(500, this, [this]() {
                if (mpvWidget_) {
                    // 렌더링만 갱신 - redetectGpuAndApply() 호출 제거
                    // redetectGpuAndApply()는 hwdec/video-sync를 재설정하여
                    // 프로젝터 설정 변경 시 프레임 끊김 유발
                    mpvWidget_->update();
                    qInfo() << "[MainWindow] 디스플레이 변경 후 렌더링 갱신 완료";
                }
            });
        }

        // Alt+F4: FramelessWindowHint이 시스템 메시지를 막으므로 직접 처리
        if (m->message == WM_SYSKEYDOWN && m->wParam == VK_F4) {
            close();
            return true;
        }

        // WM_SYSCOMMAND SC_CLOSE (Alt+F4 또는 시스템 메뉴 닫기)
        if (m->message == WM_SYSCOMMAND && (m->wParam & 0xFFF0) == SC_CLOSE) {
            close();
            return true;
        }

        // 미디어 키 처리 (재생/일시정지, 이전곡, 다음곡, 정지)
        if (m->message == WM_APPCOMMAND) {
            int cmd = GET_APPCOMMAND_LPARAM(m->lParam);
            switch (cmd) {
                case APPCOMMAND_MEDIA_PLAY_PAUSE:
                case APPCOMMAND_MEDIA_PLAY:
                case APPCOMMAND_MEDIA_PAUSE:
                    mpvWidget_->core()->togglePause();
                    return true;
                case APPCOMMAND_MEDIA_NEXTTRACK:
                    mpvWidget_->core()->command({"playlist-next"});
                    return true;
                case APPCOMMAND_MEDIA_PREVIOUSTRACK:
                    mpvWidget_->core()->command({"playlist-prev"});
                    return true;
                case APPCOMMAND_MEDIA_STOP:
                    mpvWidget_->core()->stop();
                    return true;
            }
        }

        if (m->message == WM_NCHITTEST) {
            // WM_NCHITTEST lParam은 물리 픽셀(스크린 좌표)이므로
            // DPI 스케일로 나눠서 논리 픽셀으로 변환
            const double dpr = devicePixelRatio();
            int screenX = GET_X_LPARAM(m->lParam);
            int screenY = GET_Y_LPARAM(m->lParam);
            QPoint logicalGlobal(qRound(screenX / dpr), qRound(screenY / dpr));
            QPoint pos = mapFromGlobal(logicalGlobal);

            int edge = getResizeEdge(pos);
            if (edge > 0) {
                static const LRESULT edges[] = {0,HTLEFT,HTRIGHT,HTTOP,HTBOTTOM,
                    HTTOPLEFT,HTTOPRIGHT,HTBOTTOMLEFT,HTBOTTOMRIGHT};
                *result = edges[edge];
                return true;
            }
        }
    }
#endif
    return QMainWindow::nativeEvent(type, msg, result);
}

void MainWindow::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        resizeEdge_ = getResizeEdge(e->pos());
        if (resizeEdge_ > 0) {
            resizing_ = true;
            resizeStart_ = e->globalPosition().toPoint();
            resizeStartSize_ = size();
        }
    }
    QMainWindow::mousePressEvent(e);
}

bool MainWindow::eventFilter(QObject* obj, QEvent* event) {
    if (obj == mpvWidget_) {
        // 클릭 시 MainWindow로 포커스 재설정 - 키 이벤트가 keyPressEvent로 정상 전달됨
        if (event->type() == QEvent::MouseButtonPress) {
            setFocus();
        } else if (event->type() == QEvent::MouseButtonRelease) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::RightButton) {
                showUI();
                showContextMenu(me->globalPosition().toPoint());
                return true;
            }
        }
        if (event->type() == QEvent::MouseMove) {
            // 중앙 영상 영역의 마우스 이동은 UI를 건드리지 않는다.
            // 상단·하단 가장자리 진입에만 각각의 UI를 노출한다.
            auto* me = static_cast<QMouseEvent*>(event);
            revealUiForVideoEdge(me->pos());
        } else if (event->type() == QEvent::MouseButtonDblClick) {
            // 영상 더블클릭 → 전체화면 토글 (팟플레이어 동일 동작)
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                toggleFullscreen();
                return true;
            }
        } else if (event->type() == QEvent::Wheel) {
            // 영상 위 휠 스크롤 → 볼륨 조절 (위=+5, 아래=-5)
            auto* we = static_cast<QWheelEvent*>(event);
            auto* core = mpvWidget_->core();
            if (we->angleDelta().y() > 0)
                core->setVolume(qMin(core->volume() + 5, 200));
            else
                core->setVolume(qMax(core->volume() - 5, 0));
            // OSD 볼륨 표시
            if (osdWidget_) {
                bool muted = core->getProperty("mute").toBool();
                osdWidget_->showVolume(core->volume(), muted);
            }
            showUI();  // 볼륨 변경 시 UI 잠깐 표시
            return true;
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::mouseMoveEvent(QMouseEvent* e) {
    // 중앙 화면의 단순 마우스 이동은 UI 노출을 유발하지 않는다.
    if (!resizing_) {
        int edge = getResizeEdge(e->pos());
        if (edge == 0) {
            unsetCursor();  // 리사이즈 마진 밖: 기본 커서로 복원
        } else {
            static const Qt::CursorShape cs[] = {
                Qt::ArrowCursor, Qt::SizeHorCursor, Qt::SizeHorCursor,
                Qt::SizeVerCursor, Qt::SizeVerCursor,
                Qt::SizeFDiagCursor, Qt::SizeBDiagCursor,
                Qt::SizeBDiagCursor, Qt::SizeFDiagCursor
            };
            setCursor(cs[edge]);
        }
    } else {
        QPoint d = e->globalPosition().toPoint() - resizeStart_;
        QSize ns = resizeStartSize_;
        QPoint np = pos();
        switch (resizeEdge_) {
        case 1: ns.setWidth(resizeStartSize_.width()-d.x()); np.setX(pos().x()+d.x()); break;
        case 2: ns.setWidth(resizeStartSize_.width()+d.x()); break;
        case 3: ns.setHeight(resizeStartSize_.height()-d.y()); np.setY(pos().y()+d.y()); break;
        case 4: ns.setHeight(resizeStartSize_.height()+d.y()); break;
        case 5: ns=QSize(resizeStartSize_.width()-d.x(),resizeStartSize_.height()-d.y());
                np=QPoint(pos().x()+d.x(),pos().y()+d.y()); break;
        case 6: ns=QSize(resizeStartSize_.width()+d.x(),resizeStartSize_.height()-d.y());
                np.setY(pos().y()+d.y()); break;
        case 7: ns=QSize(resizeStartSize_.width()-d.x(),resizeStartSize_.height()+d.y());
                np.setX(pos().x()+d.x()); break;
        case 8: ns=QSize(resizeStartSize_.width()+d.x(),resizeStartSize_.height()+d.y()); break;
        }
        if (ns.width()>=minimumWidth() && ns.height()>=minimumHeight())
            setGeometry(QRect(np, ns));
    }
    QMainWindow::mouseMoveEvent(e);
}

void MainWindow::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::RightButton) {
        showContextMenu(e->globalPosition().toPoint());
        return;
    }
    resizing_ = false; resizeEdge_ = 0;
    QMainWindow::mouseReleaseEvent(e);
}

void MainWindow::showContextMenu(const QPoint& globalPos) {
    auto* core = mpvWidget_->core();
    bool playing = !core->isPaused() && !core->currentFile().isEmpty();
    bool hasFile = !core->currentFile().isEmpty();

    QMenu menu(this);
    menu.setStyleSheet(SorinuriUi::menuStyle());

    // 파일 열기
    QAction* actOpen = menu.addAction("파일 열기...");
    actOpen->setShortcut(QKeySequence("Ctrl+O"));
    connect(actOpen, &QAction::triggered, this, &MainWindow::onOpenFile);

    // URL 열기 (유튜브, 트위치 등)
    QAction* actUrl = menu.addAction("URL 열기... (YouTube/스트리밍)");
    actUrl->setShortcut(QKeySequence("Ctrl+U"));
    connect(actUrl, &QAction::triggered, this, &MainWindow::onOpenUrl);
    // 최근 파일 서브메뉴
    QMenu* recentMenu = menu.addMenu("최근 파일");
    {
        const QStringList recent = settings_.value("recent/files").toStringList();
        if (recent.isEmpty()) {
            recentMenu->addAction("최근 파일 없음")->setEnabled(false);
        } else {
            for (const QString& p : recent) {
                QAction* act = recentMenu->addAction(QFileInfo(p).fileName());
                act->setToolTip(p);
                connect(act, &QAction::triggered, this, [this, p]() {
                    if (QFileInfo::exists(p)) openFiles({p});
                });
            }
            recentMenu->addSeparator();
            QAction* clearAct = recentMenu->addAction("목록 지우기");
            connect(clearAct, &QAction::triggered, this, [this]() {
                settings_.remove("recent/files");
            });
        }
    }
    menu.addSeparator();

    // 재생 제어
    QAction* actPlay = menu.addAction(playing ? "일시정지" : "재생");
    actPlay->setEnabled(hasFile);
    connect(actPlay, &QAction::triggered, core, &MpvCore::togglePause);

    QAction* actStop = menu.addAction("정지");
    actStop->setEnabled(hasFile);
    connect(actStop, &QAction::triggered, core, &MpvCore::stop);

    QAction* actPrev = menu.addAction("이전");
    actPrev->setEnabled(hasFile);
    connect(actPrev, &QAction::triggered, [core]() { core->command({"playlist-prev"}); });

    QAction* actNext = menu.addAction("다음");
    actNext->setEnabled(hasFile);
    connect(actNext, &QAction::triggered, [core]() { core->command({"playlist-next"}); });

    QAction* actMute = menu.addAction("음소거");
    actMute->setEnabled(hasFile);
    actMute->setCheckable(true);
    actMute->setChecked(core->getProperty("mute").toBool());
    connect(actMute, &QAction::triggered, [core](bool muted) { core->setMuted(muted); });

    menu.addSeparator();

    // 오디오 트랙 서브메뉴: 실제 track-list를 읽어 현재 선택 상태와 원본 포맷을 함께 표시한다.
    QMenu* audioMenu = menu.addMenu("오디오 트랙");
    auto* audioGroup = new QActionGroup(audioMenu);
    audioGroup->setExclusive(true);
    const int currentAid = core->getProperty("aid").toInt();
    const QVariantList audioTracks = core->audioTracks();
    if (audioTracks.isEmpty()) {
        audioMenu->addAction("오디오 트랙 없음")->setEnabled(false);
    } else {
        for (const QVariant& t : audioTracks) {
            const QVariantMap m = t.toMap();
            const int id = m.value("id").toInt();
            const QString lang = m.value("lang").toString().isEmpty()
                ? QStringLiteral("언어 미지정") : m.value("lang").toString();
            const QString codec = m.value("codec").toString().toUpper();
            const int channels = m.value("demux-channel-count").toInt();
            QString label = QString("%1 · %2").arg(lang, codec.isEmpty() ? QStringLiteral("오디오") : codec);
            if (channels > 0) label += QString(" · %1ch").arg(channels);
            if (!m.value("title").toString().isEmpty()) label += " · " + m.value("title").toString();
            QAction* a = audioMenu->addAction(label);
            a->setCheckable(true);
            a->setChecked(id == currentAid);
            audioGroup->addAction(a);
            connect(a, &QAction::triggered, [core, id]() { core->setAudioTrack(id); });
        }
    }

    // 자막 서브메뉴
    QMenu* subMenu = menu.addMenu("자막");
    // OpenSubtitles 자막 검색
    QAction* actSubSearch = subMenu->addAction("자막 자동 검색 (OpenSubtitles)");
    actSubSearch->setEnabled(!currentFilePath_.isEmpty() && !currentFilePath_.startsWith("http"));
    connect(actSubSearch, &QAction::triggered, this, &MainWindow::onSubtitleSearch);
    subMenu->addSeparator();
    auto* subGroup = new QActionGroup(subMenu);
    subGroup->setExclusive(true);
    const int currentSid = core->getProperty("sid").toInt();
    QAction* actSubOff = subMenu->addAction("자막 끄기");
    actSubOff->setCheckable(true);
    actSubOff->setChecked(currentSid == 0);
    subGroup->addAction(actSubOff);
    connect(actSubOff, &QAction::triggered, [core]() { core->setSubtitleTrack(0); });
    subMenu->addSeparator();
    const QVariantList subTracks = core->subtitleTracks();
    if (subTracks.isEmpty()) {
        subMenu->addAction("자막 트랙 없음")->setEnabled(false);
    }
    for (const QVariant& t : subTracks) {
        const QVariantMap m = t.toMap();
        const int id = m.value("id").toInt();
        const QString lang = m.value("lang").toString().isEmpty()
            ? QStringLiteral("언어 미지정") : m.value("lang").toString();
        const QString codec = m.value("codec").toString().toUpper();
        QString label = QString("%1 · %2").arg(lang, codec.isEmpty() ? QStringLiteral("자막") : codec);
        if (!m.value("title").toString().isEmpty()) label += " · " + m.value("title").toString();
        QAction* a = subMenu->addAction(label);
        a->setCheckable(true);
        a->setChecked(id == currentSid);
        subGroup->addAction(a);
        connect(a, &QAction::triggered, [core, id]() { core->setSubtitleTrack(id); });
    }

    menu.addSeparator();

    // 화면 크기 서브메뉴
    QMenu* sizeMenu = menu.addMenu("화면 크기");
    QAction* act50  = sizeMenu->addAction("50%  (S)");
    QAction* act100 = sizeMenu->addAction("100%  (1)");
    QAction* act150 = sizeMenu->addAction("150%");
    QAction* act200 = sizeMenu->addAction("200%");
    connect(act50,  &QAction::triggered, [this, core]() {
        int w = core->getProperty("video-params/w").toInt();
        int h = core->getProperty("video-params/h").toInt();
        if (w>0 && h>0) resize(w/2, h/2 + 62);
    });
    connect(act100, &QAction::triggered, [this, core]() {
        int w = core->getProperty("video-params/w").toInt();
        int h = core->getProperty("video-params/h").toInt();
        if (w>0 && h>0) resize(w, h + 62);
    });
    connect(act150, &QAction::triggered, [this, core]() {
        int w = core->getProperty("video-params/w").toInt();
        int h = core->getProperty("video-params/h").toInt();
        if (w>0 && h>0) resize(w*3/2, h*3/2 + 62);
    });
    connect(act200, &QAction::triggered, [this, core]() {
        int w = core->getProperty("video-params/w").toInt();
        int h = core->getProperty("video-params/h").toInt();
        if (w>0 && h>0) resize(w*2, h*2 + 62);
    });

    // 화면 비율 서브메뉴 (영상 비율 강제 설정)
    QMenu* aspectMenu = menu.addMenu("화면 비율");
    QAction* actAspectAuto = aspectMenu->addAction("원본 비율 (자동)");
    QAction* actAspect169  = aspectMenu->addAction("16:9");
    QAction* actAspect43   = aspectMenu->addAction("4:3");
    QAction* actAspect235  = aspectMenu->addAction("2.35:1 (시네마스코프)");
    QAction* actAspect11   = aspectMenu->addAction("1:1");
    connect(actAspectAuto, &QAction::triggered, [core]() { core->setProperty("video-aspect-override", QString("-1")); });
    connect(actAspect169,  &QAction::triggered, [core]() { core->setProperty("video-aspect-override", QString("16:9")); });
    connect(actAspect43,   &QAction::triggered, [core]() { core->setProperty("video-aspect-override", QString("4:3")); });
    connect(actAspect235,  &QAction::triggered, [core]() { core->setProperty("video-aspect-override", QString("2.35:1")); });
    connect(actAspect11,   &QAction::triggered, [core]() { core->setProperty("video-aspect-override", QString("1:1")); });

    // 멀티뷰 서브메뉴
    QMenu* multiViewMenu = menu.addMenu("멀티뷰");
    QAction* actSingle = multiViewMenu->addAction("단일 화면");
    QAction* actDualH  = multiViewMenu->addAction("2분할 (좌우)");
    QAction* actDualV  = multiViewMenu->addAction("2분할 (상하)");
    QAction* actPIP    = multiViewMenu->addAction("PIP (화면 속 화면)");
    QAction* actQuad   = multiViewMenu->addAction("4분할");
    connect(actSingle, &QAction::triggered, this, [this]() { toggleMultiView(MultiViewLayout::Single); });
    connect(actDualH,  &QAction::triggered, this, [this]() { toggleMultiView(MultiViewLayout::DualH); });
    connect(actDualV,  &QAction::triggered, this, [this]() { toggleMultiView(MultiViewLayout::DualV); });
    connect(actPIP,    &QAction::triggered, this, [this]() { toggleMultiView(MultiViewLayout::PIP); });
    connect(actQuad,   &QAction::triggered, this, [this]() { toggleMultiView(MultiViewLayout::Quad); });
    // 전체화면
    QAction* actFull = menu.addAction(isFullscreen_ ? "전체화면 해제  (F)": "전체화면  (F)");
    connect(actFull, &QAction::triggered, this, &MainWindow::toggleFullscreen);

    menu.addSeparator();

    // 화면 캐치
    QAction* actCapture = menu.addAction("화면 캐치  (C)");
    connect(actCapture, &QAction::triggered, this, [this, core]() {
        // MPV screenshot 명령 - 실행 파일 디렉토리에 PNG 저장
        {
            QString dir = settings_.value("general/screenshot_dir", QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)).toString();
            QString fmt = settings_.value("general/screenshot_format", "PNG").toString().toLower();
            core->setProperty("screenshot-dir", dir);
            core->setProperty("screenshot-format", fmt);
            core->setProperty("screenshot-template", "Sorinuri_%F_%p_%n");
            core->command({"screenshot", "video"});
        }
    });

    // 구간 반복 (A-B 반복)
    QMenu* abMenu = menu.addMenu("구간 반복 (A-B)");
    QAction* actAbA = abMenu->addAction("A 지점 설정  (I)");
    QAction* actAbB = abMenu->addAction("B 지점 설정  (O)");
    QAction* actAbClear = abMenu->addAction("구간 반복 해제");
    connect(actAbA, &QAction::triggered, [core]() { core->command({"ab-loop-a"}); });
    connect(actAbB, &QAction::triggered, [core]() { core->command({"ab-loop-b"}); });
    connect(actAbClear, &QAction::triggered, [core]() {
        core->setProperty("ab-loop-a", QString("no"));
        core->setProperty("ab-loop-b", QString("no"));
    });

    // 재생 속도 프리셋
    QMenu* speedMenu = menu.addMenu("재생 속도");
    struct SpeedItem { QString label; double speed; };
    QVector<SpeedItem> speeds = {
        {"0.25x (매우 느리게)", 0.25},
        {"0.5x  (느리게)",           0.5},
        {"0.75x",                            0.75},
        {"1.0x  (정상)",                1.0},
        {"1.25x",                            1.25},
        {"1.5x  (빠르게)",           1.5},
        {"2.0x  (매우 빠르게)",  2.0},
    };
    for (const auto& s : speeds) {
        QAction* a = speedMenu->addAction(s.label);
        double spd = s.speed;
        connect(a, &QAction::triggered, [core, spd]() {
            core->command({"set", "speed", QString::number(spd)});
        });
    }

    menu.addSeparator();

    // 설정
    QAction* actSettings = menu.addAction("환경 설정...");
    actSettings->setShortcut(QKeySequence("F5"));
    connect(actSettings, &QAction::triggered, this, &MainWindow::onSettingsRequested);

    menu.addSeparator();

    // 종료
    QAction* actQuit = menu.addAction("소리누리 종료");
    actQuit->setShortcut(QKeySequence("Alt+F4"));
    connect(actQuit, &QAction::triggered, this, &QMainWindow::close);

    menu.exec(globalPos);
}

void MainWindow::switchToPlayerMode() {
    isOttMode_ = false;
    mainStack_->setCurrentIndex(0);
    controlBar_->playerModeBtn()->setProperty("active", true);
    controlBar_->ottModeBtn()->setProperty("active", false);
    controlBar_->playerModeBtn()->style()->unpolish(controlBar_->playerModeBtn());
    controlBar_->playerModeBtn()->style()->polish(controlBar_->playerModeBtn());
    controlBar_->ottModeBtn()->style()->unpolish(controlBar_->ottModeBtn());
    controlBar_->ottModeBtn()->style()->polish(controlBar_->ottModeBtn());
    updateWindowTitle();
}

void MainWindow::switchToOttMode() {
    isOttMode_ = true;
    // 지연 초기화: 처음 OTT 탭 클릭 시 WebView2 생성
    if (!ottPage_) {
        ottPage_ = new OttWidget(this);
        QWidget* placeholder = mainStack_->widget(1);
        mainStack_->removeWidget(placeholder);
        delete placeholder;
        mainStack_->addWidget(ottPage_);
        connect(ottPage_, &OttWidget::titleChanged, this, [this](const QString& t) {
            if (isOttMode_) updateWindowTitle(t);
        });
    }
    mainStack_->setCurrentIndex(1);

    // OTT 모드 진입 시 광고 요청 (비동기, 광고 없으면 표시 안 함)
    if (adManager_)
        adManager_->fetchAd("ott");

    controlBar_->playerModeBtn()->setProperty("active", false);
    controlBar_->ottModeBtn()->setProperty("active", true);
    controlBar_->playerModeBtn()->style()->unpolish(controlBar_->playerModeBtn());
    controlBar_->playerModeBtn()->style()->polish(controlBar_->playerModeBtn());
    controlBar_->ottModeBtn()->style()->unpolish(controlBar_->ottModeBtn());
    controlBar_->ottModeBtn()->style()->polish(controlBar_->ottModeBtn());
    updateWindowTitle("소리누리 OTT");
}

void MainWindow::onOpenUrl() {
    UrlDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;
    QString url = dlg.url();
    if (url.isEmpty()) return;
    openUrl(url);
}

void MainWindow::openUrl(const QString& url) {
    // yt-dlp 지연 초기화 전 URL 입력이 와도 재생 요청을 잃지 않는다.
    if (!ytdlp_) {
        pendingUrl_ = url;
        QTimer::singleShot(550, this, [this]() {
            if (!pendingUrl_.isEmpty()) {
                const QString queuedUrl = pendingUrl_;
                pendingUrl_.clear();
                openUrl(queuedUrl);
            }
        });
        return;
    }
    // URL을 직접 MPV에 전달 (ytdl=yes로 자동 처리)
    // yt-dlp가 없으면 자동 다운로드
    if (!ytdlp_->isAvailable() && YtdlpManager::isSupportedUrl(url)) {
        // yt-dlp 다운로드 후 재생
        pendingUrl_ = url;

        ytdlpProgress_ = new QProgressDialog(
            "yt-dlp 다운로드 중...\n"
            "(유튜브 5.1 서라운드 지원을 위한 툴입니다)",
            "취소", 0, 100, this);
        ytdlpProgress_->setWindowTitle("yt-dlp 설치");
        ytdlpProgress_->setWindowModality(Qt::WindowModal);
        ytdlpProgress_->setStyleSheet(
            "QProgressDialog { background: #1a1a1a; color: #ddd; }"
            "QProgressBar { background: #111; border: 1px solid #333; }"
            "QProgressBar::chunk { background: #1565c0; }");
        ytdlpProgress_->show();

        connect(ytdlpProgress_, &QProgressDialog::canceled, [this]() {
            pendingUrl_.clear();
        });

        ytdlp_->downloadOrUpdate();
        return;
    }

    // yt-dlp 있으면 경로 설정 후 재생
    if (ytdlp_->isAvailable()) {
        // MPV에 yt-dlp 경로 알려줘서 해당 앱의 yt-dlp 사용
        QString ytdlpDir = QFileInfo(ytdlp_->ytdlpPath()).absolutePath();
        mpvWidget_->core()->setProperty("ytdl-raw-options",
            QString("paths=home:%1").arg(ytdlpDir));
    }

    mpvWidget_->showLogo(false);
    PlaybackQueue::Entry entry;
    entry.url = url;
    entry.title = url;
    entry.source = YtdlpManager::isYouTubeUrl(url) ? QStringLiteral("youtube") : QStringLiteral("stream");
    playQueue({entry});
}

void MainWindow::onYtdlpReady(const QString& path) {
    Q_UNUSED(path)
    if (ytdlpProgress_) {
        ytdlpProgress_->close();
        ytdlpProgress_->deleteLater();
        ytdlpProgress_ = nullptr;
    }
    if (!pendingYouTubeQueue_.isEmpty()) {
        const QList<PlaybackQueue::Entry> queue = pendingYouTubeQueue_;
        const int startIndex = pendingYouTubeQueueStartIndex_;
        pendingYouTubeQueue_.clear();
        playQueue(queue, startIndex);
        return;
    }
    if (!pendingUrl_.isEmpty()) {
        QString url = pendingUrl_;
        pendingUrl_.clear();
        openUrl(url);
    }
}

void MainWindow::onYtdlpDownloadProgress(int percent) {
    if (ytdlpProgress_) ytdlpProgress_->setValue(percent);
}

void MainWindow::onYtdlpDownloadFailed(const QString& error) {
    if (ytdlpProgress_) {
        ytdlpProgress_->close();
        ytdlpProgress_->deleteLater();
        ytdlpProgress_ = nullptr;
    }
    pendingUrl_.clear();
    pendingYouTubeQueue_.clear();
    QMessageBox::warning(this, "yt-dlp 다운로드 실패",
        QString("yt-dlp를 다운로드할 수 없습니다.\n%1\n\n"
                "yt-dlp.exe를 수동으로 다운로드하여 "
                "소리누리 폴더에 넣어주세요.\n"
                "https://github.com/yt-dlp/yt-dlp/releases").arg(error));
}

void MainWindow::loadSettings() {
    QScreen* scr = QGuiApplication::primaryScreen();
    QRect availGeom = scr ? scr->availableGeometry() : QRect(0, 0, 1920, 1080);
    QSize availSize = availGeom.size();

    // 72% 기본 크기: 재생 화면은 충분히 크게 열되, 250% HiDPI의 작은 논리 해상도도 넘지 않는다.
    const int minWidth  = qMin(800, availSize.width());
    const int minHeight = qMin(540, availSize.height());
    const int maxWidth  = qMin(1440, availSize.width());
    const int maxHeight = qMin(900,  availSize.height());
    const QSize defaultSize(
        qBound(minWidth,  availSize.width()  * 72 / 100, maxWidth),
        qBound(minHeight, availSize.height() * 72 / 100, maxHeight));

    // 저장된 창 크기 로드 - 화면보다 크거나, 너무 작거나, 화면 밖이면 자동 재설정
    QSize savedSize = settings_.value("window/size", QSize()).toSize();
    QPoint savedPos  = settings_.value("window/pos",  QPoint(-1,-1)).toPoint();

    bool needReset = false;
    if (!savedSize.isValid() || savedSize.isEmpty()) {
        needReset = true;  // 저장된 값 없음
    } else if (savedSize.width()  > availSize.width() ||
               savedSize.height() > availSize.height()) {
        needReset = true;  // 화면보다 큼서 하단바 접근 불가
    } else if (savedSize.width() < minWidth || savedSize.height() < minHeight) {
        needReset = true;  // 이전의 작은 저장값으로 컨트롤·정보 표시가 잘리는 문제 방지
    } else if (savedPos.x() >= 0) {
        // 저장된 위치가 화면 밖에 있으면 재설정
        QRect windowRect(savedPos, savedSize);
        if (!availGeom.intersects(windowRect))
            needReset = true;
    }

    QSize sz;
    QPoint pos;
    if (needReset) {
        sz  = defaultSize;
        pos = availGeom.center() - QPoint(sz.width()/2, sz.height()/2);
        // 다음 실행을 위해 저장
        settings_.setValue("window/size", sz);
        settings_.setValue("window/pos",  pos);
    } else {
        sz  = savedSize;
        pos = (savedPos.x() >= 0) ? savedPos
              : availGeom.center() - QPoint(sz.width()/2, sz.height()/2);
    }
    resize(sz);
    move(pos);

    // 항상 위에 고정 상태 복원
    bool alwaysOnTop = settings_.value("window/alwaysOnTop", false).toBool();
    if (alwaysOnTop) {
        setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
        titleBar_->setAlwaysOnTop(true);
    }
    int vol = settings_.value("audio/volume", 100).toInt();
    mpvWidget_->core()->setVolume(vol);
    controlBar_->setVolume(vol);
    // audio-exclusive: 노트북 기본값 false (공유 모드), 데스크톱 기본값 true (독점 모드)
    // 노트북 내장 스피커는 WASAPI 독점 모드를 지원하지 않는 경우가 많음
    // 실패 시 audio-fallback-to-null으로 영상 재생 보장
    {
        // WASAPI 독점 모드: 기본값 true (독점)
        // 독점 모드 실패 시 ao-reload 자동복구가 공유 모드로 폴백 처리
        // 노트북/데스크톱 구분 없이 동일하게 적용 (단순화)
        if (settings_.value("audio/exclusive", true).toBool())
            mpvWidget_->core()->setAudioExclusive(true);
    }
    if (settings_.value("audio/passthrough", true).toBool()) {
        QStringList codecs;
        if (settings_.value("audio/pt_ac3",    true).toBool()) codecs << "ac3";
        if (settings_.value("audio/pt_eac3",   true).toBool()) codecs << "eac3";
        if (settings_.value("audio/pt_dts",    true).toBool()) codecs << "dts";
        if (settings_.value("audio/pt_dtshd",  true).toBool()) codecs << "dts-hd";
        if (settings_.value("audio/pt_truehd", true).toBool()) codecs << "truehd";
        mpvWidget_->core()->setSpdifCodecs(codecs);
    }
}

void MainWindow::saveSettings() {
    settings_.setValue("window/size", size());
    settings_.setValue("window/pos",  pos());
    settings_.setValue("audio/volume", mpvWidget_->core()->volume());
}

void MainWindow::updateWindowTitle(const QString& filename) {
    QString t = filename.isEmpty() ? "소리누리" : QString("소리누리 — %1").arg(filename);
    setWindowTitle(t);
    titleBar_->setTitle(t);
}

void MainWindow::toggleMiniPlayer() {
    if (!isMusicMode_) return;

    // 처음 호출 시 지연 초기화
    if (!miniPlayer_) {
        miniPlayer_ = new MiniPlayerWidget(nullptr);  // 독립 창이므로 parent=nullptr
        auto* core = mpvWidget_->core();
        // 미니 플레이어 시그널 연결
        connect(miniPlayer_, &MiniPlayerWidget::playPauseRequested,
                core, &MpvCore::togglePause);
        connect(miniPlayer_, &MiniPlayerWidget::prevRequested,
                core, [core]() { core->command({"playlist-prev"}); });
        connect(miniPlayer_, &MiniPlayerWidget::nextRequested,
                core, [core]() { core->command({"playlist-next"}); });
        connect(miniPlayer_, &MiniPlayerWidget::expandRequested,
                this, &MainWindow::toggleMiniPlayer);
        // MPV 상태 → 미니 플레이어 업데이트
        connect(core, &MpvCore::positionChanged,
                miniPlayer_, [this](double pos) {
            if (miniPlayer_) miniPlayer_->updatePosition(pos, totalDuration_);
        });
        connect(core, &MpvCore::playbackStarted,
                miniPlayer_, [this]() { if (miniPlayer_) miniPlayer_->setPlaying(true); });
        connect(core, &MpvCore::playbackPaused,
                miniPlayer_, [this]() { if (miniPlayer_) miniPlayer_->setPlaying(false); });
    }

    if (miniPlayer_->isVisible()) {
        miniPlayer_->hide();
        show();
        activateWindow();
    } else {
        // 현재 트랙 메타 정보 전달
        if (musicPage_) {
            // MusicWidget에서 현재 메타 정보를 miniPlayer에 전달
            miniPlayer_->updatePosition(mpvWidget_->core()->position(), totalDuration_);
            miniPlayer_->setPlaying(!mpvWidget_->core()->isPaused());
        }
        hide();
        miniPlayer_->show();
                miniPlayer_->raise();
    }
}

void MainWindow::toggleCompactPlayer() {
    if (!isMusicMode_) return;

    // 첫 호출 시 지연 초기화
    if (!compactPlayer_) {
        compactPlayer_ = new CompactPlayerWidget(nullptr);
        auto* core = mpvWidget_->core();

        // 컨트롤 시그널 연결
        connect(compactPlayer_, &CompactPlayerWidget::playPauseRequested,
                core, &MpvCore::togglePause);
        connect(compactPlayer_, &CompactPlayerWidget::prevRequested,
                core, [core]() { core->command({"playlist-prev"}); });
        connect(compactPlayer_, &CompactPlayerWidget::nextRequested,
                core, [core]() { core->command({"playlist-next"}); });
        connect(compactPlayer_, &CompactPlayerWidget::seekRequested,
                core, [core](double pos) { core->seek(pos); });
        connect(compactPlayer_, &CompactPlayerWidget::volumeChanged,
                core, [core](int v) { core->setVolume(v); });
        connect(compactPlayer_, &CompactPlayerWidget::expandRequested,
                this, &MainWindow::toggleCompactPlayer);
        connect(compactPlayer_, &CompactPlayerWidget::trackSelected,
                core, [core](int idx) {
            core->command({"playlist-play-index", QString::number(idx)});
        });

        // MPV 상태 → 콤팩트 플레이어 업데이트
        connect(core, &MpvCore::positionChanged,
                compactPlayer_, [this](double pos) {
            if (compactPlayer_) compactPlayer_->updatePosition(pos, totalDuration_);
        });
        connect(core, &MpvCore::playbackStarted,
                compactPlayer_, [this]() {
            if (compactPlayer_) compactPlayer_->setPlaying(true);
        });
        connect(core, &MpvCore::playbackPaused,
                compactPlayer_, [this]() {
            if (compactPlayer_) compactPlayer_->setPlaying(false);
        });
    }

    if (compactPlayer_->isVisible()) {
        // 콤팩트 모드 종료 → 메인 창 복구
        compactPlayer_->hide();
        show();
        activateWindow();
    } else {
        // 코드 정보 전달
        if (musicPage_) {
            const auto& meta = musicPage_->currentMeta();
            compactPlayer_->setMeta(
                meta.title, meta.artist, meta.album,
                meta.albumArt, meta.codec,
                meta.bitDepth, meta.sampleRate,
                meta.channels, false);
            compactPlayer_->updatePosition(
                mpvWidget_->core()->position(), totalDuration_);
            compactPlayer_->setPlaying(!mpvWidget_->core()->isPaused());

            // 재생목록 전달
            QStringList paths;
            int cur = 0;
            auto* core = mpvWidget_->core();
            int count = core->getProperty("playlist-count").toInt();
            int curIdx = core->getProperty("playlist-pos").toInt();
            for (int i = 0; i < count; ++i) {
                paths << core->getProperty(
                    QString("playlist/%1/filename").arg(i)).toString();
            }
            compactPlayer_->setPlaylist(paths, curIdx);
        }
        hide();
        compactPlayer_->show();
        compactPlayer_->raise();
        compactPlayer_->activateWindow();
    }
}

void MainWindow::toggleWhisper(bool on) {
    // whisperWidget_이 없으면 ProFeatures 패널을 열어서 생성
    if (!whisperWidget_) {
        if (!isProFeaturesOpen_) toggleProFeatures();
        // 생성 후 재시도
        if (whisperWidget_) {
            whisperWidget_->setMediaFile(currentFilePath_);
            whisperWidget_->setActive(on);
        }
        return;
    }
    whisperWidget_->setMediaFile(currentFilePath_);
    whisperWidget_->setActive(on);
}

void MainWindow::onChapterBookmark() {
    if (!mpvWidget_) return;
    double pos = mpvWidget_->core()->getProperty("time-pos").toDouble();
    // chapterWidget_이 없으면 P 패널을 열어서 생성
    if (!chapterWidget_) {
        if (!isProFeaturesOpen_) toggleProFeatures();
        return;
    }
    chapterWidget_->addBookmark(pos);
}

void MainWindow::toggleMultiView(MultiViewLayout l) {
    if (l == MultiViewLayout::Single) {
        // 단일 모드: 멀티뷰 해제 → 일반 영상 모드로 복귀
        if (multiViewWidget_) {
            multiViewWidget_->hide();
        }
        playerStack_->setCurrentIndex(0);
        return;
    }

    // 멀티뷰 위젯 지연 초기화
    if (!multiViewWidget_) {
        multiViewWidget_ = new MultiViewWidget(mpvWidget_, playerPage_);
        playerStack_->addWidget(multiViewWidget_);  // index 2: 멀티뷰
        // 멀티뷰에서 파일 요청 시 처리
        connect(multiViewWidget_, &MultiViewWidget::secondaryFileRequested,
                this, [this]() { onOpenFile(); });
    }

    // 멀티뷰 레이아웃 설정 및 표시
    multiViewWidget_->setLayout(l);
    playerStack_->setCurrentIndex(2);
    controlBar_->show();  // 멀티뷰에서도 컨트롤바 표시
}

// ── 스마트폰 리모컨 (QTcpServer 기반 HTTP 서버) ──────────────────────────
void MainWindow::startRemoteServer() {
    if (remoteServer_) return;
    remoteServer_ = new QTcpServer(this);
    connect(remoteServer_, &QTcpServer::newConnection, this, [this]() {
        QTcpSocket* socket = remoteServer_->nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
            handleRemoteRequest(socket);
        });
    });
    if (remoteServer_->listen(QHostAddress::Any, 7373)) {
        remoteEnabled_ = true;
        qInfo() << "[Remote] HTTP 리모컨 서버 시작: 포트 7373";
    }
}

void MainWindow::stopRemoteServer() {
    if (remoteServer_) {
        remoteServer_->close();
        remoteServer_->deleteLater();
        remoteServer_ = nullptr;
    }
    remoteEnabled_ = false;
}

void MainWindow::handleRemoteRequest(QTcpSocket* socket) {
    QByteArray req = socket->readAll();
    QString reqStr = QString::fromUtf8(req);
    QString path = reqStr.section(' ', 1, 1).section('?', 0, 0);

    auto* core = mpvWidget_ ? mpvWidget_->core() : nullptr;
    QString body;
    QString contentType = "application/json";

    if (path == "/api/play")        { if (core) core->play();         body = "{\"ok\":true}"; }
    else if (path == "/api/pause")  { if (core) core->pause();        body = "{\"ok\":true}"; }
    else if (path == "/api/toggle") { if (core) core->togglePause();  body = "{\"ok\":true}"; }
    else if (path == "/api/next")   { if (core) core->command({"playlist-next"});  body = "{\"ok\":true}"; }
    else if (path == "/api/prev")   { if (core) core->command({"playlist-prev"});  body = "{\"ok\":true}"; }
    else if (path.startsWith("/api/seek/")) {
        double sec = path.section('/', 3).toDouble();
        if (core) core->seek(sec);
        body = "{\"ok\":true}";
    }
    else if (path.startsWith("/api/volume/")) {
        int vol = path.section('/', 3).toInt();
        if (core) core->setVolume(qBound(0, vol, 200));
        body = "{\"ok\":true}";
    }
    else if (path == "/api/status") {
        double pos = core ? core->position() : 0;
        double dur = core ? core->duration() : 0;
        bool paused = core ? core->isPaused() : true;
        int vol = core ? core->volume() : 0;
        QString title = currentFilePath_.isEmpty() ? "" : QFileInfo(currentFilePath_).fileName();
        body = QString("{\"pos\":%1,\"dur\":%2,\"paused\":%3,\"vol\":%4,\"title\":\"%5\"}")
            .arg(pos, 0, 'f', 1).arg(dur, 0, 'f', 1)
            .arg(paused ? "true" : "false").arg(vol)
            .arg(title.replace('"', '\''));
    }
    else {
        // 기본 리모컨 UI (HTML)
        contentType = "text/html; charset=utf-8";
        body = R"(<!DOCTYPE html><html><head><meta charset='utf-8'>
<meta name='viewport' content='width=device-width,initial-scale=1'>
<title>소리누리 리모컨</title>
<style>
body{background:#111;color:#eee;font-family:sans-serif;text-align:center;padding:20px}
h2{color:#4fc3f7;margin-bottom:24px}
.btn{display:inline-block;background:#1a3a5c;color:#4fc3f7;border:1px solid #4fc3f7;
border-radius:8px;padding:16px 28px;margin:8px;font-size:18px;cursor:pointer;
text-decoration:none;min-width:80px}
.btn:active{background:#0d2a4c}
.row{margin:12px 0}
#status{color:#888;font-size:13px;margin-top:20px}
</style></head><body>
<h2>🎵 소리누리 리모컨</h2>
<div class='row'>
  <a class='btn' href='/api/prev'>⏮</a>
  <a class='btn' href='/api/toggle'>⏯</a>
  <a class='btn' href='/api/next'>⏭</a>
</div>
<div class='row'>
  <a class='btn' href='/api/volume/70'>🔉</a>
  <a class='btn' href='/api/volume/100'>🔊</a>
  <a class='btn' href='/api/volume/130'>🔊+</a>
</div>
<div id='status'>연결됨</div>
<script>
setInterval(()=>{
  fetch('/api/status').then(r=>r.json()).then(d=>{
    document.getElementById('status').textContent=
      (d.paused?'⏸ ':'▶ ')+d.title+' | '+
      Math.floor(d.pos/60)+':'+String(Math.floor(d.pos%60)).padStart(2,'0')+
      ' / '+Math.floor(d.dur/60)+':'+String(Math.floor(d.dur%60)).padStart(2,'0')+
      ' | 볼륨 '+d.vol+'%';
  }).catch(()=>{});
},1000);
</script></body></html>)";
    }

    QString response = QString("HTTP/1.1 200 OK\r\nContent-Type: %1\r\nContent-Length: %2\r\nAccess-Control-Allow-Origin: *\r\nConnection: close\r\n\r\n%3")
        .arg(contentType).arg(body.toUtf8().size()).arg(body);
    socket->write(response.toUtf8());
    socket->flush();
    socket->disconnectFromHost();
    socket->deleteLater();
}
