#pragma once
#include <QWidget>
#include <QLabel>
#include <QPixmap>
#include <QTimer>
#include <QSlider>
#include "ControlBar.h"  // ClickSeekSlider (HiDPI 클릭 즉시 이동)
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QStackedWidget>
#include <QPropertyAnimation>
#include <QListWidget>
#include <QLineEdit>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QAction>
#include <QFileInfo>

class LyricsWidget;
class MpvCore;

// 음악 파일 메타데이터
struct MusicMeta {
    QString title;
    QString artist;
    QString album;
    QString year;
    QString genre;
    QString trackNum;
    int     sampleRate  = 0;
    int     bitDepth    = 0;
    int     channels    = 0;
    QString codec;
    double  replayGain  = 0.0;
    bool    hasReplayGain = false;
    QPixmap albumArt;
    QString filePath;   // 원본 파일 경로 (LRC 가사 탐색용)
};

// ── EQ 패널 ──────────────────────────────────────────────────────────
class EqPanel : public QWidget {
    Q_OBJECT
public:
    explicit EqPanel(QWidget* parent = nullptr);
    void applyPreset(int idx);
signals:
    void eqChanged(const QVector<double>& gains);
private:
    void buildUI();
    QVector<QSlider*> sliders_;
    QVector<QLabel*>  gainLabels_;
    static const QStringList kBands;
    static const QVector<QVector<double>> kPresets;
};

// ── 재생목록 패널 ─────────────────────────────────────────────────────
class PlaylistPanel : public QWidget {
    Q_OBJECT
public:
    explicit PlaylistPanel(QWidget* parent = nullptr);
    void setTracks(const QStringList& paths);
    void setCurrentIndex(int idx);
signals:
    void trackSelected(int idx);
private:
    QLineEdit*   searchEdit_  = nullptr;
    QListWidget* listWidget_  = nullptr;
    QStringList  allPaths_;
};

// ── MusicWidget ───────────────────────────────────────────────────────
class MusicWidget : public QWidget {
    Q_OBJECT
public:
    explicit MusicWidget(MpvCore* core, QWidget* parent = nullptr);

    void loadMeta(const MusicMeta& meta);
    const MusicMeta& currentMeta() const { return currentMeta_; }
    void updatePosition(double pos, double duration);
    void setPlaying(bool playing);
    // 음악 화면이 실제로 재생·표시될 때만 스펙트럼과 앨범아트 애니메이션을 갱신한다.
    void setVisualizationActive(bool active);
    void updateSpectrum(const QVector<float>& bins);
    // 비트퍼펙트 상태 실시간 표시: 실제 출력 경로(AO 샘플레이트/포맷/Exclusive)
    void setOutputInfo(int outSampleRate, const QString& outFormat, bool exclusive);

    bool isMiniMode() const { return miniMode_; }
    void setMiniMode(bool mini);

signals:
    void seekRequested(double pos);
    void playPauseRequested();
    void prevRequested();
    void nextRequested();
    void shuffleToggled(bool on);
    void repeatToggled(bool on);
    void volumeChanged(int vol);
    void eqRequested();
    void settingsRequested();
    void miniModeRequested();
    void compactModeRequested();   // 소형 모드 (코팩트 플레이어)
    void addToPlaylistRequested(const QString& filePath);

protected:
    void paintEvent(QPaintEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;  // 커서 숨김 방지: 이벤트를 MainWindow에 전달
    void contextMenuEvent(QContextMenuEvent* e) override;  // 우클릭 컨텍스트 메뉴

private slots:
    void onRotationTick();
    void onRightPanelToggle(int panel); // 0=가사 1=EQ 2=재생목록
    void onAbRepeatClicked();           // A-B 반복 버튼 클릭

private:
    void setupUI();
    void setupConnections();
    void updateAlbumArt(const QPixmap& art);
    void updateBlurBackground(const QPixmap& art);
    void extractDominantColor(const QPixmap& art);
    void drawAlbumArt(QPainter& p, const QRect& rect);
    void drawSpectrum(QPainter& p, const QRect& rect);
    QString formatTime(double secs) const;

    MpvCore*      core_         = nullptr;

    // 배경
    QPixmap       blurBg_;
    QColor        dominantColor_;

    // 앨범아트 회전
    QPixmap       albumArtPixmap_;
    QLabel*       albumArtLabel_ = nullptr;
    QWidget*      assistantPanel_ = nullptr;
    qreal         rotationAngle_ = 0.0;
    QTimer*       rotationTimer_ = nullptr;

    // 좌측 패널 텍스트
    QLabel*       titleLabel_     = nullptr;
    QLabel*       artistLabel_    = nullptr;
    QLabel*       albumLabel_     = nullptr;
    QWidget*      badgeRow_       = nullptr;
    QLabel*       codecBadge_     = nullptr;
    QLabel*       bitBadge_       = nullptr;
    QLabel*       rateBadge_      = nullptr;
    QLabel*       chBadge_        = nullptr;
    QLabel*       bpBadge_        = nullptr;

    // 스펙트럼
    QVector<float> specBins_;
    QVector<float> specPeak_;
    QTimer*        peakTimer_   = nullptr;

    // 재생 컨트롤
    QPushButton*  btnShuffle_   = nullptr;
    QPushButton*  btnPrev_      = nullptr;
    QPushButton*  btnPlay_      = nullptr;
    QPushButton*  btnNext_      = nullptr;
    QPushButton*  btnRepeat_    = nullptr;
    QPushButton*  btnAbRepeat_  = nullptr;  // A-B 반복 버튼
    // A-B 반복 상태 (0=비활성, 1=A 지점 설정됨, 2=A-B 구간 반복 중)
    int           abState_      = 0;
    double        abPointA_     = -1.0;
    double        abPointB_     = -1.0;

    // 우측 패널 스택 (0=가사 1=EQ 2=재생목록)
    QStackedWidget* rightStack_    = nullptr;
    LyricsWidget*   lyricsWidget_  = nullptr;
    EqPanel*        eqPanel_       = nullptr;
    PlaylistPanel*  playlistPanel_ = nullptr;

    // 우측 패널 전환 탭 버튼
    QPushButton*  btnShowLyrics_   = nullptr;
    QPushButton*  btnShowEq_       = nullptr;
    QPushButton*  btnShowPlaylist_ = nullptr;

    // 하단 시크바 (볼륨도 여기에 인라인 배치)
    ClickSeekSlider* seekSlider_   = nullptr;
    QLabel*       timeCurrent_  = nullptr;
    QLabel*       timeDuration_ = nullptr;
    // 볼륨 (seekRow 우측 인라인)
    QPushButton*  btnVolume_    = nullptr;
    QSlider*      volSlider_    = nullptr;

    // 하단 컨트롤바
    QLabel*       speedLabel_   = nullptr;
    QPushButton*  btnMini_      = nullptr;
    QPushButton*  btnCompact_   = nullptr;   // 소형 모드 버튼
    QPushButton*  btnSettings_  = nullptr;

    // 하단 상태바
    QLabel*       statusBar_    = nullptr;

    // 상태
    bool          isPlaying_    = false;
    bool          isShuffle_    = false;
    bool          isRepeat_     = false;
    bool          miniMode_     = false;
    bool          visualizationActive_ = false;
    double        duration_     = 0;
    MusicMeta     currentMeta_;
};
