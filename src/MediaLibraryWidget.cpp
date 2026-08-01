#include "MediaLibraryWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QListWidgetItem>
#include <QIcon>
#include <QPixmap>
#include <QSettings>
#include <QStandardPaths>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QCryptographicHash>
#include <QtConcurrent>
#include <QFutureWatcher>

// 지원 확장자
static const QStringList VIDEO_EXTS = {
    "mkv","mp4","avi","mov","wmv","flv","ts","m2ts","m4v","webm","mpg","mpeg","rmvb"
};
static const QStringList AUDIO_EXTS = {
    "mp3","flac","aac","wav","m4a","ogg","opus","wma","dts","ac3","truehd","ape","alac"
};

static const QString GRID_STYLE = R"(
QListWidget {
    background: #111;
    border: none;
    color: #ccc;
}
QListWidget::item {
    border-radius: 6px;
    padding: 4px;
    margin: 4px;
}
QListWidget::item:selected {
    background: #1a3a5c;
    color: #fff;
}
QListWidget::item:hover {
    background: #1e1e1e;
}
)";

MediaLibraryWidget::MediaLibraryWidget(QWidget* parent) : QWidget(parent) {
    setupUI();
    setupDatabase();

    // 저장된 폴더 불러오기
    QSettings s("Sorinuri", "SorinuriPlayer");
    folders_ = s.value("library/folders").toStringList();
    if (!folders_.isEmpty()) refresh();
}

MediaLibraryWidget::~MediaLibraryWidget() {
    if (scanThread_ && scanThread_->isRunning()) {
        scanThread_->quit();
        scanThread_->wait(2000);
    }
}

void MediaLibraryWidget::setupUI() {
    setStyleSheet("background: #111; color: #ccc;");
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 툴바
    QWidget* toolbar = new QWidget(this);
    toolbar->setFixedHeight(44);
    toolbar->setStyleSheet("background: #0d0d0d; border-bottom: 1px solid #1e1e1e;");
    auto* tbLayout = new QHBoxLayout(toolbar);
    tbLayout->setContentsMargins(8, 0, 8, 0);
    tbLayout->setSpacing(6);

    // 검색창
    searchEdit_ = new QLineEdit(toolbar);
    searchEdit_->setPlaceholderText("검색...");
    searchEdit_->setStyleSheet(
        "QLineEdit { background: #1a1a1a; border: 1px solid #2a2a2a; border-radius: 4px;"
        "  padding: 4px 8px; color: #e0e0e0; font-size: 12px; }"
        "QLineEdit:focus { border-color: #4fc3f7; }");
    connect(searchEdit_, &QLineEdit::textChanged, this, &MediaLibraryWidget::onSearch);

    // 폴더 추가 버튼
    QPushButton* btnAdd = new QPushButton("+ 폴더 추가", toolbar);
    btnAdd->setStyleSheet(
        "QPushButton { background: #1a3a5c; color: #4fc3f7; border: none; border-radius: 3px;"
        "  padding: 6px 12px; font-size: 12px; }"
        "QPushButton:hover { background: #1e4a6e; }");
    connect(btnAdd, &QPushButton::clicked, this, &MediaLibraryWidget::onAddFolder);

    // 새로고침 버튼
    QPushButton* btnRefresh = new QPushButton("↺", toolbar);
    btnRefresh->setFixedSize(32, 32);
    btnRefresh->setToolTip("라이브러리 새로고침");
    btnRefresh->setStyleSheet(
        "QPushButton { background: transparent; color: #666; border: none; font-size: 16px; }"
        "QPushButton:hover { color: #4fc3f7; }");
    connect(btnRefresh, &QPushButton::clicked, this, &MediaLibraryWidget::refresh);

    tbLayout->addWidget(searchEdit_, 1);
    tbLayout->addWidget(btnAdd);
    tbLayout->addWidget(btnRefresh);
    mainLayout->addWidget(toolbar);

    // 탭 (비디오 / 음악)
    tabs_ = new QTabWidget(this);
    tabs_->setDocumentMode(true);
    tabs_->setStyleSheet(
        "QTabWidget::pane { border: none; background: #111; }"
        "QTabBar::tab { background: #0d0d0d; color: #666; padding: 8px 20px; border: none;"
        "  border-bottom: 2px solid transparent; font-size: 12px; }"
        "QTabBar::tab:selected { color: #4fc3f7; border-bottom: 2px solid #4fc3f7; }"
        "QTabBar::tab:hover { color: #ccc; }");

    videoList_ = new QListWidget(this);
    videoList_->setStyleSheet(GRID_STYLE);
    videoList_->setViewMode(QListWidget::IconMode);
    videoList_->setIconSize(QSize(120, 80));
    videoList_->setGridSize(QSize(140, 110));
    videoList_->setResizeMode(QListWidget::Adjust);
    videoList_->setMovement(QListWidget::Static);
    videoList_->setWordWrap(true);
    videoList_->setSpacing(4);
    connect(videoList_, &QListWidget::itemDoubleClicked,
            this, &MediaLibraryWidget::onItemDoubleClicked);

    audioList_ = new QListWidget(this);
    audioList_->setStyleSheet(GRID_STYLE);
    audioList_->setViewMode(QListWidget::ListMode);
    audioList_->setIconSize(QSize(40, 40));
    audioList_->setSpacing(2);
    connect(audioList_, &QListWidget::itemDoubleClicked,
            this, &MediaLibraryWidget::onItemDoubleClicked);

    tabs_->addTab(videoList_, "🎬  비디오");
    tabs_->addTab(audioList_, "🎵  음악");
    mainLayout->addWidget(tabs_, 1);

    // 하단 상태바
    QWidget* statusBar = new QWidget(this);
    statusBar->setFixedHeight(28);
    statusBar->setStyleSheet("background: #0d0d0d; border-top: 1px solid #1a1a1a;");
    auto* sbLayout = new QHBoxLayout(statusBar);
    sbLayout->setContentsMargins(8, 0, 8, 0);

    progressBar_ = new QProgressBar(statusBar);
    progressBar_->setFixedHeight(4);
    progressBar_->setTextVisible(false);
    progressBar_->setStyleSheet(
        "QProgressBar { background: #1a1a1a; border: none; border-radius: 2px; }"
        "QProgressBar::chunk { background: #4fc3f7; border-radius: 2px; }");
    progressBar_->hide();

    statusLabel_ = new QLabel("폴더를 추가하여 라이브러리를 구성하세요", statusBar);
    statusLabel_->setStyleSheet("color: #555; font-size: 11px;");

    sbLayout->addWidget(progressBar_, 1);
    sbLayout->addWidget(statusLabel_);
    mainLayout->addWidget(statusBar);
}

void MediaLibraryWidget::setupDatabase() {
    // SQLite DB 초기화 (향후 메타데이터 캐싱용)
    QString dbFile = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                     + "/sorinuri_library.db";
    QDir().mkpath(QFileInfo(dbFile).absolutePath());

    auto db = QSqlDatabase::addDatabase("QSQLITE", "library");
    db.setDatabaseName(dbFile);
    if (db.open()) {
        QSqlQuery q(db);
        q.exec("CREATE TABLE IF NOT EXISTS media ("
               "  path TEXT PRIMARY KEY,"
               "  title TEXT,"
               "  type TEXT,"
               "  duration REAL,"
               "  added_at INTEGER"
               ")");
    }
}

void MediaLibraryWidget::onAddFolder() {
    QString dir = QFileDialog::getExistingDirectory(
        this, "미디어 폴더 선택", QDir::homePath(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (dir.isEmpty()) return;

    if (!folders_.contains(dir)) {
        folders_.append(dir);
        QSettings s("Sorinuri", "SorinuriPlayer");
        s.setValue("library/folders", folders_);
        refresh();
    }
}

void MediaLibraryWidget::refresh() {
    if (scanning_) return;
    if (folders_.isEmpty()) {
        statusLabel_->setText("폴더를 추가하여 라이브러리를 구성하세요");
        return;
    }

    scanning_ = true;
    videoFiles_.clear();
    audioFiles_.clear();
    videoList_->clear();
    audioList_->clear();
    progressBar_->setValue(0);
    progressBar_->show();
    statusLabel_->setText("스캔 중...");

    // 비동기 스캔
    QStringList foldersToScan = folders_;
    auto* watcher = new QFutureWatcher<void>(this);

    auto future = QtConcurrent::run([this, foldersToScan]() {
        QStringList videos, audios;
        int count = 0;

        for (const QString& folder : foldersToScan) {
            QDirIterator it(folder, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                it.next();
                QString ext = it.fileInfo().suffix().toLower();
                if (VIDEO_EXTS.contains(ext)) {
                    videos.append(it.filePath());
                } else if (AUDIO_EXTS.contains(ext)) {
                    audios.append(it.filePath());
                }
                count++;
                if (count % 50 == 0) {
                    QMetaObject::invokeMethod(this, "onScanProgress",
                        Qt::QueuedConnection,
                        Q_ARG(int, count), Q_ARG(int, count + 100));
                }
            }
        }

        QMetaObject::invokeMethod(this, "onScanFinished",
            Qt::QueuedConnection,
            Q_ARG(QStringList, videos), Q_ARG(QStringList, audios));
    });

    watcher->setFuture(future);
}

void MediaLibraryWidget::onScanProgress(int current, int total) {
    if (total > 0) {
        progressBar_->setMaximum(total);
        progressBar_->setValue(current);
    }
}

void MediaLibraryWidget::onScanFinished(const QStringList& videoFiles,
                                         const QStringList& audioFiles) {
    scanning_ = false;
    videoFiles_ = videoFiles;
    audioFiles_ = audioFiles;
    progressBar_->hide();

    populateList(videoList_, videoFiles_);
    populateList(audioList_, audioFiles_);

    statusLabel_->setText(
        QString("비디오 %1개  |  음악 %2개").arg(videoFiles_.size()).arg(audioFiles_.size()));
}

void MediaLibraryWidget::populateList(QListWidget* list,
                                       const QStringList& files,
                                       const QString& filter) {
    list->clear();
    for (const QString& path : files) {
        QFileInfo fi(path);
        QString name = fi.completeBaseName();
        if (!filter.isEmpty() &&
            !name.contains(filter, Qt::CaseInsensitive)) continue;

        auto* item = new QListWidgetItem(name, list);
        item->setData(Qt::UserRole, path);
        item->setToolTip(path);

        // 비디오: 필름 아이콘, 음악: 음표 아이콘 (실제 썸네일은 향후 추가)
        if (list == videoList_) {
            item->setIcon(QIcon(":/icons/video_file.svg"));
        } else {
            item->setIcon(QIcon(":/icons/audio_file.svg"));
        }
    }
}

void MediaLibraryWidget::onSearch(const QString& text) {
    int tab = tabs_->currentIndex();
    if (tab == 0) populateList(videoList_, videoFiles_, text);
    else          populateList(audioList_, audioFiles_, text);
}

void MediaLibraryWidget::onTabChanged(int) {
    onSearch(searchEdit_->text());
}

void MediaLibraryWidget::onItemDoubleClicked(QListWidgetItem* item) {
    QString path = item->data(Qt::UserRole).toString();
    if (!path.isEmpty()) emit fileRequested(path);
}
