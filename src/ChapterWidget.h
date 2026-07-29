#pragma once
#include <QWidget>
#include <QVector>
#include <QPixmap>

class QLabel;
class QPushButton;
class QListWidget;
class MpvCore;

struct Chapter {
    double  startSec  = 0.0;
    QString title;
    QPixmap thumbnail;
    bool    isBookmark = false;
};

class ChapterWidget : public QWidget {
    Q_OBJECT
public:
    explicit ChapterWidget(MpvCore* core, QWidget* parent = nullptr);

    const QVector<Chapter>& chapters() const { return chapters_; }
    void setDuration(double sec) { duration_ = sec; }

public slots:
    void loadChapters();
    void addBookmark(double sec);
    void removeBookmark(int idx);
    void jumpToChapter(int idx);
    void onPositionChanged(double sec);
    void generateThumbnails(const QString& filePath);

signals:
    void seekRequested(double sec);
    void chaptersChanged();

private:
    void buildUI();
    void refreshList();
    QString formatTime(double sec) const;

    MpvCore*       core_       = nullptr;
    QVector<Chapter> chapters_;
    double         duration_   = 0.0;
    double         currentPos_ = 0.0;
    QString        mediaPath_;

    QListWidget*   lstChapters_ = nullptr;
    QPushButton*   btnAddBm_    = nullptr;
    QPushButton*   btnRemoveBm_ = nullptr;
    QLabel*        lblCount_    = nullptr;
};
