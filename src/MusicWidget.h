#pragma once
#include <QWidget>
#include <QLabel>
#include <QPixmap>
#include <QTimer>
#include <QSlider>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPropertyAnimation>

// 전방 선언
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
    QString codec;          // FLAC, MP3, AAC, ALAC, WAV, DSD...
    double  replayGain  = 0.0;
    bool    hasReplayGain = false;
    QPixmap albumArt;
};

class MusicWidget : public QWidget {
    Q_OBJECT
public:
    explicit MusicWidget(MpvCore* core, QWidget* parent = nullptr);

    // 파일 로드 시 메타데이터 업데이트
    void loadMeta(const MusicMeta& meta);

    // 재생 위치 업데이트 (초 단위)
    void updatePosition(double pos, double duration);

    // 재생/정지 상태 업데이트
    void setPlaying(bool playing);

    // 스펙트럼 데이터 업데이트 (64개 빈)
    void updateSpectrum(const QVector<float>& bins);

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

protected:
    void paintEvent(QPaintEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;

private:
    void setupUI();
    void setupConnections();
    void updateAlbumArt(const QPixmap& art);
    void updateBlurBackground(const QPixmap& art);
    void drawSpectrum(QPainter& p, const QRect& rect);
    QString formatTime(double secs) const;

    MpvCore*      core_         = nullptr;

    // 배경
    QPixmap       blurBg_;

    // 좌측 패널
    QLabel*       albumArtLabel_  = nullptr;
    QLabel*       titleLabel_     = nullptr;
    QLabel*       artistLabel_    = nullptr;
    QLabel*       albumLabel_     = nullptr;
    QWidget*      badgeRow_       = nullptr;
    QLabel*       codecBadge_     = nullptr;
    QLabel*       bitBadge_       = nullptr;
    QLabel*       rateBadge_      = nullptr;
    QLabel*       chBadge_        = nullptr;
    QLabel*       bpBadge_        = nullptr;  // BIT-PERFECT 배지

    // 스펙트럼
    QVector<float> specBins_;
    QVector<float> specPeak_;   // 피크 홀드
    QTimer*        peakTimer_   = nullptr;

    // 재생 컨트롤 (좌측)
    QPushButton*  btnShuffle_   = nullptr;
    QPushButton*  btnPrev_      = nullptr;
    QPushButton*  btnPlay_      = nullptr;
    QPushButton*  btnNext_      = nullptr;
    QPushButton*  btnRepeat_    = nullptr;

    // 우측 패널 - 가사
    LyricsWidget* lyricsWidget_ = nullptr;

    // 하단 시크바
    QSlider*      seekSlider_   = nullptr;
    QLabel*       timeCurrent_  = nullptr;
    QLabel*       timeDuration_ = nullptr;

    // 하단 컨트롤바
    QPushButton*  btnEq_        = nullptr;
    QLabel*       speedLabel_   = nullptr;
    QPushButton*  btnVolume_    = nullptr;
    QSlider*      volSlider_    = nullptr;
    QPushButton*  btnSettings_  = nullptr;

    // 하단 상태바
    QLabel*       statusBar_    = nullptr;

    // 상태
    bool          isPlaying_    = false;
    bool          isShuffle_    = false;
    bool          isRepeat_     = false;
    double        duration_     = 0;
    MusicMeta     currentMeta_;
};
