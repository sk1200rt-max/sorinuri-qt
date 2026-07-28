#pragma once
#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QPixmap>
#include "MpvCore.h"

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
    QLabel*  badgeLabel_  = nullptr;  // 포맷 배지 이미지
    QLabel*  modeLabel_   = nullptr;  // THRU / DECODE
    QLabel*  chLabel_     = nullptr;  // 채널
    QLabel*  srLabel_     = nullptr;  // 샘플레이트
    QLabel*  sepLabel_    = nullptr;  // 구분선
    QLabel*  videoLabel_  = nullptr;  // 비디오 정보
    MpvCore* mpv_         = nullptr;

    QString formatChannels(int ch) const;
    QString formatResolution(int w, int h) const;
    QString getBadgeResource(const QString& codec) const;
};
