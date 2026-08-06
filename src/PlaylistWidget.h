#pragma once
#include <QWidget>
#include <QListWidget>
#include <QListWidgetItem>
#include <QVBoxLayout>
#include <QToolBar>
#include <QStringList>
#include <QHash>
#include <QPixmap>
#include <QMap>

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
    void onThumbnailLoaded(const QString& path, const QPixmap& thumb);
    void onSavePlaylist();
    void onLoadPlaylist();

private:
    void    loadThumbnailAsync(const QString& path, QListWidgetItem* item);
    QString formatDuration(double secs) const;

    QListWidget* listWidget_ = nullptr;
    QStringList  filePaths_;
    int          currentIdx_ = -1;
    QHash<QString, QPixmap> thumbCache_;
};
