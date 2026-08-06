#include "PlaylistWidget.h"
#include "AlbumArtExtractor.h"
#include <QFileInfo>
#include <QPainter>
#include <QPainterPath>
#include <QFileDialog>
#include <QStandardPaths>
#include <QTextStream>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QFileInfo>
#include <QListWidgetItem>
#include <QPushButton>
#include <QHBoxLayout>
#include <QAbstractItemModel>

PlaylistWidget::PlaylistWidget(QWidget* parent) : QWidget(parent) {
    setStyleSheet(R"(
        QWidget { background: #111; }
        QListWidget {
            background: #111;
            border: none;
            color: #ccc;
            font-size: 12px;
        }
        QListWidget::item {
            padding: 6px 8px;
            border-bottom: 1px solid #1a1a1a;
        }
        QListWidget::item:selected {
            background: #1a3a5c;
            color: #fff;
        }
        QListWidget::item:hover {
            background: #1e1e1e;
        }
    )");

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // 툴바
    QWidget* toolbar = new QWidget(this);
    toolbar->setFixedHeight(36);
    toolbar->setStyleSheet("background: #0f0f0f; border-bottom: 1px solid #1e1e1e;");

    QHBoxLayout* tbLayout = new QHBoxLayout(toolbar);
    tbLayout->setContentsMargins(6, 0, 6, 0);
    tbLayout->setSpacing(4);

    QPushButton* btnAdd = new QPushButton("+ 추가", toolbar);
    btnAdd->setStyleSheet(R"(
        QPushButton {
            background: #1a3a5c;
            color: #4fc3f7;
            border: none;
            border-radius: 3px;
            padding: 4px 10px;
            font-size: 12px;
        }
        QPushButton:hover { background: #1e4a6e; }
    )");
    connect(btnAdd, &QPushButton::clicked, this, &PlaylistWidget::openFileRequested);

    QPushButton* btnClear = new QPushButton("전체 삭제", toolbar);
    btnClear->setStyleSheet(R"(
        QPushButton {
            background: transparent;
            color: #666;
            border: none;
            padding: 4px 8px;
            font-size: 12px;
        }
        QPushButton:hover { color: #e57373; }
    )");
    connect(btnClear, &QPushButton::clicked, this, &PlaylistWidget::clear);

    QPushButton* btnSave = new QPushButton("저장", toolbar);
    btnSave->setStyleSheet(btnClear->styleSheet());
    connect(btnSave, &QPushButton::clicked, this, &PlaylistWidget::onSavePlaylist);

    QPushButton* btnLoad = new QPushButton("불러오기", toolbar);
    btnLoad->setStyleSheet(btnClear->styleSheet());
    connect(btnLoad, &QPushButton::clicked, this, &PlaylistWidget::onLoadPlaylist);

    tbLayout->addWidget(btnAdd);
    tbLayout->addWidget(btnSave);
    tbLayout->addWidget(btnLoad);
    tbLayout->addStretch();
    tbLayout->addWidget(btnClear);

    layout->addWidget(toolbar);

    // 목록
    listWidget_ = new QListWidget(this);
    listWidget_->setAlternatingRowColors(false);
    listWidget_->setIconSize(QSize(48, 48));  // 썸네일 크기
    listWidget_->setSpacing(2);

    // 드래그앤드롭 재정렬 활성화
    listWidget_->setDragEnabled(true);
    listWidget_->setAcceptDrops(true);
    listWidget_->setDropIndicatorShown(true);
    listWidget_->setDragDropMode(QAbstractItemView::InternalMove);
    listWidget_->setDefaultDropAction(Qt::MoveAction);

    connect(listWidget_, &QListWidget::itemDoubleClicked,
            this, &PlaylistWidget::onItemDoubleClicked);

    // 드래그로 순서 변경 시 filePaths_ 동기화
    connect(listWidget_->model(), &QAbstractItemModel::rowsMoved,
            this, [this](const QModelIndex&, int src, int, const QModelIndex&, int dst) {
        if (src >= 0 && src < filePaths_.size()) {
            QString moved = filePaths_.takeAt(src);
            int insertAt = (dst > src) ? dst - 1 : dst;
            insertAt = qBound(0, insertAt, filePaths_.size());
            filePaths_.insert(insertAt, moved);
            // currentIdx_ 재계산
            if (currentIdx_ == src) {
                currentIdx_ = insertAt;
            } else if (src < currentIdx_ && insertAt >= currentIdx_) {
                currentIdx_--;
            } else if (src > currentIdx_ && insertAt <= currentIdx_) {
                currentIdx_++;
            }
            // 재생 중 항목 하이라이트 갱신
            highlightCurrent();
        }
    });

    layout->addWidget(listWidget_, 1);
}

void PlaylistWidget::highlightCurrent() {
    for (int i = 0; i < listWidget_->count(); ++i) {
        QListWidgetItem* item = listWidget_->item(i);
        if (i == currentIdx_) {
            // 재생 중 항목: 청록색 텍스트 + 볼드 + 재생 아이콘
            item->setForeground(QColor("#4fc3f7"));
            QFont f = item->font();
            f.setBold(true);
            item->setFont(f);
            // 파일명 앞에 ▶ 표시
            QFileInfo fi(filePaths_.value(i));
            item->setText(QString("▶  %1").arg(fi.fileName()));
        } else {
            item->setForeground(QColor("#ccc"));
            QFont f = item->font();
            f.setBold(false);
            item->setFont(f);
            QFileInfo fi(filePaths_.value(i));
            item->setText(fi.fileName());
        }
    }
}

void PlaylistWidget::addFile(const QString& path) {
    QFileInfo fi(path);
    // 파일명 + 재생시간 표시
    QString displayName = fi.fileName();
    QListWidgetItem* item = new QListWidgetItem(displayName, listWidget_);
    item->setToolTip(path);
    item->setForeground(QColor("#ccc"));
    filePaths_.append(path);

    // 썸네일 비동기 로드 (음악 파일만)
    static const QStringList audioExts = {
        "mp3","flac","aac","wav","m4a","ogg","opus","wma","ape","alac","dsd","dsf","dff"
    };
    QString ext = fi.suffix().toLower();
    if (audioExts.contains(ext)) {
        if (thumbCache_.contains(path)) {
            item->setIcon(QIcon(thumbCache_[path]));
        } else {
            item->setIcon(QIcon(":/icons/audio.svg"));
            loadThumbnailAsync(path, item);
        }
    } else {
        // 영상 파일: 기본 아이콘 (video 아이콘 없으면 빈 아이콘)
        QIcon videoIcon = QIcon::fromTheme("video-x-generic");
        if (videoIcon.isNull()) videoIcon = QIcon(":/icons/audio.svg");
        item->setIcon(videoIcon);
    }
}

void PlaylistWidget::loadThumbnailAsync(const QString& path, QListWidgetItem* item) {
    auto* watcher = new QFutureWatcher<QPixmap>(this);
    connect(watcher, &QFutureWatcher<QPixmap>::finished, this,
            [this, watcher, path, item]() {
        QPixmap thumb = watcher->result();
        watcher->deleteLater();
        if (!thumb.isNull()) {
            QPixmap scaled = thumb.scaled(48, 48,
                Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            QPixmap cropped(48, 48);
            cropped.fill(Qt::transparent);
            QPainter p(&cropped);
            p.setRenderHint(QPainter::Antialiasing);
            QPainterPath pp;
            pp.addRoundedRect(0, 0, 48, 48, 4, 4);
            p.setClipPath(pp);
            int ox = (scaled.width() - 48) / 2;
            int oy = (scaled.height() - 48) / 2;
            p.drawPixmap(-ox, -oy, scaled);
            thumbCache_[path] = cropped;
            if (item) item->setIcon(QIcon(cropped));
        }
    });
    watcher->setFuture(QtConcurrent::run([path]() -> QPixmap {
        return AlbumArtExtractor::extract(path);
    }));
}

void PlaylistWidget::onThumbnailLoaded(const QString& path, const QPixmap& thumb) {
    thumbCache_[path] = thumb;
}

void PlaylistWidget::onSavePlaylist() {
    if (filePaths_.isEmpty()) return;
    QString path = QFileDialog::getSaveFileName(this, "재생목록 저장",
        QStandardPaths::writableLocation(QStandardPaths::MusicLocation) + "/playlist.m3u",
        "M3U 재생목록 (*.m3u)");
    if (path.isEmpty()) return;

    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out.setEncoding(QStringConverter::Utf8);
        out << "#EXTM3U\n";
        for (const QString& p : filePaths_) {
            QFileInfo fi(p);
            // #EXTINF:재생시간(초),아티스트 - 제목 형식
            // 재생시간은 현재 알 수 없으므로 -1 사용 (표준 M3U 관례)
            QString title = fi.completeBaseName();
            out << QString("#EXTINF:-1,%1\n").arg(title);
            out << p << "\n";
        }
    }
}

void PlaylistWidget::onLoadPlaylist() {
    QString path = QFileDialog::getOpenFileName(this, "재생목록 불러오기",
        QStandardPaths::writableLocation(QStandardPaths::MusicLocation),
        "M3U 재생목록 (*.m3u *.m3u8);;모든 파일 (*.*)");
    if (path.isEmpty()) return;

    QFile file(path);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        in.setEncoding(QStringConverter::Utf8);
        QStringList newPaths;
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (line.isEmpty() || line.startsWith("#")) continue;
            newPaths << line;
        }
        if (!newPaths.isEmpty()) {
            addFiles(newPaths);
        }
    }
}

QString PlaylistWidget::formatDuration(double secs) const {
    if (secs <= 0) return "";
    int t = static_cast<int>(secs);
    int h = t / 3600, m = (t % 3600) / 60, s = t % 60;
    if (h > 0)
        return QString("%1:%2:%3").arg(h).arg(m,2,10,QChar('0')).arg(s,2,10,QChar('0'));
    return QString("%1:%2").arg(m,2,10,QChar('0')).arg(s,2,10,QChar('0'));
}

void PlaylistWidget::addFiles(const QStringList& paths) {
    for (const QString& p : paths) addFile(p);
}

void PlaylistWidget::clear() {
    listWidget_->clear();
    filePaths_.clear();
    currentIdx_ = -1;
}

void PlaylistWidget::setCurrentFile(const QString& path) {
    int idx = filePaths_.indexOf(path);
    if (idx >= 0) {
        currentIdx_ = idx;
        listWidget_->setCurrentRow(idx);
        highlightCurrent();
        // 현재 재생 항목이 보이도록 스크롤
        listWidget_->scrollToItem(listWidget_->item(idx),
                                   QAbstractItemView::PositionAtCenter);
    }
}

void PlaylistWidget::playNext() {
    if (currentIdx_ + 1 < filePaths_.size()) {
        currentIdx_++;
        listWidget_->setCurrentRow(currentIdx_);
        highlightCurrent();
        emit itemDoubleClicked(currentIdx_);
    }
}

void PlaylistWidget::playPrev() {
    if (currentIdx_ > 0) {
        currentIdx_--;
        listWidget_->setCurrentRow(currentIdx_);
        highlightCurrent();
        emit itemDoubleClicked(currentIdx_);
    }
}

QString PlaylistWidget::filePath(int index) const {
    if (index >= 0 && index < filePaths_.size())
        return filePaths_.at(index);
    return {};
}

int PlaylistWidget::currentIndex() const { return currentIdx_; }
int PlaylistWidget::count() const { return filePaths_.size(); }

void PlaylistWidget::onItemDoubleClicked(QListWidgetItem* item) {
    int idx = listWidget_->row(item);
    currentIdx_ = idx;
    highlightCurrent();
    emit itemDoubleClicked(idx);
}
