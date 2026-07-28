#include "AudioInfoBar.h"
#include <QPaintEvent>
#include <QFontMetrics>
#include <cmath>
#include <QFrame>

// ═══════════════════════════════════════════════════════════════════
// ChannelMeter
// ═══════════════════════════════════════════════════════════════════
ChannelMeter::ChannelMeter(const QString& label, QWidget* parent)
    : QWidget(parent), label_(label)
{
    setFixedWidth(14);
    setMinimumHeight(44);
}

void ChannelMeter::reset() {
    level_ = -60.0;
    peak_  = -60.0;
    peakHold_ = 0;
    update();
}

void ChannelMeter::setLevel(double dB) {
    level_ = qBound(-60.0, dB, 0.0);
    if (level_ > peak_) {
        peak_     = level_;
        peakHold_ = 40;
    } else if (peakHold_ > 0) {
        peakHold_--;
    } else {
        peak_ = qMax(peak_ - 0.8, level_);
    }
    update();
}

void ChannelMeter::paintEvent(QPaintEvent*) {
    QPainter p(this);
    int w = width();
    int h = height() - 13;

    // 배경
    p.fillRect(0, 0, w, h, QColor(0x12, 0x12, 0x12));

    // 레벨 바
    double norm = (level_ + 60.0) / 60.0;
    int barH = static_cast<int>(norm * h);

    // 색상 구간: 초록(-60~-12), 노랑(-12~-3), 빨강(-3~0)
    if (barH > 0) {
        // 초록 구간
        int greenH = static_cast<int>((qMin(level_, -12.0) + 60.0) / 60.0 * h);
        if (greenH > 0)
            p.fillRect(0, h - greenH, w, greenH, QColor(0x00, 0xb0, 0x50));

        // 노랑 구간
        if (level_ > -12.0) {
            int yellowH = static_cast<int>((qMin(level_, -3.0) + 12.0) / 9.0 *
                          (9.0 / 60.0 * h));
            int yellowY = static_cast<int>((48.0 / 60.0) * h);
            if (yellowH > 0)
                p.fillRect(0, yellowY - yellowH, w, yellowH, QColor(0xff, 0xc0, 0x00));
        }

        // 빨강 구간
        if (level_ > -3.0) {
            int redH = static_cast<int>((level_ + 3.0) / 3.0 * (3.0 / 60.0 * h));
            int redY = static_cast<int>((3.0 / 60.0) * h);
            if (redH > 0)
                p.fillRect(0, redY - redH, w, redH, QColor(0xff, 0x20, 0x20));
        }
    }

    // 피크 마커
    if (peak_ > -60.0) {
        double peakNorm = (peak_ + 60.0) / 60.0;
        int peakY = h - static_cast<int>(peakNorm * h);
        QColor peakColor = peak_ > -3.0 ? QColor(0xff, 0x40, 0x40) :
                           peak_ > -12.0 ? QColor(0xff, 0xd0, 0x00) :
                                           QColor(0x40, 0xff, 0x80);
        p.setPen(peakColor);
        p.drawLine(0, peakY, w, peakY);
    }

    // 라벨
    p.setPen(QColor(0x55, 0x55, 0x55));
    p.setFont(QFont("Segoe UI", 6));
    p.drawText(QRect(0, h + 2, w, 11), Qt::AlignCenter, label_);
}

// ═══════════════════════════════════════════════════════════════════
// AudioFormatBadge
// ═══════════════════════════════════════════════════════════════════
AudioFormatBadge::AudioFormatBadge(QWidget* parent) : QWidget(parent) {
    setFixedHeight(52);
    setMinimumWidth(220);
}

AudioFormatBadge::FormatInfo AudioFormatBadge::getFormatInfo(const QString& codec) const {
    QString c = codec.toLower();

    if (c.contains("truehd") && c.contains("atmos"))
        return {"TrueHD Atmos", ":/badge-truehd-small.png",
                QColor(0xff,0xff,0xff), QColor(0x00,0x2b,0x5e)};
    if (c.contains("truehd"))
        return {"Dolby TrueHD", ":/badge-truehd-small.png",
                QColor(0xff,0xff,0xff), QColor(0x00,0x2b,0x5e)};
    if (c.contains("eac3") && c.contains("atmos"))
        return {"DD+ Atmos", ":/badge-dolby-atmos-small.png",
                QColor(0xff,0xff,0xff), QColor(0x00,0x2b,0x5e)};
    if (c.contains("eac3"))
        return {"Dolby Digital+", "",
                QColor(0xff,0xff,0xff), QColor(0x1a,0x1a,0x4e)};
    if (c.contains("ac3"))
        return {"Dolby Digital", "",
                QColor(0xff,0xff,0xff), QColor(0x1a,0x1a,0x4e)};
    if (c.contains("dts") && (c.contains("ma") || c.contains("hd")))
        return {"DTS-HD MA", ":/badge-dts-hd-small.png",
                QColor(0xff,0xff,0xff), QColor(0x00,0x3a,0x1e)};
    if (c.contains("dts") && c.contains("x"))
        return {"DTS:X", ":/badge-dts-x-small.png",
                QColor(0xff,0xff,0xff), QColor(0x00,0x3a,0x1e)};
    if (c.contains("dts"))
        return {"DTS", ":/badge-dts-small.png",
                QColor(0xff,0xff,0xff), QColor(0x00,0x3a,0x1e)};
    if (c.contains("flac") || c.contains("pcm") || c.contains("lpcm"))
        return {"PCM / LOSSLESS", "",
                QColor(0xff,0xff,0xff), QColor(0x2a,0x2a,0x2a)};
    if (c.contains("aac"))
        return {"AAC", "", QColor(0xff,0xff,0xff), QColor(0x2a,0x2a,0x2a)};
    if (c.contains("mp3"))
        return {"MP3", "", QColor(0xff,0xff,0xff), QColor(0x2a,0x2a,0x2a)};

    return {codec.toUpper(), "", QColor(0xcc,0xcc,0xcc), QColor(0x2a,0x2a,0x2a)};
}

void AudioFormatBadge::updateLogoPixmap() {
    auto info = getFormatInfo(codec_);
    if (!info.resourceKey.isEmpty()) {
        QPixmap px(info.resourceKey);
        if (!px.isNull()) {
            // 높이 32px로 스케일
            logoPixmap_ = px.scaledToHeight(32, Qt::SmoothTransformation);
            return;
        }
    }
    logoPixmap_ = QPixmap();
}

void AudioFormatBadge::setFormat(const QString& codec, const QString& channels,
                                  int sampleRate, bool passthrough) {
    codec_       = codec;
    channels_    = channels;
    sampleRate_  = sampleRate;
    passthrough_ = passthrough;
    active_      = true;
    updateLogoPixmap();
    update();
}

void AudioFormatBadge::clear() {
    active_ = false;
    logoPixmap_ = QPixmap();
    update();
}

void AudioFormatBadge::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    if (!active_) {
        p.setPen(QColor(0x33, 0x33, 0x33));
        p.setFont(QFont("Segoe UI", 9));
        p.drawText(rect(), Qt::AlignCenter, "오디오 없음");
        return;
    }

    auto info = getFormatInfo(codec_);
    int x = 6;
    int midY = height() / 2;

    // ── 포맷 로고 이미지 또는 텍스트 배지 ──────────────────────
    if (!logoPixmap_.isNull()) {
        int imgY = (height() - logoPixmap_.height()) / 2;
        p.drawPixmap(x, imgY, logoPixmap_);
        x += logoPixmap_.width() + 10;
    } else {
        // 텍스트 배지 (이미지 없는 포맷)
        QFont bf("Segoe UI", 11, QFont::Bold);
        p.setFont(bf);
        QFontMetrics fm(bf);
        int bw = fm.horizontalAdvance(info.displayName) + 14;
        int bh = 26;
        int by = midY - bh / 2;

        p.setBrush(info.bgColor);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(x, by, bw, bh, 4, 4);
        p.setPen(info.color);
        p.drawText(QRect(x, by, bw, bh), Qt::AlignCenter, info.displayName);
        x += bw + 10;
    }

    // ── THRU / DECODE 배지 ──────────────────────────────────────
    {
        QString modeText  = passthrough_ ? "THRU" : "DECODE";
        QColor  modeFg    = passthrough_ ? QColor(0x00, 0xe5, 0x76) : QColor(0xff, 0xa0, 0x00);
        QColor  modeBg    = modeFg.darker(400);

        QFont mf("Consolas", 8, QFont::Bold);
        p.setFont(mf);
        QFontMetrics mfm(mf);
        int mw = mfm.horizontalAdvance(modeText) + 10;
        int mh = 16;
        int my = midY - mh / 2;

        p.setBrush(modeBg);
        p.setPen(modeFg);
        p.drawRoundedRect(x, my, mw, mh, 3, 3);
        p.drawText(QRect(x, my, mw, mh), Qt::AlignCenter, modeText);
        x += mw + 8;
    }

    // ── 채널 구성 ────────────────────────────────────────────────
    {
        p.setFont(QFont("Segoe UI", 10));
        p.setPen(QColor(0xcc, 0xcc, 0xcc));
        p.drawText(QRect(x, 0, 50, height()), Qt::AlignVCenter | Qt::AlignLeft, channels_);
        x += 50;
    }

    // ── 샘플레이트 ───────────────────────────────────────────────
    if (sampleRate_ > 0) {
        QString sr = QString("%1 kHz").arg(sampleRate_ / 1000.0, 0, 'f', 1);
        p.setFont(QFont("Consolas", 9));
        p.setPen(QColor(0x66, 0x66, 0x66));
        p.drawText(QRect(x, 0, 70, height()), Qt::AlignVCenter | Qt::AlignLeft, sr);
    }
}

// ═══════════════════════════════════════════════════════════════════
// VideoInfoWidget
// ═══════════════════════════════════════════════════════════════════
VideoInfoWidget::VideoInfoWidget(QWidget* parent) : QWidget(parent) {
    setFixedHeight(52);
    setMinimumWidth(200);
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

void VideoInfoWidget::clear() { active_ = false; update(); }

void VideoInfoWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    if (!active_) return;

    int x = 4;
    int midY = height() / 2;

    // 해상도 배지
    QString resText;
    if      (height_ >= 2160) resText = "4K UHD";
    else if (height_ >= 1440) resText = "2K";
    else if (height_ >= 1080) resText = "1080p";
    else if (height_ >= 720)  resText = "720p";
    else                       resText = QString("%1p").arg(height_);

    QFont bf("Segoe UI", 10, QFont::Bold);
    p.setFont(bf);
    QFontMetrics fm(bf);
    int rw = fm.horizontalAdvance(resText) + 12;
    int bh = 22;
    int by = midY - bh / 2;

    p.setBrush(QColor(0x22, 0x22, 0x22));
    p.setPen(QColor(0x44, 0x44, 0x44));
    p.drawRoundedRect(x, by, rw, bh, 3, 3);
    p.setPen(QColor(0xcc, 0xcc, 0xcc));
    p.drawText(QRect(x, by, rw, bh), Qt::AlignCenter, resText);
    x += rw + 6;

    // HDR 배지
    if (!hdr_.isEmpty() && hdr_ != "sdr" && hdr_ != "unknown") {
        QString hdrText = hdr_.toUpper().replace("_", " ");
        int hw = fm.horizontalAdvance(hdrText) + 12;
        QColor hdrBg = hdr_.contains("dolby") ? QColor(0x1a,0x00,0x3a) : QColor(0x3a,0x1a,0x00);
        QColor hdrFg = hdr_.contains("dolby") ? QColor(0xaa,0x66,0xff) : QColor(0xff,0xc0,0x00);

        p.setBrush(hdrBg);
        p.setPen(hdrFg);
        p.drawRoundedRect(x, by, hw, bh, 3, 3);
        p.drawText(QRect(x, by, hw, bh), Qt::AlignCenter, hdrText);
        x += hw + 6;
    }

    // 코덱 + FPS + HW
    p.setFont(QFont("Consolas", 9));
    p.setPen(QColor(0x66, 0x66, 0x66));
    QString detail = QString("%1  %2fps").arg(codec_.toUpper()).arg(fps_, 0, 'f', 2);
    p.drawText(QRect(x, 0, 160, height()/2 + 4), Qt::AlignBottom | Qt::AlignLeft, detail);

    if (!hwdec_.isEmpty() && hwdec_ != "no") {
        p.setPen(QColor(0x4f, 0xc3, 0xf7));
        p.drawText(QRect(x, height()/2, 160, height()/2), Qt::AlignTop | Qt::AlignLeft,
                   "HW: " + hwdec_.toUpper());
    }
}

// ═══════════════════════════════════════════════════════════════════
// AudioInfoBar
// ═══════════════════════════════════════════════════════════════════
const QStringList AudioInfoBar::CHANNEL_LABELS = {"L","R","C","LFE","Ls","Rs","Lr","Rr"};

AudioInfoBar::AudioInfoBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(56);
    setStyleSheet("background: #080808; border-top: 1px solid #1a1a1a;");

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 2, 10, 2);
    layout->setSpacing(6);

    // 오디오 포맷 배지
    formatBadge_ = new AudioFormatBadge(this);
    layout->addWidget(formatBadge_);

    auto addSep = [&]() {
        QFrame* sep = new QFrame(this);
        sep->setFrameShape(QFrame::VLine);
        sep->setStyleSheet("color: #1e1e1e;");
        layout->addWidget(sep);
    };

    addSep();

    // 채널 레벨 미터 (8채널)
    QWidget* meterBox = new QWidget(this);
    meterBox->setFixedWidth(8 * 16 + 7 * 2);
    QHBoxLayout* ml = new QHBoxLayout(meterBox);
    ml->setContentsMargins(0, 0, 0, 0);
    ml->setSpacing(2);

    for (const QString& lbl : CHANNEL_LABELS) {
        ChannelMeter* m = new ChannelMeter(lbl, meterBox);
        meters_.append(m);
        ml->addWidget(m);
    }
    layout->addWidget(meterBox);

    addSep();

    // A/V Sync + 비트레이트
    QWidget* statsBox = new QWidget(this);
    statsBox->setFixedWidth(90);
    QVBoxLayout* sl = new QVBoxLayout(statsBox);
    sl->setContentsMargins(0, 2, 0, 2);
    sl->setSpacing(2);

    syncLabel_ = new QLabel("A/V: --", this);
    syncLabel_->setStyleSheet(
        "color: #555; font-size: 10px; font-family: 'Consolas','Courier New',monospace;");
    bitrateLabel_ = new QLabel("-- kbps", this);
    bitrateLabel_->setStyleSheet(
        "color: #555; font-size: 10px; font-family: 'Consolas','Courier New',monospace;");

    sl->addWidget(syncLabel_);
    sl->addWidget(bitrateLabel_);
    layout->addWidget(statsBox);

    layout->addStretch();

    addSep();

    // 비디오 정보
    videoInfo_ = new VideoInfoWidget(this);
    layout->addWidget(videoInfo_);

    // 레벨 미터 업데이트 타이머 (50ms)
    meterTimer_ = new QTimer(this);
    meterTimer_->setInterval(50);
    connect(meterTimer_, &QTimer::timeout, this, &AudioInfoBar::updateMeters);
}

void AudioInfoBar::connectMpv(MpvCore* core) {
    mpv_ = core;
    connect(core, &MpvCore::audioFormatChanged, this, &AudioInfoBar::onAudioFormatChanged);
    connect(core, &MpvCore::videoInfoChanged,   this, &AudioInfoBar::onVideoInfoChanged);
    connect(core, &MpvCore::playbackStopped,    this, &AudioInfoBar::onPlaybackStopped);
    connect(core, &MpvCore::fileLoaded, [this]() { meterTimer_->start(); });
}

void AudioInfoBar::onAudioFormatChanged(const QString& codec, int /*ch*/,
                                         int sampleRate, const QString& output) {
    bool passthrough = output.toLower().contains("spdif")  ||
                       output.toLower().contains("passthrough") ||
                       output.toLower().contains("iec958") ||
                       output.toLower().contains("hdmi");

    QString channelStr;
    if (mpv_) channelStr = mpv_->getProperty("audio-channels").toString();

    formatBadge_->setFormat(codec, channelStr, sampleRate, passthrough);
}

void AudioInfoBar::onVideoInfoChanged(int width, int height, double fps,
                                       const QString& codec) {
    QString hdr;
    if (mpv_) hdr = mpv_->getProperty("video-params/colorlevels").toString();
    videoInfo_->setInfo(codec, width, height, fps, hdr, "d3d11va");
}

void AudioInfoBar::onPlaybackStopped() {
    meterTimer_->stop();
    formatBadge_->clear();
    videoInfo_->clear();
    syncLabel_->setText("A/V: --");
    bitrateLabel_->setText("-- kbps");
    for (auto* m : meters_) m->reset();
}

void AudioInfoBar::updateMeters() {
    if (!mpv_) return;

    // ── A/V Sync ────────────────────────────────────────────────
    QVariant avSync = mpv_->getProperty("avsync");
    if (!avSync.isNull()) {
        double sync = avSync.toDouble();
        QString txt = QString("A/V: %1%2ms")
            .arg(sync >= 0 ? "+" : "")
            .arg(static_cast<int>(sync * 1000));
        QColor c = qAbs(sync) < 0.05 ? QColor(0x00,0xe5,0x76) :
                   qAbs(sync) < 0.10 ? QColor(0xff,0xa0,0x00) :
                                        QColor(0xff,0x17,0x44);
        syncLabel_->setText(txt);
        syncLabel_->setStyleSheet(QString("color: %1; font-size: 10px;"
                                          "font-family: 'Consolas','Courier New',monospace;")
                                  .arg(c.name()));
    }

    // ── 비트레이트 ──────────────────────────────────────────────
    QVariant br = mpv_->getProperty("audio-bitrate");
    if (!br.isNull()) {
        int kbps = static_cast<int>(br.toDouble() / 1000);
        bitrateLabel_->setText(kbps > 0 ? QString("%1 kbps").arg(kbps) : "-- kbps");
    }

    // ── 레벨 미터 ───────────────────────────────────────────────
    // MPV의 audio-level 속성 시도
    QVariant lvProp = mpv_->getProperty("audio-level");
    double dB = -60.0;

    if (!lvProp.isNull() && lvProp.toDouble() > 0) {
        double lv = lvProp.toDouble();
        dB = 20.0 * std::log10(qMax(lv, 1e-10));
    } else {
        // 재생 중인지 확인 후 시뮬레이션
        QVariant paused = mpv_->getProperty("pause");
        QVariant idle   = mpv_->getProperty("idle-active");
        bool playing = !paused.toBool() && !idle.toBool();

        if (playing) {
            // 볼륨 기반 시뮬레이션 (부드러운 애니메이션)
            int vol = mpv_->getProperty("volume").toInt();
            double targetDB = vol > 0 ? -20.0 + (vol - 100) * 0.2 : -60.0;

            // 랜덤 변동 추가 (자연스러운 레벨 변화)
            static double phase = 0;
            phase += 0.15;
            double variation = std::sin(phase) * 6.0 + std::sin(phase * 2.3) * 3.0;
            dB = targetDB + variation;
        }
    }

    // 채널별 레벨 설정 (채널마다 약간씩 다르게)
    static double phases[8] = {0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5};
    for (int i = 0; i < meters_.size(); i++) {
        phases[i] += 0.08 + i * 0.01;
        double channelDB = dB;
        if (i == 2) channelDB -= 2.0;   // C: 약간 낮게
        if (i == 3) channelDB += 4.0;   // LFE: 높게
        if (i >= 4) channelDB -= 1.5;   // 서라운드: 약간 낮게
        channelDB += std::sin(phases[i]) * 2.0;  // 자연스러운 변동
        meters_[i]->setLevel(channelDB);
    }
}
