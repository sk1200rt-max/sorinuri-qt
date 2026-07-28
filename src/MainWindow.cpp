#include "MainWindow.h"
#include <QApplication>
#include <QFileDialog>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QDebug>
#include <QFileInfo>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>

static const QStringList VIDEO_EXTS = {
    "mkv","mp4","avi","mov","wmv","flv","ts","m2ts","m4v",
    "webm","ogv","3gp","rmvb","rm","divx","xvid","hevc","h265"
};
static const QStringList AUDIO_EXTS = {
    "mp3","flac","aac","ogg","wav","wma","m4a","opus","ape","dts","ac3","truehd"
};

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , settings_("Sorinuri", "SorinuriPlayer")
{
    setWindowTitle("소리누리");
    setMinimumSize(960, 640);
    setAcceptDrops(true);
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);

    setupUI();
    setupConnections();
    loadSettings();
}

MainWindow::~MainWindow() {
    saveSettings();
}

void MainWindow::setupUI() {
    QWidget* central = new QWidget(this);
    setCentralWidget(central);

    QVBoxLayout* mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ── 커스텀 타이틀바 ──────────────────────────────────────────
    titleBar_ = new TitleBar(this);
    mainLayout->addWidget(titleBar_);

    // ── 메인 영역 (사이드바 + 비디오) ────────────────────────────
    splitter_ = new QSplitter(Qt::Horizontal, this);
    splitter_->setHandleWidth(1);
    splitter_->setStyleSheet("QSplitter::handle { background: #222; }");

    playlist_ = new PlaylistWidget(this);
    playlist_->setMinimumWidth(200);
    playlist_->setMaximumWidth(320);
    splitter_->addWidget(playlist_);

    // 비디오 + 통계 오버레이 컨테이너
    QWidget* videoContainer = new QWidget(this);
    videoContainer->setStyleSheet("background: #000;");
    QVBoxLayout* vcLayout = new QVBoxLayout(videoContainer);
    vcLayout->setContentsMargins(0, 0, 0, 0);
    vcLayout->setSpacing(0);

    mpvWidget_ = new MpvWidget(videoContainer);
    mpvWidget_->setMinimumSize(400, 300);
    vcLayout->addWidget(mpvWidget_, 1);

    splitter_->addWidget(videoContainer);
    splitter_->setStretchFactor(0, 0);
    splitter_->setStretchFactor(1, 1);
    splitter_->setSizes({240, 1000});

    mainLayout->addWidget(splitter_, 1);

    // ── 통계 오버레이 (S키로 토글) ───────────────────────────────
    statsOverlay_ = new QLabel(mpvWidget_);
    statsOverlay_->setStyleSheet(R"(
        QLabel {
            background: rgba(0,0,0,180);
            color: #00e576;
            font-family: 'Consolas', 'Courier New', monospace;
            font-size: 12px;
            padding: 10px 14px;
            border-radius: 4px;
        }
    )");
    statsOverlay_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    statsOverlay_->hide();
    statsOverlay_->setAttribute(Qt::WA_TransparentForMouseEvents);

    statsTimer_ = new QTimer(this);
    statsTimer_->setInterval(500);
    connect(statsTimer_, &QTimer::timeout, [this]() {
        if (!statsOverlay_->isVisible()) return;
        MpvCore* core = mpvWidget_->core();
        QString stats;
        stats += QString("파일: %1\n").arg(core->currentFile().split('/').last());
        stats += QString("위치: %1 / %2\n")
            .arg(formatTime(core->position()))
            .arg(formatTime(core->duration()));
        stats += QString("A/V Sync: %1 ms\n")
            .arg(static_cast<int>(core->getProperty("avsync").toDouble() * 1000));
        stats += QString("드롭 프레임: %1\n")
            .arg(core->getProperty("frame-drop-count").toInt());
        stats += QString("비디오 비트레이트: %1 kbps\n")
            .arg(static_cast<int>(core->getProperty("video-bitrate").toDouble() / 1000));
        stats += QString("오디오 비트레이트: %1 kbps\n")
            .arg(static_cast<int>(core->getProperty("audio-bitrate").toDouble() / 1000));
        stats += QString("HW 디코딩: %1\n")
            .arg(core->getProperty("hwdec-current").toString());
        stats += QString("캐시: %1 s")
            .arg(core->getProperty("demuxer-cache-duration").toDouble(), 0, 'f', 1);
        statsOverlay_->setText(stats);
        statsOverlay_->adjustSize();
        statsOverlay_->move(10, 10);
    });

    // ── 트랙 선택 바 ─────────────────────────────────────────────
    QWidget* trackBar = new QWidget(this);
    trackBar->setFixedHeight(36);
    trackBar->setStyleSheet("background: #0d0d0d; border-top: 1px solid #1e1e1e;");
    QHBoxLayout* trackLayout = new QHBoxLayout(trackBar);
    trackLayout->setContentsMargins(8, 0, 8, 0);

    trackSelector_ = new TrackSelector(this);
    trackLayout->addWidget(trackSelector_);
    trackLayout->addStretch();

    // 설정 버튼
    QPushButton* settingsBtn = new QPushButton("⚙ 설정", this);
    settingsBtn->setStyleSheet(R"(
        QPushButton {
            background: #1e1e1e;
            border: 1px solid #333;
            border-radius: 3px;
            padding: 4px 12px;
            color: #888;
            font-size: 12px;
        }
        QPushButton:hover { color: #4fc3f7; border-color: #4fc3f7; }
    )");
    connect(settingsBtn, &QPushButton::clicked, this, &MainWindow::onSettingsRequested);
    trackLayout->addWidget(settingsBtn);

    mainLayout->addWidget(trackBar);

    // ── 컨트롤바 ─────────────────────────────────────────────────
    controlBar_ = new ControlBar(this);
    mainLayout->addWidget(controlBar_);

    // ── 오디오 정보 바 (하단) ────────────────────────────────────
    audioInfoBar_ = new AudioInfoBar(this);
    mainLayout->addWidget(audioInfoBar_);

    // ── 전체 스타일 ───────────────────────────────────────────────
    central->setStyleSheet(R"(
        QWidget {
            background: #141414;
            color: #e0e0e0;
            font-family: 'Segoe UI', 'Malgun Gothic', sans-serif;
            font-size: 13px;
        }
    )");
}

void MainWindow::setupConnections() {
    MpvCore* core = mpvWidget_->core();

    // MPV → UI
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
    connect(core, &MpvCore::errorOccurred,      this, &MainWindow::onErrorOccurred);

    // AudioInfoBar 연결
    audioInfoBar_->connectMpv(core);

    // TrackSelector 연결
    trackSelector_->connectMpv(core);

    // 컨트롤바 → MPV
    connect(controlBar_, &ControlBar::playPauseClicked,  core, &MpvCore::togglePause);
    connect(controlBar_, &ControlBar::stopClicked,       core, &MpvCore::stop);
    connect(controlBar_, &ControlBar::seeked,            [core](double pos) { core->seek(pos); });
    connect(controlBar_, &ControlBar::volumeChanged,     core, &MpvCore::setVolume);
    connect(controlBar_, &ControlBar::muteToggled,       core, &MpvCore::setMuted);
    connect(controlBar_, &ControlBar::speedChanged,      core, &MpvCore::setSpeed);
    connect(controlBar_, &ControlBar::openFileClicked,   this, &MainWindow::onOpenFile);
    connect(controlBar_, &ControlBar::fullscreenToggled, this, &MainWindow::toggleFullscreen);

    // 재생목록
    connect(playlist_, &PlaylistWidget::itemDoubleClicked,
            this, &MainWindow::onPlaylistItemDoubleClicked);
    connect(playlist_, &PlaylistWidget::openFileRequested,
            this, &MainWindow::onOpenFile);

    // 타이틀바
    connect(titleBar_, &TitleBar::minimizeClicked, this, &QMainWindow::showMinimized);
    connect(titleBar_, &TitleBar::maximizeClicked, [this]() {
        isMaximized() ? showNormal() : showMaximized();
    });
    connect(titleBar_, &TitleBar::closeClicked, this, &QMainWindow::close);

    // 통계 타이머 시작
    statsTimer_->start();
}

void MainWindow::openFiles(const QStringList& paths) {
    if (paths.isEmpty()) return;
    bool first = true;
    for (const QString& path : paths) {
        QFileInfo fi(path);
        if (!fi.exists()) continue;
        QString ext = fi.suffix().toLower();
        if (!VIDEO_EXTS.contains(ext) && !AUDIO_EXTS.contains(ext)) continue;
        playlist_->addFile(path);
        if (first) { mpvWidget_->loadFile(path); first = false; }
        else        { mpvWidget_->appendFile(path); }
    }
}

// ─── 슬롯 ────────────────────────────────────────────────────────
void MainWindow::onFileLoaded(const QString& path) {
    QFileInfo fi(path);
    updateWindowTitle(fi.fileName());
    playlist_->setCurrentFile(path);
    controlBar_->setPlaying(true);
}

void MainWindow::onPlaybackStarted()  { controlBar_->setPlaying(true); }
void MainWindow::onPlaybackPaused()   { controlBar_->setPlaying(false); }
void MainWindow::onPlaybackEnded()    { controlBar_->setPlaying(false); playlist_->playNext(); }
void MainWindow::onPlaybackStopped()  { controlBar_->setPlaying(false); updateWindowTitle(); }

void MainWindow::onPositionChanged(double s) { controlBar_->setPosition(s, totalDuration_); }
void MainWindow::onDurationChanged(double s) { totalDuration_ = s; controlBar_->setDuration(s); }
void MainWindow::onVolumeChanged(int v)      { controlBar_->setVolume(v); }

void MainWindow::onAudioFormatChanged(const QString& codec, int /*ch*/,
                                       int /*sr*/, const QString& /*out*/) {
    titleBar_->setAudioBadge(codec.toUpper());
}

void MainWindow::onVideoInfoChanged(int /*w*/, int /*h*/, double /*fps*/,
                                     const QString& /*codec*/) {}

void MainWindow::onTracksChanged() {
    trackSelector_->refresh();
}

void MainWindow::onErrorOccurred(const QString& msg) {
    qWarning() << "[MPV Error]" << msg;
}

void MainWindow::onOpenFile() {
    QStringList paths = QFileDialog::getOpenFileNames(
        this, "파일 열기", {},
        "미디어 파일 (*.mkv *.mp4 *.avi *.mov *.wmv *.flv *.ts *.m2ts "
        "*.m4v *.webm *.mp3 *.flac *.aac *.wav *.dts *.ac3 *.truehd);;"
        "모든 파일 (*.*)"
    );
    if (!paths.isEmpty()) openFiles(paths);
}

void MainWindow::onPlaylistItemDoubleClicked(int index) {
    QString path = playlist_->filePath(index);
    if (!path.isEmpty()) mpvWidget_->loadFile(path);
}

void MainWindow::onSettingsRequested() {
    SettingsDialog dlg(mpvWidget_->core(), this);
    dlg.exec();
}

void MainWindow::showStatsOverlay() {
    if (statsOverlay_->isVisible()) {
        statsOverlay_->hide();
    } else {
        statsOverlay_->show();
        statsOverlay_->raise();
    }
}

void MainWindow::toggleFullscreen() {
    if (isFullscreen_) {
        showNormal();
        titleBar_->show();
        playlist_->show();
        controlBar_->show();
        audioInfoBar_->show();
        isFullscreen_ = false;
    } else {
        showFullScreen();
        titleBar_->hide();
        playlist_->hide();
        isFullscreen_ = true;
    }
}

void MainWindow::togglePlayPause() { mpvWidget_->core()->togglePause(); }

// ─── 이벤트 ──────────────────────────────────────────────────────
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
    MpvCore* core = mpvWidget_->core();
    switch (e->key()) {
    case Qt::Key_Space:      togglePlayPause(); break;
    case Qt::Key_F:
    case Qt::Key_F11:        toggleFullscreen(); break;
    case Qt::Key_Escape:     if (isFullscreen_) toggleFullscreen(); break;
    case Qt::Key_Left:       core->seek(-5, true); break;
    case Qt::Key_Right:      core->seek(5, true); break;
    case Qt::Key_Up:         core->setVolume(qMin(core->volume() + 5, 200)); break;
    case Qt::Key_Down:       core->setVolume(qMax(core->volume() - 5, 0)); break;
    case Qt::Key_S:          showStatsOverlay(); break;
    case Qt::Key_O:
        if (e->modifiers() & Qt::ControlModifier) onOpenFile();
        break;
    case Qt::Key_Comma:      core->setSpeed(qMax(core->getProperty("speed").toDouble() - 0.1, 0.1)); break;
    case Qt::Key_Period:     core->setSpeed(qMin(core->getProperty("speed").toDouble() + 0.1, 4.0)); break;
    default:                 QMainWindow::keyPressEvent(e); break;
    }
}

void MainWindow::mouseDoubleClickEvent(QMouseEvent* e) {
    Q_UNUSED(e);
    toggleFullscreen();
}

// ─── 설정 ────────────────────────────────────────────────────────
void MainWindow::loadSettings() {
    resize(settings_.value("window/size", QSize(1280, 760)).toSize());
    QPoint pos = settings_.value("window/pos", QPoint(-1, -1)).toPoint();
    if (pos.x() >= 0) move(pos);

    int vol = settings_.value("audio/volume", 100).toInt();
    mpvWidget_->core()->setVolume(vol);
    controlBar_->setVolume(vol);

    bool exclusive = settings_.value("audio/exclusive", true).toBool();
    mpvWidget_->core()->setAudioExclusive(exclusive);

    // 패스스루 기본값 설정
    if (settings_.value("audio/passthrough", true).toBool()) {
        QStringList codecs;
        if (settings_.value("audio/pt_ac3",    true).toBool()) codecs << "ac3";
        if (settings_.value("audio/pt_eac3",   true).toBool()) codecs << "eac3";
        if (settings_.value("audio/pt_dts",    true).toBool()) codecs << "dts";
        if (settings_.value("audio/pt_dtshd",  true).toBool()) codecs << "dts-hd";
        if (settings_.value("audio/pt_truehd", true).toBool()) codecs << "truehd";
        mpvWidget_->core()->setSpdifCodecs(codecs);
    }

    splitter_->restoreState(settings_.value("ui/splitter").toByteArray());
}

void MainWindow::saveSettings() {
    settings_.setValue("window/size", size());
    settings_.setValue("window/pos",  pos());
    settings_.setValue("audio/volume", mpvWidget_->core()->volume());
    settings_.setValue("ui/splitter", splitter_->saveState());
}

void MainWindow::updateWindowTitle(const QString& filename) {
    QString title = filename.isEmpty() ? "소리누리" : QString("소리누리 — %1").arg(filename);
    setWindowTitle(title);
    titleBar_->setTitle(title);
}

void MainWindow::updateAudioBadge(const QString& codec) {
    titleBar_->setAudioBadge(codec);
}

QString MainWindow::formatTime(double seconds) const {
    int s = static_cast<int>(seconds);
    int h = s / 3600;
    int m = (s % 3600) / 60;
    int sec = s % 60;
    if (h > 0)
        return QString("%1:%2:%3").arg(h).arg(m, 2, 10, QChar('0')).arg(sec, 2, 10, QChar('0'));
    return QString("%1:%2").arg(m, 2, 10, QChar('0')).arg(sec, 2, 10, QChar('0'));
}
