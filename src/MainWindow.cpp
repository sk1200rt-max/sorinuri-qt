#include "MainWindow.h"
#include <QApplication>
#include <QFileDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDebug>
#include <QFileInfo>
#include <QMimeData>
#include <QUrl>

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

    // ── 타이틀바 ─────────────────────────────────────────────────
    titleBar_ = new TitleBar(this);
    mainLayout->addWidget(titleBar_);

    // ── 비디오 영역 (전체) ────────────────────────────────────────
    mpvWidget_ = new MpvWidget(this);
    mpvWidget_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    mainLayout->addWidget(mpvWidget_, 1);

    // ── 트랙 선택 바 ─────────────────────────────────────────────
    auto* trackBar = new QWidget(this);
    trackBar->setFixedHeight(32);
    trackBar->setStyleSheet("background: #0a0a0a; border-top: 1px solid #1a1a1a;");
    auto* trackLayout = new QHBoxLayout(trackBar);
    trackLayout->setContentsMargins(10, 0, 10, 0);
    trackLayout->setSpacing(6);

    trackSelector_ = new TrackSelector(this);
    trackLayout->addWidget(trackSelector_);
    trackLayout->addStretch();

    auto* settingsBtn = new QPushButton(this);
    settingsBtn->setIcon(QIcon(":/icons/settings.svg"));
    settingsBtn->setIconSize(QSize(14, 14));
    settingsBtn->setFixedSize(26, 26);
    settingsBtn->setToolTip("설정");
    settingsBtn->setStyleSheet(
        "QPushButton { background: transparent; border: 1px solid #222; border-radius: 3px; }"
        "QPushButton:hover { border-color: #4fc3f7; }");
    connect(settingsBtn, &QPushButton::clicked, this, &MainWindow::onSettingsRequested);
    trackLayout->addWidget(settingsBtn);
    mainLayout->addWidget(trackBar);

    // ── 컨트롤바 ─────────────────────────────────────────────────
    controlBar_ = new ControlBar(this);
    mainLayout->addWidget(controlBar_);

    // ── 오디오 정보 바 ────────────────────────────────────────────
    audioInfoBar_ = new AudioInfoBar(this);
    mainLayout->addWidget(audioInfoBar_);
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

    connect(titleBar_, &TitleBar::minimizeClicked,   this, &QMainWindow::showMinimized);
    connect(titleBar_, &TitleBar::maximizeClicked,   [this]() { isMaximized() ? showNormal() : showMaximized(); });
    connect(titleBar_, &TitleBar::fullscreenClicked, this, &MainWindow::toggleFullscreen);
    connect(titleBar_, &TitleBar::closeClicked,      this, &QMainWindow::close);
}

void MainWindow::openFiles(const QStringList& paths) {
    bool first = true;
    for (const QString& path : paths) {
        QFileInfo fi(path);
        if (!fi.exists()) continue;
        if (!MEDIA_EXTS.contains(fi.suffix().toLower())) continue;
        if (first) { mpvWidget_->loadFile(path); first = false; }
        else        { mpvWidget_->appendFile(path); }
    }
}

void MainWindow::onFileLoaded(const QString& path) {
    updateWindowTitle(QFileInfo(path).fileName());
    controlBar_->setPlaying(true);
}

void MainWindow::onPlaybackStarted()  { controlBar_->setPlaying(true); }
void MainWindow::onPlaybackPaused()   { controlBar_->setPlaying(false); }
void MainWindow::onPlaybackEnded()    { controlBar_->setPlaying(false); }
void MainWindow::onPlaybackStopped()  { controlBar_->setPlaying(false); updateWindowTitle(); }
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
    case Qt::Key_F:
    case Qt::Key_F11:    toggleFullscreen(); break;
    case Qt::Key_Escape: if (isFullscreen_) toggleFullscreen(); break;
    case Qt::Key_Left:   core->seek(-5, true); break;
    case Qt::Key_Right:  core->seek(5, true); break;
    case Qt::Key_Up:     core->setVolume(qMin(core->volume() + 5, 200)); break;
    case Qt::Key_Down:   core->setVolume(qMax(core->volume() - 5, 0)); break;
    case Qt::Key_M:      core->setMuted(!core->getProperty("mute").toBool()); break;
    case Qt::Key_O:
        if (e->modifiers() & Qt::ControlModifier) onOpenFile();
        break;
    default: QMainWindow::keyPressEvent(e); break;
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
    resizing_ = false; resizeEdge_ = 0;
    QMainWindow::mouseReleaseEvent(e);
}

void MainWindow::loadSettings() {
    resize(settings_.value("window/size", QSize(1280, 760)).toSize());
    QPoint p = settings_.value("window/pos", QPoint(-1,-1)).toPoint();
    if (p.x() >= 0) move(p);
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
