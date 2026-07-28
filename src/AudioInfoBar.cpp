#include "AudioInfoBar.h"

AudioInfoBar::AudioInfoBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(28);
    setStyleSheet("background: #080808; border-top: 1px solid #141414;");

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(14, 0, 14, 0);
    layout->setSpacing(0);

    auto makeLbl = [this](const QString& style) {
        auto* lbl = new QLabel(this);
        lbl->setStyleSheet(style + " background: transparent;");
        lbl->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        return lbl;
    };

    codecLabel_ = makeLbl(
        "color: #4fc3f7; font-size: 10px; font-weight: 700;"
        "font-family: 'Consolas', monospace; padding: 0 10px 0 0;");

    modeLabel_ = makeLbl(
        "color: #888; font-size: 9px; font-weight: 600;"
        "font-family: 'Consolas', monospace; padding: 0 10px 0 0;");

    chLabel_ = makeLbl(
        "color: #555; font-size: 10px; font-family: 'Consolas', monospace;"
        "padding: 0 10px 0 0;");

    srLabel_ = makeLbl(
        "color: #444; font-size: 10px; font-family: 'Consolas', monospace;"
        "padding: 0 16px 0 0;");

    sepLabel_ = makeLbl("color: #222; font-size: 12px; padding: 0 12px 0 0;");
    sepLabel_->setText("│");
    sepLabel_->hide();

    videoLabel_ = makeLbl(
        "color: #444; font-size: 10px; font-family: 'Consolas', monospace;");

    layout->addWidget(codecLabel_);
    layout->addWidget(modeLabel_);
    layout->addWidget(chLabel_);
    layout->addWidget(srLabel_);
    layout->addWidget(sepLabel_);
    layout->addWidget(videoLabel_);
    layout->addStretch();

    syncLabel_ = makeLbl(
        "color: #2a2a2a; font-size: 9px; font-family: 'Consolas', monospace;");
    syncLabel_->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    layout->addWidget(syncLabel_);
}

void AudioInfoBar::connectMpv(MpvCore* core) {
    mpv_ = core;
    connect(core, &MpvCore::audioFormatChanged, this, &AudioInfoBar::onAudioFormatChanged);
    connect(core, &MpvCore::videoInfoChanged,   this, &AudioInfoBar::onVideoInfoChanged);
    connect(core, &MpvCore::playbackStopped,    this, &AudioInfoBar::onPlaybackStopped);
}

void AudioInfoBar::onAudioFormatChanged(const QString& codec, int channels,
                                         int sampleRate, const QString& output) {
    QString c = codec.toUpper();
    QString displayCodec;

    if      (c.contains("TRUEHD") && c.contains("ATMOS")) displayCodec = "TrueHD Atmos";
    else if (c.contains("TRUEHD"))                         displayCodec = "TrueHD";
    else if (c.contains("EAC3")   && c.contains("ATMOS")) displayCodec = "DD+ Atmos";
    else if (c.contains("EAC3"))                           displayCodec = "DD+";
    else if (c.contains("AC3"))                            displayCodec = "Dolby Digital";
    else if (c.contains("DTS-HD") || c.contains("DTSHD")) displayCodec = "DTS-HD MA";
    else if (c.contains("DTS"))                            displayCodec = "DTS";
    else if (c.contains("PCM") || c.contains("FLAC"))     displayCodec = "PCM";
    else                                                   displayCodec = codec.isEmpty() ? "" : codec;

    codecLabel_->setText(displayCodec);

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
    sepLabel_->setVisible(!info.isEmpty() && !codecLabel_->text().isEmpty());
}

void AudioInfoBar::onPlaybackStopped() {
    codecLabel_->clear();
    modeLabel_->clear();
    chLabel_->clear();
    srLabel_->clear();
    videoLabel_->clear();
    syncLabel_->clear();
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
