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
#include <QPropertyAnimation>

// LRC 가사 한 줄
struct LrcLine {
    double  timeMs;   // 밀리초 타임스탬프
    QString text;
};

// AI 가사 검색 상태
enum class LyricsSearchState {
    Idle,           // 대기
    Searching,      // AI 검색 중 (점멸 애니메이션)
    Found,          // 가사 찾음
    NotFound,       // 가사 없음
    LocalFile       // 로컬 LRC 파일
};

class LyricsWidget : public QWidget {
    Q_OBJECT
public:
    explicit LyricsWidget(QWidget* parent = nullptr);

    // 트랙 정보로 가사 로드 (LRC 파일 → /api/get 정확 매칭 → /api/search 순)
    void loadForTrack(const QString& title, const QString& artist,
                      const QString& filePath = QString(),
                      double durationSecs = 0.0,
                      const QString& album = QString());

    // 재생 위치 업데이트 (초 단위)
    void setPosition(double posSecs);

    // 가사 초기화
    void clear();

protected:
    void paintEvent(QPaintEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;

private slots:
    void onNetworkReply(QNetworkReply* reply);
    void onAiBlinkTick();   // AI 아이콘 점멸 타이머

private:
    void parseLrc(const QString& lrcText);
    void searchBySignature(const QString& title, const QString& artist,
                           const QString& album, double durationSecs);
    void searchOnline(const QString& title, const QString& artist);
    int  findCurrentLine(double posMs) const;
    void drawHeader(QPainter& p);
    void setSearchState(LyricsSearchState state);

    QNetworkAccessManager* nam_           = nullptr;

    QVector<LrcLine>  lines_;
    int               currentIdx_         = -1;
    double            posMs_              = 0;

    // 표시 상태
    QString           statusText_;
    bool              hasLyrics_          = false;
    LyricsSearchState searchState_        = LyricsSearchState::Idle;

    // AI 아이콘 점멸 애니메이션
    QTimer*           aiBlinkTimer_       = nullptr;
    bool              aiBlinkOn_          = false;
    float             aiIconAlpha_        = 0.0f;   // 0~1

    // 스크롤 애니메이션
    double            scrollOffset_       = 0;
    double            targetOffset_       = 0;
    QTimer*           scrollTimer_        = nullptr;

    // 현재 트랙 정보
    QString           currentTitle_;
    QString           currentArtist_;
    QString           currentAlbum_;
    double            currentDuration_    = 0.0;

    // 검색 단계 추적 (1=signature, 2=search fallback)
    int               searchStep_         = 0;

    // 싱크 오프셋 (밀리초 단위, 양수=늦게, 음수=일직 표시)
    double            syncOffsetMs_       = 0.0;

public:
    // 싱크 오프셋 조정 (+/-밀리초)
    void   setSyncOffset(double ms) { syncOffsetMs_ = ms; update(); }
    double syncOffset() const { return syncOffsetMs_; }
    // 오프셋 저장/불러오기 (파일 경로 기반 QSettings)
    void   saveSyncOffset(const QString& filePath);
    void   loadSyncOffset(const QString& filePath);
};
