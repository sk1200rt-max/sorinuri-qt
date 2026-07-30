#include "MainWindow.h"
#include "UrlDialog.h"
#include "ProFeaturesWidget.h"
#include "ShortcutOverlay.h"
#include <QApplication>
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

    // yt-dlp 관리자 초기화
    ytdlp_ = new YtdlpManager(this);
    connect(ytdlp_, &YtdlpManager::ytdlpReady,
            this, &MainWindow::onYtdlpReady);
    connect(ytdlp_, &YtdlpManager::downloadProgress,
            this, &MainWindow::onYtdlpDownloadProgress);
    connect(ytdlp_, &YtdlpManager::downloadFailed,
            this, &MainWindow::onYtdlpDownloadFailed);

    setupUI();
    setupConnections();
    loadSettings();
}

MainWindow::~MainWindow() { saveSettings(); }

void MainWindow::setupUI() {
    auto* central = new QWidget(this);
    central->setStyleSheet("QWidget { background: #0e0e0e; color: #ccc; }");
    setCentralWidget(central);

    auto* mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ── 타이틀바 ─────────────────────────────────────────────
    titleBar_ = new TitleBar(this);
    mainLayout->addWidget(titleBar_);

    // ── 모드 전환 버튼 바 (플레이어 / OTT) ───────────────────────
    auto* modeBar = new QWidget(this);
    modeBar->setFixedHeight(34);
    modeBar->setStyleSheet("background: #0d0d0d; border-bottom: 1px solid #1e1e1e;");
    auto* modeLayout = new QHBoxLayout(modeBar);
    modeLayout->setContentsMargins(8, 3, 8, 3);
    modeLayout->setSpacing(4);

    QString modeBtnStyle =
        "QPushButton { background: #1a1a1a; color: #888; border: 1px solid #2a2a2a; "
        "border-radius: 4px; padding: 3px 14px; font-size: 12px; }"
        "QPushButton:hover { color: #ccc; border-color: #333; }"
        "QPushButton[active=true] { background: #1565c0; color: #fff; border-color: #1976d2; }";

    playerModeBtn_ = new QPushButton("▶  파일 플레이어", modeBar);
    ottModeBtn_    = new QPushButton("🌐  OTT 스트리밍 (Netflix · Disney+ · YouTube)", modeBar);
    playerModeBtn_->setStyleSheet(modeBtnStyle);
    ottModeBtn_->setStyleSheet(modeBtnStyle);
    playerModeBtn_->setProperty("active", true);
    playerModeBtn_->setToolTip("로컈 파일 재생 모드");
    ottModeBtn_->setToolTip("Edge WebView2 기반 OTT 스트리밍\nNetflix Dolby Atmos / 5.1 완전 지원");

    modeLayout->addWidget(playerModeBtn_);
    modeLayout->addWidget(ottModeBtn_);
    modeLayout->addStretch();
    mainLayout->addWidget(modeBar);

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
    videoLayout->addWidget(mpvWidget_);
    // ── 플레이어 페이지 내부: 영상 vs 음악 스택 ────────────────────────────
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

    // ── 오디오 정보 바 ────────────────────────────────────────────
    audioInfoBar_ = new AudioInfoBar(this);
    mainLayout->addWidget(audioInfoBar_);

    // ── 전문 기능 패널 (기본 숨김, P 키로 토글) ──────────────────────
    proFeatures_ = new ProFeaturesWidget(this);
    proFeatures_->hide();
    mainLayout->addWidget(proFeatures_);

    // ── 단축키 오버레이 (영상 위에 표시) ─────────────────────────────
    shortcutOverlay_ = new ShortcutOverlay(mpvWidget_);
    shortcutOverlay_->hide();
}

void MainWindow::setupConnections() {
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

    audioInfoBar_->connectMpv(core);
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

    // 모드 전환 버튼
    connect(playerModeBtn_, &QPushButton::clicked, this, &MainWindow::switchToPlayerMode);
    connect(ottModeBtn_,    &QPushButton::clicked, this, &MainWindow::switchToOttMode);

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
    // 미니 플레이어 연결
    connect(musicPage_, &MusicWidget::miniModeRequested, this, &MainWindow::toggleMiniPlayer);
    // miniPlayer_ 연결은 toggleMiniPlayer에서 처리 (지연 초기화)
    // miniPlayer_ 연결은 toggleMiniPlayer에서 처리 (지연 초기화)
    // miniPlayer_ 연결은 toggleMiniPlayer에서 처리 (지연 초기화)
    // miniPlayer_ 연결은 toggleMiniPlayer에서 처리 (지연 초기화)
    // miniPlayer_ 연결은 toggleMiniPlayer에서 처리 (지연 초기화)
    connect(core, &MpvCore::positionChanged, miniPlayer_, [this](double pos) {
    });
    // AI 자막 연결
    // 지연 초기화 위젯 연결은 P 키 패널 열 때 처리
            this, [this](const QString& text, double start, double end, int conf) {
        Q_UNUSED(start); Q_UNUSED(end);
        // AI 배지 포함 자막 텍스트 구성
        QString badge = (conf >= 90) ? " [AI]" : (conf >= 80 ? " [AI?]" : " [AI!]");
        mpvWidget_->core()->setProperty("sub-text", text + badge);
    });
    // 지연 초기화 위젯 연결은 P 키 패널 열 때 처리
            mpvWidget_->core(), [this](double s){ mpvWidget_->core()->seek(s); });
    // 챕터 연결
    // 지연 초기화 위젯 연결은 P 키 패널 열 때 처리
            mpvWidget_->core(), [this](double s){ mpvWidget_->core()->seek(s); });
    // 지연 초기화 위젯 연결은 P 키 패널 열 때 처리
    connect(core, &MpvCore::fileLoaded, chapterWidget_, [this](const QString& path){
        Q_UNUSED(path);
    // 지연 초기화 위젯 연결은 P 키 패널 열 때 처리
    });
    connect(core, &MpvCore::durationChanged, chapterWidget_, [this](double d){
    // 지연 초기화 위젯 연결은 P 키 패널 열 때 처리
    });

}

void MainWindow::openFiles(const QStringList& paths) {
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
            mpvWidget_->loadFile(path);
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
    audioInfoBar_->hide();
}

void MainWindow::switchToVideoMode() {
    isMusicMode_ = false;
    playerStack_->setCurrentIndex(0);
    controlBar_->show();
    audioInfoBar_->show();
}

void MainWindow::loadMusicMeta(const QString& path) {
    if (!musicPage_) return;
    auto* core = mpvWidget_->core();
    MusicMeta meta;
    meta.codec      = core->getProperty("audio-codec-name").toString();
    meta.sampleRate = core->getProperty("audio-params/samplerate").toInt();
    meta.bitDepth   = 24;
    meta.channels   = core->getProperty("audio-params/channel-count").toInt();
    meta.title      = core->getProperty("media-title").toString();
    meta.artist     = core->getProperty("metadata/by-key/artist").toString();
    meta.album      = core->getProperty("metadata/by-key/album").toString();
    meta.year       = core->getProperty("metadata/by-key/date").toString();
    // 폴더 내 커버 이미지 탐색
    QFileInfo fi(path);
    QDir dir = fi.dir();
    for (const QString& name : {"cover.jpg", "cover.png", "folder.jpg", "albumart.jpg"}) {
        if (dir.exists(name)) { meta.albumArt = QPixmap(dir.filePath(name)); break; }
    }
    musicPage_->loadMeta(meta);
}

void MainWindow::onMusicSeekRequested(double pos) {
    mpvWidget_->core()->seek(pos);
}

void MainWindow::onMusicVolumeChanged(int vol) {
    mpvWidget_->core()->setVolume(vol);
}

void MainWindow::onFileLoaded(const QString& path) {
    updateWindowTitle(QFileInfo(path).fileName());
    if (isMusicMode_) {
        // 음악 모드: 메타데이터 로드 (파일 로드 후 MPV가 태그를 읽은 시점)
        QTimer::singleShot(200, this, [this, path]() { loadMusicMeta(path); });
        musicPage_->setPlaying(true);
    } else {
        controlBar_->setPlaying(true);
    }
}

void MainWindow::onPlaybackStarted() {
    controlBar_->setPlaying(true);
#ifdef Q_OS_WIN
    // 영상/음악 재생 중 화면 켜짐 및 절전모드 진입 방지
    SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED);
#endif
}
void MainWindow::onPlaybackPaused() {
    controlBar_->setPlaying(false);
#ifdef Q_OS_WIN
    // 일시정지 시 절전모드 방지 해제
    SetThreadExecutionState(ES_CONTINUOUS);
#endif
}
void MainWindow::onPlaybackEnded() {
    controlBar_->setPlaying(false);
    mpvWidget_->showLogo(true);
#ifdef Q_OS_WIN
    SetThreadExecutionState(ES_CONTINUOUS);
#endif
}
void MainWindow::onPlaybackStopped() {
    controlBar_->setPlaying(false);
    updateWindowTitle();
    mpvWidget_->showLogo(true);
#ifdef Q_OS_WIN
    SetThreadExecutionState(ES_CONTINUOUS);
#endif
}
void MainWindow::onPositionChanged(double s) { controlBar_->setPosition(s, totalDuration_); }
void MainWindow::onDurationChanged(double s) { totalDuration_ = s; controlBar_->setDuration(s); }
void MainWindow::onVolumeChanged(int v)      { controlBar_->setVolume(v); }

void MainWindow::onAudioFormatChanged(const QString& codec, int, int, const QString&) {
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

void MainWindow::onSettingsRequested() {
    SettingsDialog dlg(mpvWidget_->core(), this);
    dlg.exec();
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

void MainWindow::closeEvent(QCloseEvent* e)  { saveSettings(); e->accept(); }

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
        if (e->modifiers() & Qt::ShiftModifier)  core->seek(-60, true);
        else if (e->modifiers() & Qt::ControlModifier) core->seek(-10, true);
        else core->seek(-5, true);
        break;
    case Qt::Key_Right:
        if (e->modifiers() & Qt::ShiftModifier)  core->seek(60, true);
        else if (e->modifiers() & Qt::ControlModifier) core->seek(10, true);
        else core->seek(5, true);
        break;
    case Qt::Key_Up:     core->setVolume(qMin(core->volume() + 5, 200)); break;
    case Qt::Key_Down:   core->setVolume(qMax(core->volume() - 5, 0)); break;
    case Qt::Key_PageUp:   core->seek(-300, true); break;  // 5분 앞으로
    case Qt::Key_PageDown: core->seek(300, true);  break;  // 5분 뒤로
    case Qt::Key_Home:   core->seek(0); break;  // 첫 장면으로
    case Qt::Key_End:    core->seek(100, false); break;  // 마지막으로
    case Qt::Key_M:      core->setMuted(!core->getProperty("mute").toBool()); break;
    // 재생목록 이전/다음
    case Qt::Key_N:      core->command({"playlist-next"}); break;
    case Qt::Key_BracketLeft:   // [ 이전 챕터
        core->command({"add", "chapter", "-1"});
        break;
    case Qt::Key_BracketRight:  // ] 다음 챕터
        core->command({"add", "chapter", "1"});
        break;
    case Qt::Key_O:
        if (e->modifiers() & Qt::ControlModifier) onOpenFile();
        break;
    // ── 전문 기능 단축키 ────────────────────────────────────────────
    case Qt::Key_A:
        if (e->modifiers() & Qt::ControlModifier)
            proFeatures_->clearAbLoop();
        else
            proFeatures_->setAbPointA();
        break;
    case Qt::Key_B:
        proFeatures_->setAbPointB();
        break;
    case Qt::Key_S:
        proFeatures_->takeScreenshot();
        break;
    case Qt::Key_D:
        if (e->modifiers() & Qt::ShiftModifier)
            proFeatures_->setAudioDelay(proFeatures_->audioDelay() - 100);
        else
            proFeatures_->setAudioDelay(proFeatures_->audioDelay() + 100);
        break;
    case Qt::Key_Z:
        if (e->modifiers() & Qt::ShiftModifier)
            proFeatures_->setSubDelay(proFeatures_->subDelay() - 100);
        else
            proFeatures_->setSubDelay(proFeatures_->subDelay() + 100);
        break;
    case Qt::Key_Greater:  // >
        proFeatures_->setSpeed(qMin(proFeatures_->currentSpeed() + 0.25, 4.0));
        break;
    case Qt::Key_Less:     // <
        proFeatures_->setSpeed(qMax(proFeatures_->currentSpeed() - 0.25, 0.25));
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
        if (m->message == WM_NCHITTEST) {
            QPoint pos = mapFromGlobal(QPoint(GET_X_LPARAM(m->lParam), GET_Y_LPARAM(m->lParam)));
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

void MainWindow::mouseMoveEvent(QMouseEvent* e) {
    if (!resizing_) {
        static const Qt::CursorShape cs[] = {
            Qt::ArrowCursor, Qt::SizeHorCursor, Qt::SizeHorCursor,
            Qt::SizeVerCursor, Qt::SizeVerCursor,
            Qt::SizeFDiagCursor, Qt::SizeBDiagCursor,
            Qt::SizeBDiagCursor, Qt::SizeFDiagCursor
        };
        setCursor(cs[getResizeEdge(e->pos())]);
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
    QAction* actUrl = menu.addAction("▶  URL 열기... (YouTube/스트리밍)");
    actUrl->setShortcut(QKeySequence("Ctrl+U"));
    connect(actUrl, &QAction::triggered, this, &MainWindow::onOpenUrl);

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

    // 화면 크기
    QMenu* sizeMenu = menu.addMenu("화면 크기");
    QAction* act50  = sizeMenu->addAction("50%");
    QAction* act100 = sizeMenu->addAction("100%");
    QAction* act150 = sizeMenu->addAction("150%");
    QAction* act200 = sizeMenu->addAction("200%");
    connect(act50,  &QAction::triggered, [this, core]() {
        int w = core->getProperty("video-params/w").toInt();
        int h = core->getProperty("video-params/h").toInt();
        if (w>0 && h>0) resize(w/2, h/2 + 120);
    });
    connect(act100, &QAction::triggered, [this, core]() {
        int w = core->getProperty("video-params/w").toInt();
        int h = core->getProperty("video-params/h").toInt();
        if (w>0 && h>0) resize(w, h + 120);
    });
    connect(act150, &QAction::triggered, [this, core]() {
        int w = core->getProperty("video-params/w").toInt();
        int h = core->getProperty("video-params/h").toInt();
        if (w>0 && h>0) resize(w*3/2, h*3/2 + 120);
    });
    connect(act200, &QAction::triggered, [this, core]() {
        int w = core->getProperty("video-params/w").toInt();
        int h = core->getProperty("video-params/h").toInt();
        if (w>0 && h>0) resize(w*2, h*2 + 120);
    });

    // 전체화면
    QAction* actFull = menu.addAction(isFullscreen_ ? "전체화면 해제" : "전체화면");
    actFull->setShortcut(QKeySequence("F"));
    connect(actFull, &QAction::triggered, this, &MainWindow::toggleFullscreen);

    menu.addSeparator();

    // 설정
    QAction* actSettings = menu.addAction("환경 설정...");
    actSettings->setShortcut(QKeySequence("F5"));
    connect(actSettings, &QAction::triggered, this, &MainWindow::onSettingsRequested);

    menu.addSeparator();

    // 종료
    QAction* actQuit = menu.addAction("종료");
    actQuit->setShortcut(QKeySequence("Alt+F4"));
    connect(actQuit, &QAction::triggered, this, &QMainWindow::close);

    menu.exec(globalPos);
}

void MainWindow::switchToPlayerMode() {
    isOttMode_ = false;
    mainStack_->setCurrentIndex(0);
    playerModeBtn_->setProperty("active", true);
    ottModeBtn_->setProperty("active", false);
    // Qt 스타일 재적용 (property 변경 후 필요)
    playerModeBtn_->style()->unpolish(playerModeBtn_);
    playerModeBtn_->style()->polish(playerModeBtn_);
    ottModeBtn_->style()->unpolish(ottModeBtn_);
    ottModeBtn_->style()->polish(ottModeBtn_);
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
    playerModeBtn_->setProperty("active", false);
    ottModeBtn_->setProperty("active", true);
    playerModeBtn_->style()->unpolish(playerModeBtn_);
    playerModeBtn_->style()->polish(playerModeBtn_);
    ottModeBtn_->style()->unpolish(ottModeBtn_);
    ottModeBtn_->style()->polish(ottModeBtn_);
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
    resize(settings_.value("window/size", QSize(1280, 760)).toSize());
    QPoint p = settings_.value("window/pos", QPoint(-1,-1)).toPoint();
    if (p.x() >= 0) move(p);

    // 항상 위에 고정 상태 복원
    bool alwaysOnTop = settings_.value("window/alwaysOnTop", false).toBool();
    if (alwaysOnTop) {
        setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
        titleBar_->setAlwaysOnTop(true);
    }
    int vol = settings_.value("audio/volume", 100).toInt();
    mpvWidget_->core()->setVolume(vol);
    controlBar_->setVolume(vol);
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
    if (!miniPlayer_ || !isMusicMode_) return;
    if (miniPlayer_->isVisible()) {
        miniPlayer_->hide();
        show();
        activateWindow();
    } else {
        // 미니 플레이어에 현재 메타 정보 전달
        if (musicPage_) {
            // 미니 플레이어 업데이트는 loadMusicMeta에서 처리
        }
        hide();
        miniPlayer_->show();
        miniPlayer_->raise();
    }
}

void MainWindow::toggleWhisper(bool on) {
    if (whisperWidget_) {
        whisperWidget_->setMediaFile(currentFilePath_);
        whisperWidget_->setActive(on);
    }
}

void MainWindow::onChapterBookmark() {
    if (chapterWidget_ && mpvWidget_) {
        double pos = mpvWidget_->core()->getProperty("time-pos").toDouble();
        chapterWidget_->addBookmark(pos);
    }
}

void MainWindow::toggleMultiView(MultiViewLayout l) {
    Q_UNUSED(l);
    // MultiViewWidget은 별도 창으로 구현 예정
}
