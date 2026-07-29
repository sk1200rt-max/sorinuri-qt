#pragma once
#include <QWidget>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QVector>
#include <QPair>

// LRC 가사 한 줄
struct LrcLine {
    double  timeMs;   // 밀리초 타임스탬프
    QString text;
};

class LyricsWidget : public QWidget {
    Q_OBJECT
public:
    explicit LyricsWidget(QWidget* parent = nullptr);

    // 트랙 정보로 가사 로드 (LRC 파일 → 인터넷 검색 순)
    void loadForTrack(const QString& title, const QString& artist,
                      const QString& filePath = QString());

    // 재생 위치 업데이트 (초 단위)
    void setPosition(double posSecs);

    // 가사 초기화
    void clear();

protected:
    void paintEvent(QPaintEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;

private slots:
    void onNetworkReply(QNetworkReply* reply);

private:
    void parseLrc(const QString& lrcText);
    void searchOnline(const QString& title, const QString& artist);
    void updateDisplay();
    int  findCurrentLine(double posMs) const;
    void drawHeader(QPainter& p);

    QNetworkAccessManager* nam_       = nullptr;

    QVector<LrcLine>  lines_;
    int               currentIdx_     = -1;
    double            posMs_          = 0;

    // 표시 상태
    QString           statusText_;    // "검색 중...", "가사 없음" 등
    bool              hasLyrics_      = false;

    // 스크롤 애니메이션
    double            scrollOffset_   = 0;
    double            targetOffset_   = 0;
    QTimer*           scrollTimer_    = nullptr;

    // 현재 트랙 정보
    QString           currentTitle_;
    QString           currentArtist_;
};
