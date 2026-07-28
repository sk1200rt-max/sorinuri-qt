#include "AudioInfoBar.h"
#include <QPaintEvent>
#include <QFontMetrics>
#include <cmath>

// ─── ChannelMeter ─────────────────────────────────────────────────
ChannelMeter::ChannelMeter(const QString& label, QWidget* parent)
    : QWidget(parent), label_(label)
{
    setFixedWidth(16);
    setMinimumHeight(40);
}

void ChannelMeter::setLevel(double dB) {
    level_ = qBound(-60.0, dB, 0.0);
    if (level_ > peak_) {
        peak_ = level_;
        peakHold_ = 30;
    } else if (peakHold_ > 0) {
        peakHold_--;
    } else {
        peak_ = qMax(peak_ - 1.0, level_);
    }
    update();
}

void ChannelMeter::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int w = width();
    int h = height() - 14;  // 라벨 공간

    // 배경
    p.fillRect(0, 0, w, h, QColor(0x1a, 0x1a, 0x1a));

    // 레벨 바
    double normalized = (level_ + 60.0) / 60.0;  // 0~1
    int barH = static_cast<int>(normalized * h);

    // 색상 그라데이션 (초록 → 노랑 → 빨강)
    QLinearGradient grad(0, h, 0, 0);
    grad.setColorAt(0.0,  QColor(0x00, 0xc8, 0x53));  // 초록
    grad.setColorAt(0.7,  QColor(0xff, 0xd6, 0x00));  // 노랑
    grad.setColorAt(1.0,  QColor(0xff, 0x17, 0x44));  // 빨강

    p.fillRect(0, h - barH, w, barH, grad);

    // 피크 마커
    if (peak_ > -60.0) {
        double peakNorm = (peak_ + 60.0) / 60.0;
        int peakY = h - static_cast<int>(peakNorm * h);
        p.setPen(QColor(0xff, 0xff, 0xff, 200));
        p.drawLine(0, peakY, w, peakY);
    }

    // 라벨
    p.setPen(QColor(0x88, 0x88, 0x88));
    p.setFont(QFont("Segoe UI", 7));
    p.drawText(QRect(0, h + 2, w, 12), Qt::AlignCenter, label_);
}

// ─── AudioFormatBadge ─────────────────────────────────────────────
AudioFormatBadge::AudioFormatBadge(QWidget* parent) : QWidget(parent) {
    setFixedHeight(50);
    setMinimumWidth(200);
}

AudioFormatBadge::FormatInfo AudioFormatBadge::getFormatInfo(const QString& codec) const {
    QString c = codec.toLower();

    if (c.contains("truehd") && c.contains("atmos"))
        return {"TrueHD Atmos", QColor(0xff, 0xff, 0xff), QColor(0x00, 0x2b, 0x5e)};
    if (c.contains("truehd"))
        return {"Dolby TrueHD", QColor(0xff, 0xff, 0xff), QColor(0x00, 0x2b, 0x5e)};
    if (c.contains("eac3") && c.contains("atmos"))
        return {"DD+ Atmos", QColor(0xff, 0xff, 0xff), QColor(0x00, 0x2b, 0x5e)};
    if (c.contains("eac3"))
        return {"Dolby Digital+", QColor(0xff, 0xff, 0xff), QColor(0x1a, 0x1a, 0x4e)};
    if (c.contains("ac3"))
        return {"Dolby Digital", QColor(0xff, 0xff, 0xff), QColor(0x1a, 0x1a, 0x4e)};
    if (c.contains("dts") && (c.contains("ma") || c.contains("hd")))
        return {"DTS-HD MA", QColor(0xff, 0xff, 0xff), QColor(0x00, 0x3a, 0x1e)};
    if (c.contains("dts") && c.contains("x"))
        return {"DTS:X", QColor(0xff, 0xff, 0xff), QColor(0x00, 0x3a, 0x1e)};
    if (c.contains("dts"))
        return {"DTS", QColor(0xff, 0xff, 0xff), QColor(0x00, 0x3a, 0x1e)};
    if (c.contains("flac") || c.contains("pcm") || c.contains("lpcm"))
        return {"PCM", QColor(0xff, 0xff, 0xff), QColor(0x2a, 0x2a, 0x2a)};
    if (c.contains("aac"))
        return {"AAC", QColor(0xff, 0xff, 0xff), QColor(0x2a, 0x2a, 0x2a)};
    if (c.contains("mp3"))
        return {"MP3", QColor(0xff, 0xff, 0xff), QColor(0x2a, 0x2a, 0x2a)};
    if (c.contains("opus"))
        return {"Opus", QColor(0xff, 0xff, 0xff), QColor(0x2a, 0x2a, 0x2a)};

    return {codec.toUpper(), QColor(0xcc, 0xcc, 0xcc), QColor(0x2a, 0x2a, 0x2a)};
}

void AudioFormatBadge::setFormat(const QString& codec, const QString& channels,
                                  int sampleRate, bool passthrough) {
    codec_       = codec;
    channels_    = channels;
    sampleRate_  = sampleRate;
    passthrough_ = passthrough;
    active_      = true;
    update();
}

void AudioFormatBadge::clear() {
    active_ = false;
    update();
}

void AudioFormatBadge::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    if (!active_) {
        p.setPen(QColor(0x44, 0x44, 0x44));
        p.setFont(QFont("Segoe UI", 10));
        p.drawText(rect(), Qt::AlignCenter, "오디오 없음");
        return;
    }

    auto info = getFormatInfo(codec_);

    int x = 4;

    // 포맷 배지
    QFont boldFont("Segoe UI", 12, QFont::Bold);
    p.setFont(boldFont);
    QFontMetrics fm(boldFont);
    int textW = fm.horizontalAdvance(info.displayName) + 16;
    int textH = 28;
    int y = (height() - textH) / 2;

    // 배지 배경
    p.setBrush(info.bgColor);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(x, y, textW, textH, 4, 4);

    // 배지 텍스트
    p.setPen(info.color);
    p.drawText(QRect(x, y, textW, textH), Qt::AlignCenter, info.displayName);
    x += textW + 8;

    // 패스스루/디코딩 표시
    QFont smallFont("Segoe UI", 9, QFont::Bold);
    p.setFont(smallFont);
    QFontMetrics sfm(smallFont);

    QString modeText = passthrough_ ? "THRU" : "DECODE";
    QColor  modeColor = passthrough_ ? QColor(0x00, 0xe5, 0x76) : QColor(0xff, 0xa0, 0x00);
    int modeW = sfm.horizontalAdvance(modeText) + 10;
    int modeH = 18;
    int modeY = (height() - modeH) / 2;

    p.setBrush(modeColor.darker(300));
    p.setPen(modeColor);
    p.drawRoundedRect(x, modeY, modeW, modeH, 3, 3);
    p.drawText(QRect(x, modeY, modeW, modeH), Qt::AlignCenter, modeText);
    x += modeW + 8;

    // 채널 정보
    p.setFont(QFont("Segoe UI", 10));
    p.setPen(QColor(0xcc, 0xcc, 0xcc));
    p.drawText(QRect(x, 0, 60, height()), Qt::AlignVCenter | Qt::AlignLeft, channels_);
    x += 60;

    // 샘플레이트
    if (sampleRate_ > 0) {
        QString srText = QString("%1 kHz").arg(sampleRate_ / 1000);
        p.setPen(QColor(0x88, 0x88, 0x88));
        p.setFont(QFont("Segoe UI", 9));
        p.drawText(QRect(x, 0, 70, height()), Qt::AlignVCenter | Qt::AlignLeft, srText);
    }
}

// ─── VideoInfoWidget ───────────────────────────────────────────────
VideoInfoWidget::VideoInfoWidget(QWidget* parent) : QWidget(parent) {
    setFixedHeight(50);
    setMinimumWidth(180);
}

void VideoInfoWidget::setInfo(const QString& codec, int width, int height,
                               double fps, const QString& hdr, const QString& hwdec) {
    codec_  = codec;
    width_  = width;
    height_ = height;
    fps_    = fps;
    hdr_    = hdr;
    hwdec_  = hwdec;
    active_ = true;
    update();
}

void VideoInfoWidget::clear() {
    active_ = false;
    update();
}

void VideoInfoWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    if (!active_) return;

    int x = 4;

    // 해상도 배지
    QString resText;
    if (height_ >= 2160)      resText = "4K UHD";
    else if (height_ >= 1080) resText = "1080p";
    else if (height_ >= 720)  resText = "720p";
    else                       resText = QString("%1p").arg(height_);

    QFont boldFont("Segoe UI", 11, QFont::Bold);
    p.setFont(boldFont);
    QFontMetrics fm(boldFont);
    int resW = fm.horizontalAdvance(resText) + 14;
    int badgeH = 26;
    int badgeY = (height() - badgeH) / 2;

    p.setBrush(QColor(0x2a, 0x2a, 0x2a));
    p.setPen(QColor(0x44, 0x44, 0x44));
    p.drawRoundedRect(x, badgeY, resW, badgeH, 4, 4);
    p.setPen(QColor(0xcc, 0xcc, 0xcc));
    p.drawText(QRect(x, badgeY, resW, badgeH), Qt::AlignCenter, resText);
    x += resW + 6;

    // HDR 배지
    if (!hdr_.isEmpty() && hdr_ != "sdr") {
        QString hdrText = hdr_.toUpper();
        int hdrW = fm.horizontalAdvance(hdrText) + 14;

        QColor hdrBg = hdr_.contains("dolby") ? QColor(0x1a, 0x00, 0x3a) : QColor(0x3a, 0x1a, 0x00);
        QColor hdrFg = hdr_.contains("dolby") ? QColor(0xaa, 0x66, 0xff) : QColor(0xff, 0xc0, 0x00);

        p.setBrush(hdrBg);
        p.setPen(hdrFg);
        p.drawRoundedRect(x, badgeY, hdrW, badgeH, 4, 4);
        p.drawText(QRect(x, badgeY, hdrW, badgeH), Qt::AlignCenter, hdrText);
        x += hdrW + 6;
    }

    // 코덱 + FPS
    p.setFont(QFont("Segoe UI", 9));
    p.setPen(QColor(0x88, 0x88, 0x88));
    QString detail = QString("%1  %2fps").arg(codec_.toUpper()).arg(fps_, 0, 'f', 3);
    p.drawText(QRect(x, 0, 150, height()), Qt::AlignVCenter | Qt::AlignLeft, detail);

    // HW 디코딩 표시
    if (!hwdec_.isEmpty() && hwdec_ != "no") {
        p.setPen(QColor(0x4f, 0xc3, 0xf7));
        p.drawText(QRect(x, height()/2, 150, height()/2), Qt::AlignVCenter | Qt::AlignLeft,
                   "HW: " + hwdec_.toUpper());
    }
}

// ─── AudioInfoBar ─────────────────────────────────────────────────
const QStringList AudioInfoBar::CHANNEL_LABELS = {"L", "R", "C", "LFE", "Ls", "Rs", "Lr", "Rr"};

AudioInfoBar::AudioInfoBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(56);
    setStyleSheet("background: #080808; border-top: 1px solid #1a1a1a;");

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 2, 8, 2);
    layout->setSpacing(8);

    // 오디오 포맷 배지
    formatBadge_ = new AudioFormatBadge(this);
    layout->addWidget(formatBadge_);

    // 구분선
    QFrame* sep1 = new QFrame(this);
    sep1->setFrameShape(QFrame::VLine);
    sep1->setStyleSheet("color: #2a2a2a;");
    layout->addWidget(sep1);

    // 채널 레벨 미터 (8채널)
    QWidget* meterContainer = new QWidget(this);
    meterContainer->setFixedWidth(8 * 18);
    QHBoxLayout* meterLayout = new QHBoxLayout(meterContainer);
    meterLayout->setContentsMargins(0, 0, 0, 0);
    meterLayout->setSpacing(2);

    for (const QString& label : CHANNEL_LABELS) {
        ChannelMeter* meter = new ChannelMeter(label, meterContainer);
        meters_.append(meter);
        meterLayout->addWidget(meter);
    }
    layout->addWidget(meterContainer);

    // 구분선
    QFrame* sep2 = new QFrame(this);
    sep2->setFrameShape(QFrame::VLine);
    sep2->setStyleSheet("color: #2a2a2a;");
    layout->addWidget(sep2);

    // A/V Sync + 비트레이트
    QWidget* statsWidget = new QWidget(this);
    QVBoxLayout* statsLayout = new QVBoxLayout(statsWidget);
    statsLayout->setContentsMargins(0, 0, 0, 0);
    statsLayout->setSpacing(2);

    syncLabel_ = new QLabel("A/V: --", this);
    syncLabel_->setStyleSheet("color: #666; font-size: 10px; font-family: 'Consolas';");
    bitrateLabel_ = new QLabel("-- kbps", this);
    bitrateLabel_->setStyleSheet("color: #666; font-size: 10px; font-family: 'Consolas';");

    statsLayout->addWidget(syncLabel_);
    statsLayout->addWidget(bitrateLabel_);
    layout->addWidget(statsWidget);

    layout->addStretch();

    // 구분선
    QFrame* sep3 = new QFrame(this);
    sep3->setFrameShape(QFrame::VLine);
    sep3->setStyleSheet("color: #2a2a2a;");
    layout->addWidget(sep3);

    // 비디오 정보
    videoInfo_ = new VideoInfoWidget(this);
    layout->addWidget(videoInfo_);

    // 레벨 미터 업데이트 타이머 (60fps)
    meterTimer_ = new QTimer(this);
    meterTimer_->setInterval(16);
    connect(meterTimer_, &QTimer::timeout, this, &AudioInfoBar::updateMeters);
}

void AudioInfoBar::connectMpv(MpvCore* core) {
    mpv_ = core;
    connect(core, &MpvCore::audioFormatChanged, this, &AudioInfoBar::onAudioFormatChanged);
    connect(core, &MpvCore::videoInfoChanged,   this, &AudioInfoBar::onVideoInfoChanged);
    connect(core, &MpvCore::playbackStopped,    this, &AudioInfoBar::onPlaybackStopped);
    connect(core, &MpvCore::fileLoaded, [this]() {
        meterTimer_->start();
    });
}

void AudioInfoBar::onAudioFormatChanged(const QString& codec, int /*channels*/,
                                         int sampleRate, const QString& output) {
    // 패스스루 여부 판단 (출력 장치 이름에 "spdif" 또는 "passthrough" 포함)
    bool passthrough = output.toLower().contains("spdif") ||
                       output.toLower().contains("passthrough") ||
                       output.toLower().contains("iec958") ||
                       output.toLower().contains("hdmi");

    // 채널 수 파싱
    QString channelStr = (QString) output;
    if (channelStr.isEmpty()) {
        if (mpv_) {
            QVariant ch = mpv_->getProperty("audio-channels");
            channelStr = ch.toString();
        }
    }

    formatBadge_->setFormat(codec, channelStr, sampleRate, passthrough);
}

void AudioInfoBar::onVideoInfoChanged(int width, int height, double fps,
                                       const QString& codec) {
    // HDR 정보 가져오기
    QString hdr;
    if (mpv_) {
        QVariant hdrProp = mpv_->getProperty("video-params/colorlevels");
        hdr = hdrProp.toString();
    }
    videoInfo_->setInfo(codec, width, height, fps, hdr, "d3d11va");
}

void AudioInfoBar::onPlaybackStopped() {
    meterTimer_->stop();
    formatBadge_->clear();
    videoInfo_->clear();
    syncLabel_->setText("A/V: --");
    bitrateLabel_->setText("-- kbps");
    for (auto* m : meters_) m->setLevel(-60.0);
}

void AudioInfoBar::updateMeters() {
    if (!mpv_) return;

    // A/V sync
    QVariant avSync = mpv_->getProperty("avsync");
    if (!avSync.isNull()) {
        double sync = avSync.toDouble();
        QString syncText = QString("A/V: %1%2ms")
            .arg(sync >= 0 ? "+" : "")
            .arg(static_cast<int>(sync * 1000));
        QColor syncColor = qAbs(sync) < 0.05 ? QColor(0x00, 0xe5, 0x76) :
                           qAbs(sync) < 0.1  ? QColor(0xff, 0xa0, 0x00) :
                                               QColor(0xff, 0x17, 0x44);
        syncLabel_->setText(syncText);
        syncLabel_->setStyleSheet(QString("color: %1; font-size: 10px; font-family: 'Consolas';")
                                  .arg(syncColor.name()));
    }

    // 비트레이트
    QVariant bitrate = mpv_->getProperty("audio-bitrate");
    if (!bitrate.isNull()) {
        int kbps = static_cast<int>(bitrate.toDouble() / 1000);
        bitrateLabel_->setText(QString("%1 kbps").arg(kbps));
    }

    // 레벨 미터 (MPV audio-level 속성 사용)
    // 실제 레벨은 MPV의 audio-level 속성으로 읽음
    // 채널별 레벨은 현재 MPV API에서 직접 지원하지 않으므로
    // 전체 레벨을 기반으로 시뮬레이션
    QVariant levelProp = mpv_->getProperty("audio-level");
    if (!levelProp.isNull()) {
        double level = levelProp.toDouble();
        // dB 변환 (0~1 → -60~0 dB)
        double dB = level > 0 ? 20.0 * std::log10(level) : -60.0;

        // 채널별로 약간씩 다르게 표시 (L/R은 동일, C는 약간 낮게, LFE는 별도)
        for (int i = 0; i < meters_.size(); i++) {
            double channelDB = dB;
            if (i == 2) channelDB -= 3.0;   // C: -3dB
            if (i == 3) channelDB += 6.0;   // LFE: +6dB
            if (i >= 4) channelDB -= 1.5;   // 서라운드: -1.5dB
            meters_[i]->setLevel(channelDB);
        }
    }
}
