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
#include <QApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QEvent>
#include <QMessageBox>
#include <QProgressDialog>
#include <QFileDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <QMimeData>
#include <QUrl>
#include <QMenu>
#include <QAction>
#include <QCryptographicHash>
#include <QDateTime>

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
    setMinimumSize(800, 540);
    setAcceptDrops(true);
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);

    // yt-dlp 관리자 - 지연 초기화 (시작 직후 네트워크 요청 방지)
    ytdlp_ = nullptr;

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

void MainWindow::setupUI() {
    // 키 이벤트가 항상 MainWindow로 전달되도록 포커스 정책 설정
    setFocusPolicy(Qt::StrongFocus);

    auto* central = new QWidget(this);
    central->setStyleSheet("QWidget { background: #0e0e0e; color: #ccc; }");
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
    musicPage_ = new MusicWidget(mpvWidget_->core(), playerPage_);
    playerStack_->addWidget(musicPage_);      // index 1: 음악
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

    // ── 전문 기능 패널 (기본 숨김, P 키로 토글) ──────────────────────
    proFeatures_ = new ProFeaturesWidget(this);
    proFeatures_->hide();
    mainLayout->addWidget(proFeatures_);

    // ── 단축키 오버레이 (영상 위에 표시) ─────────────────────────────
    shortcutOverlay_ = new ShortcutOverlay(mpvWidget_);
    shortcutOverlay_->hide();

    // OSD 위젯 (화면 중앙 반투명 표시) - mpvWidget_ 위에 오버레이
    osdWidget_ = new OsdWidget(mpvWidget_);
    osdWidget_->hide();

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

void MainWindow::setupConnections() {
    // MpvWidget의 마우스 이벤트를 MainWindow가 가로체 → UI 자동 숨김/표시 동작
    mpvWidget_->setMouseTracking(true);
    mpvWidget_->installEventFilter(this);
    setMouseTracking(true);

    // HiDPI 근본 수정: mpvInitialized 시그널 연결
    // initializeGL() 완료 후 pendingStartupFiles_ 자동 처리
    // window.show() 직후 openFiles() 호출 시 initialized_=false로 무시되던 문제 근본 해결
    // Qt::SingleShotConnection: 한 번만 실행 (initializeGL은 한 번만 호출됨)
    connect(mpvWidget_, &MpvWidget::mpvInitialized, this, [this]() {
        if (!pendingStartupFiles_.isEmpty()) {
            qInfo() << "[MainWindow] mpvInitialized: pendingStartupFiles_ 처리" << pendingStartupFiles_;
            QStringList files = pendingStartupFiles_;
            pendingStartupFiles_.clear();
            openFiles(files);
        }
    }, Qt::SingleShotConnection);

    auto* core = mpvWidget_->core();

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

    // 전문 기능 패널 연결
    proFeatures_->connectMpv(core);
    // 패널 내부 닫기 버튼 → toggleProFeatures (앱 종료 아님)
    connect(proFeatures_, &ProFeaturesWidget::closeRequested,
            this, &MainWindow::toggleProFeatures);

    connect(titleBar_, &TitleBar::minimizeClicked,   this, &QMainWindow::showMinimized);
    connect(titleBar_, &TitleBar::maximizeClicked,   [this]() { isMaximized() ? showNormal() : showMaximized(); });
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

    // 음악 모드 MusicWidget 시그널 연결
    connect(musicPage_, &MusicWidget::seekRequested,    this, &MainWindow::onMusicSeekRequested);
    connect(musicPage_, &MusicWidget::volumeChanged,    this, &MainWindow::onMusicVolumeChanged);
    connect(musicPage_, &MusicWidget::playPauseRequested, core, &MpvCore::togglePause);
    connect(musicPage_, &MusicWidget::prevRequested,    [core]() { core->command({"playlist-prev"}); });
    connect(musicPage_, &MusicWidget::nextRequested,    [core]() { core->command({"playlist-next"}); });
    connect(musicPage_, &MusicWidget::eqRequested,      this, &MainWindow::onSettingsRequested);
    connect(musicPage_, &MusicWidget::settingsRequested, this, &MainWindow::onSettingsRequested);
    // 음악 모드에서도 위치/재생 상태 업데이트
    connect(core, &MpvCore::positionChanged, musicPage_, [this](double pos) {
        musicPage_->updatePosition(pos, totalDuration_);
    });
    connect(core, &MpvCore::playbackStarted, musicPage_, [this]() { musicPage_->setPlaying(true); });
    connect(core, &MpvCore::playbackPaused,  musicPage_, [this]() { musicPage_->setPlaying(false); });
    // 비트퍼펙트 상태 실시간 표시: 실제 AO 출력 경로 조회 후 음악 화면에 반영
    connect(core, &MpvCore::audioFormatChanged, musicPage_,
            [this, core](const QString&, int, int, const QString&) {
        if (!isMusicMode_) return;
        const int outRate = core->getProperty("audio-out-params/samplerate").toInt();
        const QString outFmt = core->getProperty("audio-out-params/format").toString();
        const bool exclusive = core->getProperty("audio-exclusive").toBool();
        if (outRate > 0)
            musicPage_->setOutputInfo(outRate, outFmt, exclusive);
    });
    // 미니 플레이어 연결
    connect(musicPage_, &MusicWidget::miniModeRequested, this, &MainWindow::toggleMiniPlayer);
    // 소형 모드 (코팩트 플레이어) 연결
    connect(musicPage_, &MusicWidget::compactModeRequested, this, &MainWindow::toggleCompactPlayer);
    // 셔플/반복 시그널 연결 - MPV 속성 직접 적용
    connect(musicPage_, &MusicWidget::shuffleToggled, this, [core](bool on) {
        core->setProperty("shuffle", on ? QString("yes") : QString("no"));
    });
    connect(musicPage_, &MusicWidget::repeatToggled, this, [core](bool on) {
        core->setProperty("loop-file", on ? QString("inf") : QString("no"));
    });
    // miniPlayer_, whisperWidget_, chapterWidget_ 연결은 지연 초기화
    // (toggleMiniPlayer, onProFeaturesRequested에서 처음 생성 시 연결)
}

void MainWindow::openFiles(const QStringList& paths) {
    // HiDPI 근본 수정: MPV 초기화 전이면 pendingStartupFiles_에 저장
    // initializeGL()이 완료되면 mpvInitialized 시그널이 발생하고
    // setupConnections()의 슬롯에서 이 큐를 처리함
    if (!mpvWidget_->isMpvInitialized()) {
        qInfo() << "[MainWindow] openFiles: MPV 초기화 전 → pendingStartupFiles_에 저장" << paths;
        pendingStartupFiles_ = paths;
        return;
    }

    bool first = true;
    for (const QString& path : paths) {
        QFileInfo fi(path);
        if (!fi.exists()) continue;
        if (first) {
            currentFilePath_ = path;
            // 음악 파일이면 자동으로 HiFi 모드로 전환
            if (HiFiEngine::isMusicFile(path)) {
                switchToMusicMode();
                hifiEngine_->applyAll();
            } else {
                switchToVideoMode();
            }
            // switchToVideoMode/MusicMode에서 QOpenGLWidget 컨텍스트가 재배치될 수 있음.
            // QTimer::singleShot(0)으로 현재 이벤트 루프 사이클이 끝난 후 loadFile 호출.
            // → 컨텍스트 메뉴 닫힌 처리가 완전히 끝난 후 실행 보장.
            // 0ms: MPV 초기화 완료 후 호출되므로 타이밍 의존 불필요.
            const QString pathCopy = path;
            QTimer::singleShot(0, this, [this, pathCopy]() {
                mpvWidget_->loadFile(pathCopy);
            });
            first = false;
        } else {
            mpvWidget_->appendFile(path);
        }
    }
}

void MainWindow::switchToMusicMode() {
    isMusicMode_ = true;
    playerStack_->setCurrentIndex(1);
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
}

void MainWindow::switchToVideoMode() {
    isMusicMode_ = false;
    playerStack_->setCurrentIndex(0);
    controlBar_->show();
}

void MainWindow::loadMusicMeta(const QString& path) {
    if (!musicPage_) return;
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
    lastPosition_ = 0.0;
    // 새 파일 로드 시 멀티체널 안내 상태 초기화 (파일마다 한 번씩 감지)
    multichannelPromptShown_ = false;
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

void MainWindow::showUI() {
    if (!uiVisible_) {
        if (isMusicMode_) {
            // 음악 모드: 타이틀바만 표시, ControlBar는 절대 표시하지 않음
            // (MusicWidget 내부에 자체 시크바가 있으므로 중복 방지)
            titleBar_->show();
        } else if (isFullscreen_) {
            // 영상 전체화면: 타이틀바 숨김 유지, 컨트롤바만 표시
            controlBar_->show();
        } else {
            // 영상 창 모드: 타이틀바+컨트롤바 모두 표시
            titleBar_->show();
            controlBar_->show();
        }
        uiVisible_ = true;
    }
    // 마우스 커서 복원 (음악 모드 포함 항상 복원)
    if (cursor().shape() == Qt::BlankCursor)
        unsetCursor();
    mpvWidget_->unsetCursor();
    if (musicPage_) musicPage_->unsetCursor();
    // 재생 중 마우스 움직임이 있으면 3초 후 숨김 타이머 재시작
    // 음악 모드에서는 타이머 비활성화 (커서/타이틀바 숨기지 않음)
    if (isPlaying_ && !isMusicMode_ && uiHideTimer_) {
        uiHideTimer_->start(3000);
    }
}

void MainWindow::hideUI() {
    // 음악 모드에서는 타이틀바/커서를 절대 숨기지 않음
    // (음악 청취 중 타이틀바와 마우스 커서는 항상 보여야 함)
    if (isMusicMode_) return;

    if (uiVisible_ && isPlaying_) {
        if (isFullscreen_) {
            // 영상 전체화면: 컨트롤바만 숨김
            controlBar_->hide();
        } else {
            // 영상 창 모드: 타이틀바+컨트롤바 모두 숨김
            titleBar_->hide();
            controlBar_->hide();
        }
        uiVisible_ = false;
        // 마우스 커서 숨김 (영상 모드에서만)
        setCursor(Qt::BlankCursor);
        mpvWidget_->setCursor(Qt::BlankCursor);
    }
}

void MainWindow::onPlaybackStarted() {
    if (!isMusicMode_) controlBar_->setPlaying(true);
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
    controlBar_->setPlaying(false);
    // 일시정지 시 UI 항상 표시
    if (uiHideTimer_) uiHideTimer_->stop();
    showUI();
#ifdef Q_OS_WIN
    SetThreadExecutionState(ES_CONTINUOUS);
#endif
}
void MainWindow::onPlaybackEnded() {
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
    if (uiHideTimer_) uiHideTimer_->stop();
    showUI();
    if (!isMusicMode_) {
        controlBar_->setPlaying(false);
        mpvWidget_->showLogo(true);
    } else {
        musicPage_->setPlaying(false);
    }
    updateWindowTitle();
#ifdef Q_OS_WIN
    SetThreadExecutionState(ES_CONTINUOUS);
#endif
}
void MainWindow::onPositionChanged(double s) {
    controlBar_->setPosition(s, totalDuration_);
    lastPosition_ = s;  // 이어보기용 현재 위치 추적
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

void MainWindow::onAudioFormatChanged(const QString& codec, int channelCount, int, const QString&) {
    titleBar_->setAudioBadge(codec.toUpper());

    // 멀티채널 파일 감지: 공유 모드에서 5.1 이상인 경우 안내
    // QSettings 대신 MPV audio-exclusive 속성 직접 확인 (실제 상태 반영)
    if (channelCount >= 6 && !multichannelPromptShown_) {
        auto* core = mpvWidget_->core();
        // MPV에서 현재 실제 독점 모드 여부 직접 확인
        const bool isExclusive = core->getProperty("audio-exclusive").toString() == "yes";
        if (!isExclusive) {
            multichannelPromptShown_ = true;
            QMetaObject::invokeMethod(this, [this, channelCount]() {
                showMultichannelPrompt(channelCount);
            }, Qt::QueuedConnection);
        }
    }
}

void MainWindow::showMultichannelPrompt(int channelCount) {
    const QString chStr = (channelCount == 6) ? "5.1" :
                          (channelCount == 8) ? "7.1" :
                          QString("%1ch").arg(channelCount);

    QDialog dlg(this);
    dlg.setWindowTitle("멀티체널 오디오 감지");
    dlg.setFixedSize(460, 230);
    dlg.setStyleSheet(
        "QDialog { background:#1a1a1a; color:#e0e0e0; "
        "font-family:'Segoe UI','Malgun Gothic',sans-serif; font-size:13px; }"
        "QLabel { color:#e0e0e0; }"
        "QPushButton { background:#252525; border:1px solid #444; border-radius:5px; "
        "  padding:8px 18px; color:#e0e0e0; font-size:13px; min-width:100px; }"
        "QPushButton:hover { background:#2a2a2a; }"
        "QPushButton#btnExclusive { background:#00443a; border-color:#00c8b4; "
        "  color:#00c8b4; font-weight:bold; }"
        "QPushButton#btnExclusive:hover { background:#005a4d; }");

    QVBoxLayout* vl = new QVBoxLayout(&dlg);
    vl->setContentsMargins(24, 20, 24, 20);
    vl->setSpacing(12);

    QLabel* title = new QLabel(
        QString("🔊  %1 서라운드 파일이 감지되었습니다").arg(chStr), &dlg);
    title->setStyleSheet("font-size:15px; font-weight:bold; color:#00c8b4;");
    vl->addWidget(title);

    QLabel* msg = new QLabel(
        QString("현재 <b>공유 모드</b>로 재생 중입니다.<br>"
                "%1 서라운드를 완벽하게 듣려면 <b>독점 모드</b>로 전환하세요.<br>"
                "<span style='color:#888;font-size:11px;'>"
                "⚠️ 독점 모드 전환 시 다른 앱의 소리가 일시적으로 차단될 수 있습니다."
                "</span>").arg(chStr), &dlg);
    msg->setWordWrap(true);
    msg->setTextFormat(Qt::RichText);
    vl->addWidget(msg);

    vl->addStretch();

    QHBoxLayout* hl = new QHBoxLayout();
    hl->setSpacing(10);
    QPushButton* btnKeep      = new QPushButton("공유 모드 유지", &dlg);
    QPushButton* btnExclusive = new QPushButton("🔊  독점 모드로 전환", &dlg);
    btnExclusive->setObjectName("btnExclusive");
    hl->addStretch();
    hl->addWidget(btnKeep);
    hl->addWidget(btnExclusive);
    vl->addLayout(hl);

    // 독점 모드 전환: QSettings 저장 + MPV 즉시 적용 (재시작 불필요)
    connect(btnExclusive, &QPushButton::clicked, &dlg, [this, &dlg]() {
        QSettings s("Sorinuri", "SorinuriPlayer");
        s.setValue("audio/exclusive", true);
        s.sync();
        mpvWidget_->core()->setAudioExclusive(true);
        dlg.accept();
    });
    connect(btnKeep, &QPushButton::clicked, &dlg, &QDialog::reject);

    dlg.exec();
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
    switch (e->key()) {
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
    case Qt::Key_E:  // 자막 폰트 크기 감소
        core->command({"add", "sub-scale", "-0.1"});
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
            core->command({"screenshot", "video"});
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
        core->command({"screenshot", "video"});
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
    isProFeaturesOpen_ = !isProFeaturesOpen_;

    // 처음 열 때 지연 초기화 위젯 생성 및 탭 추가
    if (isProFeaturesOpen_) {
        auto* core = mpvWidget_->core();

        if (!whisperWidget_) {
            whisperWidget_ = new WhisperWidget(this);
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
            upscaleWidget_ = new UpscaleWidget(core, this);
            proFeatures_->addTab(upscaleWidget_, "화질 개선");
        }

        if (!chapterWidget_) {
            chapterWidget_ = new ChapterWidget(core, this);
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
            audioAdvancedWidget_ = new AudioAdvancedWidget(core, this);
            proFeatures_->addTab(audioAdvancedWidget_, "하이엔드 오디오");
        }
        // 하이엔드 비디오 고급 기능 (3D LUT 케리브레이션)
        if (!videoAdvancedWidget_) {
            videoAdvancedWidget_ = new VideoAdvancedWidget(core, this);
            proFeatures_->addTab(videoAdvancedWidget_, "하이엔드 비디오");
        }
        // 네트워크 탐색 (SMB/NAS + 360도 VR + 캐스팅)
        if (!networkBrowserWidget_) {
            networkBrowserWidget_ = new NetworkBrowserWidget(core, this);
            proFeatures_->addTab(networkBrowserWidget_, "네트워크");
            // SMB 파일 열기 요청 연결
            connect(networkBrowserWidget_, &NetworkBrowserWidget::openFileRequested,
                    this, [this](const QString& path) { openFiles({path}); });
        }
        // 스마트 미디어 라이브러리
        if (!mediaLibrary_) {
            mediaLibrary_ = new MediaLibraryWidget(this);
            proFeatures_->addTab(mediaLibrary_, "미디어 라이브러리");
            connect(mediaLibrary_, &MediaLibraryWidget::fileRequested,
                    this, [this](const QString& path) { openFiles({path}); });
        }
        if (!subtitleEditor_) {
            subtitleEditor_ = new SubtitleEditorWidget(mpvWidget_->core(), this);
            proFeatures_->addTab(subtitleEditor_, "자막 편집기");
        }
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
        }
        if (event->type() == QEvent::MouseMove) {
            auto* me = static_cast<QMouseEvent*>(event);
            showUI();
            // 전체화면: 상단 60px 영역에 마우스 오면 타이틀바 일시 표시
            if (isFullscreen_) {
                if (me->pos().y() <= 60) {
                    titleBar_->show();
                } else {
                    if (!uiVisible_) titleBar_->hide();
                }
            }
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
    // 마우스 움직임 시 UI 표시 (3초 후 다시 숨김)
    showUI();
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
    menu.setStyleSheet(
        "QMenu { background: #1a1a1a; color: #ddd; border: 1px solid #333; "
        "font-size: 12px; padding: 4px 0; } "
        "QMenu::item { padding: 6px 24px 6px 16px; } "
        "QMenu::item:selected { background: #2a2a2a; color: #fff; } "
        "QMenu::item:disabled { color: #555; } "
        "QMenu::separator { height: 1px; background: #2a2a2a; margin: 3px 8px; }"
    );

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
    connect(actPrev, &QAction::triggered, [core]() { core->command({"playlist-prev"}); });

    QAction* actNext = menu.addAction("다음");
    connect(actNext, &QAction::triggered, [core]() { core->command({"playlist-next"}); });

    menu.addSeparator();

    // 오디오 트랙 서브메뉴
    QMenu* audioMenu = menu.addMenu("오디오 트랙");
    QVariantList audioTracks = core->audioTracks();
    if (audioTracks.isEmpty()) {
        audioMenu->addAction("트랙 없음")->setEnabled(false);
    } else {
        for (const QVariant& t : audioTracks) {
            QVariantMap m = t.toMap();
            QString label = QString("[%1] %2").arg(m["id"].toInt()).arg(m["lang"].toString());
            if (!m["title"].toString().isEmpty()) label += " - " + m["title"].toString();
            if (!m["codec"].toString().isEmpty()) label += " (" + m["codec"].toString() + ")";
            QAction* a = audioMenu->addAction(label);
            int id = m["id"].toInt();
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
    QAction* actSubOff = subMenu->addAction("자막 끄기");
    connect(actSubOff, &QAction::triggered, [core]() { core->setSubtitleTrack(0); });
    subMenu->addSeparator();
    QVariantList subTracks = core->subtitleTracks();
    for (const QVariant& t : subTracks) {
        QVariantMap m = t.toMap();
        QString label = QString("[%1] %2").arg(m["id"].toInt()).arg(m["lang"].toString());
        if (!m["title"].toString().isEmpty()) label += " - " + m["title"].toString();
        QAction* a = subMenu->addAction(label);
        int id = m["id"].toInt();
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
    connect(actCapture, &QAction::triggered, this, [core]() {
        // MPV screenshot 명령 - 실행 파일 디렉토리에 PNG 저장
        core->command({"screenshot", "video"});
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
    mpvWidget_->core()->loadFile(url, false);
    updateWindowTitle(url);
}

void MainWindow::onYtdlpReady(const QString& path) {
    Q_UNUSED(path)
    if (ytdlpProgress_) {
        ytdlpProgress_->close();
        ytdlpProgress_->deleteLater();
        ytdlpProgress_ = nullptr;
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

    // 화면 크기의 60%를 기본 크기로 설정
    QSize defaultSize(availSize.width() * 6 / 10, availSize.height() * 6 / 10);

    // 저장된 창 크기 로드 - 단, 화면보다 크거나 화면 밖에 있으면 자동 재설정
    QSize savedSize = settings_.value("window/size", QSize()).toSize();
    QPoint savedPos  = settings_.value("window/pos",  QPoint(-1,-1)).toPoint();

    bool needReset = false;
    if (!savedSize.isValid() || savedSize.isEmpty()) {
        needReset = true;  // 저장된 값 없음
    } else if (savedSize.width()  > availSize.width() ||
               savedSize.height() > availSize.height()) {
        needReset = true;  // 화면보다 큼서 하단바 접근 불가
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
    // audio-exclusive 및 passthrough: 기본값 true (Exclusive 모드 기본)
    // 실패 시 audio-fallback-to-null으로 영상 재생 보장
    if (settings_.value("audio/exclusive", true).toBool())
        mpvWidget_->core()->setAudioExclusive(true);
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
