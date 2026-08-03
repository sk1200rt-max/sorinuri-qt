#pragma once
#include <QWidget>
#include <QLabel>
#include <QSlider>
#include "ControlBar.h"  // ClickSeekSlider (HiDPI 클릭 즉시 이동)
#include <QPushButton>
#include <QListWidget>
#include <QPixmap>
#include <QTimer>
#include <QMouseEvent>
#include <QPropertyAnimation>

/**
 * CompactPlayerWidget — 소리누리 소형 모드
 *
 * AIMP 스타일에서 영감을 받은 세로형 독립 음악 플레이어 창.
 * 상단: 앨범아트 + 트랙 정보 + 스펙트럼 + 컨트롤
 * 하단: 재생목록 (드래그로 분할 비율 조절)
 *
 * 크기: 340×620px (기본), 280×480 ~ 500×900 범위에서 리사이즈 가능
 * 항상 위에 고정 가능 (핀 버튼)
 * 타이틀바 없음 (커스텀 드래그 영역)
 */
class CompactPlayerWidget : public QWidget {
    Q_OBJECT
public:
    explicit CompactPlayerWidget(QWidget* parent = nullptr);

    // 트랙 메타 업데이트
    void setMeta(const QString& title, const QString& artist,
                 const QString& album, const QPixmap& art,
                 const QString& codec, int bitDepth, int sampleRate,
                 int channels, bool bitPerfect);

    // 재생 위치 업데이트
    void updatePosition(double pos, double duration);

    // 재생 상태 업데이트
    void setPlaying(bool playing);

    // 재생목록 업데이트
    void setPlaylist(const QStringList& paths, int currentIndex);

    // 스펙트럼 데이터 업데이트
    void updateSpectrum(const QVector<float>& bins);

signals:
    void playPauseRequested();
    void prevRequested();
    void nextRequested();
    void seekRequested(double pos);
    void volumeChanged(int vol);
    void trackSelected(int index);
    void expandRequested();       // 전체 플레이어로 돌아가기
    void alwaysOnTopToggled(bool on);
    void shuffleToggled(bool on);
    void repeatToggled(bool on);

protected:
    void paintEvent(QPaintEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;

private:
    void setupUI();
    void setupConnections();
    void updateAlbumArt(const QPixmap& art);
    void drawSpectrum(QPainter& p, const QRect& rect);
    void drawVuMeter(QPainter& p, const QRect& rect);
    QString formatTime(double secs) const;

    // ── 상단 플레이어 섹션 ──────────────────────────────────────────
    QWidget*     playerSection_   = nullptr;

    // 앨범아트
    QPixmap      albumArtPixmap_;
    QPixmap      blurBg_;
    QColor       dominantColor_   = QColor(0, 200, 180);
    QRect        artRect_;          // resizeEvent에서 계산되는 앨범아트 영역

    // 트랙 정보
    QLabel*      titleLabel_      = nullptr;
    QLabel*      artistLabel_     = nullptr;
    QLabel*      codecLabel_      = nullptr;   // "MP3 · 44.1kHz · 32bit"

    // 컨트롤 버튼
    QPushButton* btnShuffle_      = nullptr;
    QPushButton* btnPrev_         = nullptr;
    QPushButton* btnPlay_         = nullptr;
    QPushButton* btnNext_         = nullptr;
    QPushButton* btnRepeat_       = nullptr;

    // 시크바
    ClickSeekSlider* seekSlider_  = nullptr;
    QLabel*      timeCurrent_     = nullptr;
    QLabel*      timeDuration_    = nullptr;

    // 볼륨
    QSlider*     volSlider_       = nullptr;
    QPushButton* btnVolume_       = nullptr;

    // 상단 우측 버튼
    QPushButton* btnPin_          = nullptr;   // 항상 위에 고정
    QPushButton* btnExpand_       = nullptr;   // 전체 플레이어로
    QPushButton* btnClose_        = nullptr;   // 닫기

    // ── 하단 재생목록 섹션 ──────────────────────────────────────────
    QListWidget* playlistWidget_  = nullptr;

    // ── 분할선 드래그 ───────────────────────────────────────────────
    int          splitterY_       = 380;       // 분할선 Y 위치 (픽셀)
    bool         splitterDragging_= false;

    // ── 스펙트럼 ────────────────────────────────────────────────────
    QVector<float> specBins_;
    QVector<float> specPeak_;
    QTimer*        peakTimer_     = nullptr;

    // ── 상태 ────────────────────────────────────────────────────────
    bool         isPlaying_       = false;
    bool         isPinned_        = false;
    bool         isShuffle_       = false;
    bool         isRepeat_        = false;
    double       duration_        = 0;
    int          currentTrackIdx_ = -1;

    // ── 창 드래그 이동 ──────────────────────────────────────────────
    bool         dragging_        = false;
    QPoint       dragOffset_;
    bool         resizing_        = false;
    QPoint       resizeStart_;
    QSize        resizeStartSize_;
    int          resizeEdge_      = 0;         // 1=L 2=R 3=T 4=B 5=TL 6=TR 7=BL 8=BR

    // ── 리사이즈 마진 ───────────────────────────────────────────────
    static constexpr int kResizeMargin = 6;
    int getResizeEdge(const QPoint& pos) const;
};
