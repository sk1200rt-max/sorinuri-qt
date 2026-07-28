#pragma once
#include <QWidget>
#include <QLabel>
#include <QTimer>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QPainter>
#include <QPixmap>
#include <QVector>
#include "MpvCore.h"

// ── 채널 레벨 미터 ────────────────────────────────────────────────
class ChannelMeter : public QWidget {
    Q_OBJECT
public:
    explicit ChannelMeter(const QString& label, QWidget* parent = nullptr);
    void setLevel(double dB);
    void reset();

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QString label_;
    double  level_    = -60.0;
    double  peak_     = -60.0;
    int     peakHold_ = 0;
};

// ── 오디오 포맷 배지 (이미지 + 텍스트 혼합) ─────────────────────
class AudioFormatBadge : public QWidget {
    Q_OBJECT
public:
    explicit AudioFormatBadge(QWidget* parent = nullptr);
    void setFormat(const QString& codec, const QString& channels,
                   int sampleRate, bool passthrough);
    void clear();

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QString  codec_;
    QString  channels_;
    int      sampleRate_  = 0;
    bool     passthrough_ = false;
    bool     active_      = false;

    // 포맷별 이미지 캐시
    QPixmap  logoPixmap_;
    void     updateLogoPixmap();

    struct FormatInfo {
        QString displayName;
        QString resourceKey;   // :/badge-xxx-small.png
        QColor  color;
        QColor  bgColor;
    };
    FormatInfo getFormatInfo(const QString& codec) const;
};

// ── 비디오 정보 위젯 ─────────────────────────────────────────────
class VideoInfoWidget : public QWidget {
    Q_OBJECT
public:
    explicit VideoInfoWidget(QWidget* parent = nullptr);
    void setInfo(const QString& codec, int width, int height,
                 double fps, const QString& hdr, const QString& hwdec);
    void clear();

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QString codec_;
    int     width_ = 0, height_ = 0;
    double  fps_   = 0;
    QString hdr_;
    QString hwdec_;
    bool    active_ = false;
};

// ── 오디오 정보 바 전체 ──────────────────────────────────────────
class AudioInfoBar : public QWidget {
    Q_OBJECT
public:
    explicit AudioInfoBar(QWidget* parent = nullptr);
    void connectMpv(MpvCore* core);

public slots:
    void onAudioFormatChanged(const QString& codec, int channels,
                              int sampleRate, const QString& output);
    void onVideoInfoChanged(int width, int height, double fps, const QString& codec);
    void onPlaybackStopped();

private slots:
    void updateMeters();

private:
    AudioFormatBadge*      formatBadge_  = nullptr;
    VideoInfoWidget*       videoInfo_    = nullptr;
    QVector<ChannelMeter*> meters_;
    QLabel*                syncLabel_    = nullptr;
    QLabel*                bitrateLabel_ = nullptr;
    QTimer*                meterTimer_   = nullptr;
    MpvCore*               mpv_          = nullptr;

    // 레벨 미터 시뮬레이션 (패스스루 모드에서 사용)
    double  simLevel_    = -60.0;
    double  simTarget_   = -60.0;
    QTimer* simTimer_    = nullptr;

    static const QStringList CHANNEL_LABELS;
};
