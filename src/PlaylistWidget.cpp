#include "PlaylistWidget.h"
#include <QFileInfo>
#include <QListWidgetItem>
#include <QPushButton>
#include <QHBoxLayout>

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

    tbLayout->addWidget(btnAdd);
    tbLayout->addStretch();
    tbLayout->addWidget(btnClear);

    layout->addWidget(toolbar);

    // 목록
    listWidget_ = new QListWidget(this);
    listWidget_->setAlternatingRowColors(false);
    connect(listWidget_, &QListWidget::itemDoubleClicked,
            this, &PlaylistWidget::onItemDoubleClicked);
    layout->addWidget(listWidget_, 1);
}

void PlaylistWidget::addFile(const QString& path) {
    QFileInfo fi(path);
    QListWidgetItem* item = new QListWidgetItem(fi.fileName(), listWidget_);
    item->setToolTip(path);
    filePaths_.append(path);
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
    }
}

void PlaylistWidget::playNext() {
    if (currentIdx_ + 1 < filePaths_.size()) {
        currentIdx_++;
        listWidget_->setCurrentRow(currentIdx_);
        emit itemDoubleClicked(currentIdx_);
    }
}

void PlaylistWidget::playPrev() {
    if (currentIdx_ > 0) {
        currentIdx_--;
        listWidget_->setCurrentRow(currentIdx_);
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
    emit itemDoubleClicked(idx);
}
