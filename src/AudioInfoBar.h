#pragma once
#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>
#include "MpvCore.h"

// ── 오디오 정보 바 (레벨미터 없음, 심플) ─────────────────────────
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

private:
    QLabel* codecLabel_   = nullptr;  // 오디오 코덱 (DTS-HD MA / TrueHD 등)
    QLabel* modeLabel_    = nullptr;  // THRU / DECODE
    QLabel* chLabel_      = nullptr;  // 채널 (7.1 / 5.1 등)
    QLabel* srLabel_      = nullptr;  // 샘플레이트
    QLabel* sepLabel_     = nullptr;  // 구분선
    QLabel* videoLabel_   = nullptr;  // 비디오 정보 (4K H265 / 1080p H264 등)
    QLabel* syncLabel_    = nullptr;  // A/V Sync
    MpvCore* mpv_         = nullptr;

    QString formatChannels(int ch) const;
    QString formatResolution(int w, int h) const;
};
