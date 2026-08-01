#pragma once
#include <QWidget>
#include <QListWidget>
#include <QVBoxLayout>
#include <QToolBar>
#include <QStringList>

class PlaylistWidget : public QWidget {
    Q_OBJECT
public:
    explicit PlaylistWidget(QWidget* parent = nullptr);

    void addFile(const QString& path);
    void addFiles(const QStringList& paths);
    void clear();
    void setCurrentFile(const QString& path);
    void playNext();
    void playPrev();
    void highlightCurrent();

    QString filePath(int index) const;
    int     currentIndex() const;
    int     count() const;

signals:
    void itemDoubleClicked(int index);
    void openFileRequested();

private slots:
    void onItemDoubleClicked(QListWidgetItem* item);

private:
    QListWidget* listWidget_ = nullptr;
    QStringList  filePaths_;
    int          currentIdx_ = -1;
};
