#include "AudioInfoBar.h"

AudioInfoBar::AudioInfoBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(32);
    setStyleSheet("background: #080808; border-top: 1px solid #141414;");

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 0, 12, 0);
    layout->setSpacing(0);

    auto makeLbl = [this](const QString& style) {
        auto* lbl = new QLabel(this);
        lbl->setStyleSheet(style + " background: transparent;");
        lbl->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        return lbl;
    };

    // 포맷 배지 이미지 (18px 높이로 표시)
    badgeLabel_ = new QLabel(this);
    badgeLabel_->setStyleSheet("background: transparent;");
    badgeLabel_->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    badgeLabel_->setFixedHeight(22);
    badgeLabel_->setContentsMargins(0, 0, 10, 0);

    // THRU / DECODE
    modeLabel_ = makeLbl(
        "color: #4caf50; font-size: 9px; font-weight: 700;"
        "font-family: 'Consolas', monospace; padding: 0 10px 0 0;");

    // 채널
    chLabel_ = makeLbl(
        "color: #666; font-size: 10px; font-family: 'Consolas', monospace;"
        "padding: 0 10px 0 0;");

    // 샘플레이트
    srLabel_ = makeLbl(
        "color: #444; font-size: 10px; font-family: 'Consolas', monospace;"
        "padding: 0 16px 0 0;");

    // 구분선
    sepLabel_ = makeLbl("color: #1e1e1e; font-size: 14px; padding: 0 12px 0 0;");
    sepLabel_->setText("│");
    sepLabel_->hide();

    // 비디오 정보
    videoLabel_ = makeLbl(
        "color: #444; font-size: 10px; font-family: 'Consolas', monospace;");

    layout->addWidget(badgeLabel_);
    layout->addWidget(modeLabel_);
    layout->addWidget(chLabel_);
    layout->addWidget(srLabel_);
    layout->addWidget(sepLabel_);
    layout->addWidget(videoLabel_);
    layout->addStretch();
}

void AudioInfoBar::connectMpv(MpvCore* core) {
    mpv_ = core;
    connect(core, &MpvCore::audioFormatChanged, this, &AudioInfoBar::onAudioFormatChanged);
    connect(core, &MpvCore::videoInfoChanged,   this, &AudioInfoBar::onVideoInfoChanged);
    connect(core, &MpvCore::playbackStopped,    this, &AudioInfoBar::onPlaybackStopped);
}

QString AudioInfoBar::getBadgeResource(const QString& codec) const {
    QString c = codec.toLower();
    if (c.contains("truehd") && c.contains("atmos")) return ":/badges/dolby-atmos.png";
    if (c.contains("truehd"))                         return ":/badges/truehd.png";
    if (c.contains("eac3")   && c.contains("atmos")) return ":/badges/dolby-atmos.png";
    if (c.contains("eac3"))                           return ":/badges/dd-plus.png";
    if (c.contains("ac3"))                            return ":/badges/dd.png";
    if (c.contains("dts") && (c.contains("ma") || c.contains("hd"))) return ":/badges/dts-hd.png";
    if (c.contains("dts") && c.contains("x"))        return ":/badges/dts-x.png";
    if (c.contains("dts"))                            return ":/badges/dts.png";
    if (c.contains("pcm") || c.contains("flac") || c.contains("lpcm")) return ":/badges/pcm.png";
    return "";
}

void AudioInfoBar::onAudioFormatChanged(const QString& codec, int channels,
                                         int sampleRate, const QString& output) {
    // 배지 이미지 표시
    QString res = getBadgeResource(codec);
    if (!res.isEmpty()) {
        QPixmap px(res);
        if (!px.isNull()) {
            QPixmap scaled = px.scaledToHeight(20, Qt::SmoothTransformation);
            badgeLabel_->setPixmap(scaled);
            badgeLabel_->setFixedWidth(scaled.width() + 10);
        }
    } else {
        // 이미지 없는 포맷은 텍스트로
        badgeLabel_->clear();
        badgeLabel_->setText(codec.toUpper().left(12));
        badgeLabel_->setStyleSheet(
            "background: #1a1a1a; color: #aaa; font-size: 10px; font-weight: 700;"
            "font-family: 'Consolas', monospace; padding: 2px 6px; border-radius: 2px;");
    }

    // THRU / DECODE
    bool isPassthrough = output.toLower().contains("spdif") ||
                         output.toLower().contains("passthrough") ||
                         output.toLower().contains("thru");
    modeLabel_->setText(isPassthrough ? "THRU" : "DECODE");
    modeLabel_->setStyleSheet(
        QString("background: transparent; font-size: 9px; font-weight: 700;"
                "font-family: 'Consolas', monospace; padding: 0 10px 0 0; color: %1;")
        .arg(isPassthrough ? "#4caf50" : "#ff9800"));

    chLabel_->setText(formatChannels(channels));
    srLabel_->setText(sampleRate > 0 ? QString("%1 Hz").arg(sampleRate) : "");
}

void AudioInfoBar::onVideoInfoChanged(int width, int height, double fps, const QString& codec) {
    QString res = formatResolution(width, height);
    QString c = codec.toUpper();
    QString displayCodec;
    if      (c.contains("HEVC") || c.contains("H265")) displayCodec = "H.265";
    else if (c.contains("H264") || c.contains("AVC"))  displayCodec = "H.264";
    else if (c.contains("AV1"))                        displayCodec = "AV1";
    else if (c.contains("VP9"))                        displayCodec = "VP9";
    else                                               displayCodec = codec.left(8);

    QString info;
    if (!res.isEmpty()) info = res;
    if (!displayCodec.isEmpty()) info += (info.isEmpty() ? "" : "  ") + displayCodec;
    if (fps > 0) info += QString("  %1fps").arg(fps, 0, 'f', 2);

    videoLabel_->setText(info);
    sepLabel_->setVisible(!info.isEmpty() && !badgeLabel_->pixmap().isNull());
}

void AudioInfoBar::onPlaybackStopped() {
    badgeLabel_->clear();
    badgeLabel_->setFixedWidth(0);
    modeLabel_->clear();
    chLabel_->clear();
    srLabel_->clear();
    videoLabel_->clear();
    sepLabel_->hide();
}

QString AudioInfoBar::formatChannels(int ch) const {
    switch (ch) {
    case 1: return "1.0";
    case 2: return "2.0";
    case 6: return "5.1";
    case 8: return "7.1";
    default: return ch > 0 ? QString("%1ch").arg(ch) : "";
    }
}

QString AudioInfoBar::formatResolution(int w, int h) const {
    if (w <= 0 || h <= 0) return "";
    if (h >= 2160) return "4K";
    if (h >= 1080) return "1080p";
    if (h >= 720)  return "720p";
    if (h >= 480)  return "480p";
    return QString("%1×%2").arg(w).arg(h);
}
