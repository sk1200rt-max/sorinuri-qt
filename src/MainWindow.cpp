#include "MainWindow.h"
#include "ControlBar.h"
#include "PlaylistWidget.h"
#include "TitleBar.h"
#include "Settings.h"

#include <QApplication>
#include <QFileDialog>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QCloseEvent>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QStatusBar>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QDebug>
#include <QFileInfo>
#include <QMimeDatabase>

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
    setMinimumSize(900, 600);
    setAcceptDrops(true);

    // 프레임리스 윈도우 (커스텀 타이틀바 사용)
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);

    setupUI();
    setupConnections();
    loadSettings();
}

MainWindow::~MainWindow() {
    saveSettings();
}

void MainWindow::setupUI() {
    // ── 중앙 위젯 ──────────────────────────────────────────────────
    QWidget* central = new QWidget(this);
    setCentralWidget(central);

    QVBoxLayout* mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ── 커스텀 타이틀바 ────────────────────────────────────────────
    titleBar_ = new TitleBar(this);
    mainLayout->addWidget(titleBar_);

    // ── 메인 영역 (사이드바 + 비디오) ──────────────────────────────
    splitter_ = new QSplitter(Qt::Horizontal, this);
    splitter_->setHandleWidth(1);
    splitter_->setStyleSheet("QSplitter::handle { background: #2a2a2a; }");

    // 재생목록 사이드바
    playlist_ = new PlaylistWidget(this);
    playlist_->setMinimumWidth(200);
    playlist_->setMaximumWidth(320);
    splitter_->addWidget(playlist_);

    // MPV 비디오 위젯
    mpvWidget_ = new MpvWidget(this);
    mpvWidget_->setMinimumSize(400, 300);
    splitter_->addWidget(mpvWidget_);

    splitter_->setStretchFactor(0, 0);
    splitter_->setStretchFactor(1, 1);
    splitter_->setSizes({240, 1000});

    mainLayout->addWidget(splitter_, 1);

    // ── 컨트롤바 ────────────────────────────────────────────────────
    controlBar_ = new ControlBar(this);
    mainLayout->addWidget(controlBar_);

    // ── 상태바 (오디오 포맷) ────────────────────────────────────────
    QWidget* statusWidget = new QWidget(this);
    statusWidget->setFixedHeight(28);
    statusWidget->setStyleSheet("background: #0d0d0d; border-top: 1px solid #1e1e1e;");

    QHBoxLayout* statusLayout = new QHBoxLayout(statusWidget);
    statusLayout->setContentsMargins(12, 0, 12, 0);

    statusLabel_ = new QLabel("소리누리 v1.0", this);
    statusLabel_->setStyleSheet("color: #666; font-size: 11px;");
    statusLayout->addWidget(statusLabel_);
    statusLayout->addStretch();

    mainLayout->addWidget(statusWidget);

    // ── 전체 스타일 ─────────────────────────────────────────────────
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

    connect(core, &MpvCore::fileLoaded,         this, &MainWindow::onFileLoaded);
    connect(core, &MpvCore::playbackStarted,    this, &MainWindow::onPlaybackStarted);
    connect(core, &MpvCore::playbackPaused,     this, &MainWindow::onPlaybackPaused);
    connect(core, &MpvCore::playbackEnded,      this, &MainWindow::onPlaybackEnded);
    connect(core, &MpvCore::positionChanged,    this, &MainWindow::onPositionChanged);
    connect(core, &MpvCore::durationChanged,    this, &MainWindow::onDurationChanged);
    connect(core, &MpvCore::volumeChanged,      this, &MainWindow::onVolumeChanged);
    connect(core, &MpvCore::audioFormatChanged, this, &MainWindow::onAudioFormatChanged);
    connect(core, &MpvCore::tracksChanged,      this, &MainWindow::onTracksChanged);

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

        if (first) {
            mpvWidget_->loadFile(path);
            first = false;
        } else {
            mpvWidget_->appendFile(path);
        }
    }
}

// ─── 슬롯 ────────────────────────────────────────────────────────
void MainWindow::onFileLoaded(const QString& path) {
    QFileInfo fi(path);
    updateWindowTitle(fi.fileName());
    playlist_->setCurrentFile(path);
    controlBar_->setPlaying(true);
    qInfo() << "[UI] 파일 로드됨:" << fi.fileName();
}

void MainWindow::onPlaybackStarted() {
    controlBar_->setPlaying(true);
}

void MainWindow::onPlaybackPaused() {
    controlBar_->setPlaying(false);
}

void MainWindow::onPlaybackEnded() {
    controlBar_->setPlaying(false);
    // 다음 파일 재생
    playlist_->playNext();
}

void MainWindow::onPositionChanged(double seconds) {
    controlBar_->setPosition(seconds, totalDuration_);
}

void MainWindow::onDurationChanged(double seconds) {
    totalDuration_ = seconds;
    controlBar_->setDuration(seconds);
}

void MainWindow::onVolumeChanged(int vol) {
    controlBar_->setVolume(vol);
}

void MainWindow::onAudioFormatChanged(const QString& codec, int /*channels*/,
                                       int sampleRate, const QString& output) {
    QString info = QString("%1 | %2 Hz | %3")
        .arg(codec.toUpper())
        .arg(sampleRate)
        .arg(output);
    statusLabel_->setText(info);
    titleBar_->setAudioBadge(codec.toUpper());
}

void MainWindow::onTracksChanged() {
    // 트랙 목록 업데이트
    controlBar_->updateTracks(mpvWidget_->core());
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

void MainWindow::toggleFullscreen() {
    if (isFullscreen_) {
        showNormal();
        titleBar_->show();
        playlist_->show();
        controlBar_->show();
        isFullscreen_ = false;
    } else {
        showFullScreen();
        titleBar_->hide();
        playlist_->hide();
        isFullscreen_ = true;
    }
}

void MainWindow::togglePlayPause() {
    mpvWidget_->core()->togglePause();
}

// ─── 이벤트 ──────────────────────────────────────────────────────
void MainWindow::closeEvent(QCloseEvent* event) {
    saveSettings();
    event->accept();
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* event) {
    QStringList paths;
    for (const QUrl& url : event->mimeData()->urls()) {
        if (url.isLocalFile()) paths << url.toLocalFile();
    }
    if (!paths.isEmpty()) openFiles(paths);
}

void MainWindow::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
    case Qt::Key_Space:
        togglePlayPause();
        break;
    case Qt::Key_F:
    case Qt::Key_F11:
        toggleFullscreen();
        break;
    case Qt::Key_Escape:
        if (isFullscreen_) toggleFullscreen();
        break;
    case Qt::Key_Left:
        mpvWidget_->core()->seek(-5, true);
        break;
    case Qt::Key_Right:
        mpvWidget_->core()->seek(5, true);
        break;
    case Qt::Key_Up:
        mpvWidget_->core()->setVolume(
            qMin(mpvWidget_->core()->volume() + 5, 200));
        break;
    case Qt::Key_Down:
        mpvWidget_->core()->setVolume(
            qMax(mpvWidget_->core()->volume() - 5, 0));
        break;
    case Qt::Key_O:
        if (event->modifiers() & Qt::ControlModifier) onOpenFile();
        break;
    default:
        QMainWindow::keyPressEvent(event);
    }
}

void MainWindow::mouseDoubleClickEvent(QMouseEvent* event) {
    Q_UNUSED(event);
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

    splitter_->restoreState(settings_.value("ui/splitter").toByteArray());
}

void MainWindow::saveSettings() {
    settings_.setValue("window/size", size());
    settings_.setValue("window/pos",  pos());
    settings_.setValue("audio/volume", mpvWidget_->core()->volume());
    settings_.setValue("ui/splitter", splitter_->saveState());
}

void MainWindow::updateWindowTitle(const QString& filename) {
    if (filename.isEmpty())
        setWindowTitle("소리누리");
    else
        setWindowTitle(QString("소리누리 — %1").arg(filename));
    titleBar_->setTitle(windowTitle());
}

void MainWindow::updateAudioBadge(const QString& codec) {
    titleBar_->setAudioBadge(codec);
}
