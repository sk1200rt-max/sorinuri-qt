#pragma once
#include <QWidget>
#include <QVector>
#include <QPixmap>
#include <QLabel>
#include <QSlider>
#include <QProcess>

class QPushButton;
class QListWidget;
class QListWidgetItem;
class MpvCore;

struct Chapter {
    double  startSec   = 0.0;
    QString title;
    QPixmap thumbnail;
    bool    isBookmark = false;
};

// ─── 챕터 타임라인 바 (커스텀 위젯) ────────────────────────────────────────
class ChapterTimeline : public QWidget {
    Q_OBJECT
public:
    explicit ChapterTimeline(QWidget* parent = nullptr);
    void setChapters(const QVector<Chapter>& chapters, double duration);
    void setPosition(double sec);

signals:
    void seekRequested(double sec);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override;

private:
    double posToSec(int x) const;
    int    secToPos(double sec) const;

    QVector<Chapter> chapters_;
    double duration_   = 0.0;
    double position_   = 0.0;
    int    hoverX_     = -1;
    int    hoverChapter_ = -1;
};

// ─── 썸네일 스트립 (커스텀 위젯) ────────────────────────────────────────────
class ThumbnailStrip : public QWidget {
    Q_OBJECT
public:
    explicit ThumbnailStrip(QWidget* parent = nullptr);
    void setChapters(const QVector<Chapter>& chapters);
    void setCurrentChapter(int idx);

signals:
    void chapterClicked(int idx);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;

private:
    QVector<Chapter> chapters_;
    int currentIdx_ = -1;
    static const int THUMB_W = 120;
    static const int THUMB_H = 68;
};

// ─── 챕터 & 북마크 메인 위젯 ─────────────────────────────────────────────
class ChapterWidget : public QWidget {
    Q_OBJECT
public:
    explicit ChapterWidget(MpvCore* core, QWidget* parent = nullptr);

    const QVector<Chapter>& chapters() const { return chapters_; }
    void setDuration(double sec);

public slots:
    void loadChapters();
    void addBookmark(double sec);
    void removeBookmark(int idx);
    void jumpToChapter(int idx);
    void onPositionChanged(double sec);
    void generateThumbnails(const QString& filePath);
    // AI 장면 전환 감지
    void detectScenes(const QString& filePath);

signals:
    void seekRequested(double sec);
    void chaptersChanged();

private slots:
    void onSceneDetectFinished(int exitCode, QProcess::ExitStatus status);

private:
    void buildUI();
    void refreshList();
    void refreshTimeline();
    QString formatTime(double sec) const;

    MpvCore*         core_          = nullptr;
    QVector<Chapter> chapters_;
    double           duration_      = 0.0;
    double           currentPos_    = 0.0;
    QString          mediaPath_;

    // AI 장면 감지
    QProcess*        sceneProcess_         = nullptr;
    QPushButton*     btnDetect_            = nullptr;
    QSlider*         sceneThresholdSlider_ = nullptr;  // v6.18.0 민감도 조절
    QLabel*          lblThresholdVal_      = nullptr;
    double           sceneThreshold_       = 0.35;

    // UI 컴포넌트
    ChapterTimeline* timeline_      = nullptr;
    ThumbnailStrip*  thumbStrip_    = nullptr;
    QListWidget*     lstChapters_   = nullptr;
    QPushButton*     btnAddBm_      = nullptr;
    QPushButton*     btnRemoveBm_   = nullptr;
    QLabel*          lblCount_      = nullptr;
};
