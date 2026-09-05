#include "ControlBar.h"
#include "UiTheme.h"
#include <QFrame>
#include <QIcon>
#include <QStyle>

static const char* SEEK_STYLE =
    "QSlider::groove:horizontal { height: 2px; background: #2A3233; border: none; }"
    "QSlider::handle:horizontal { width: 10px; height: 10px; margin: -4px 0;"
    "  background: #00D4B4; border: 1px solid #0A0F10; border-radius: 5px; }"
    "QSlider::handle:horizontal:hover { width: 12px; height: 12px; margin: -5px 0;"
    "  background: #2AE2C5; border-radius: 6px; }"
    "QSlider::sub-page:horizontal { background: #00D4B4; border: none; }";
static const char* VOL_STYLE =
    "QSlider::groove:horizontal { height: 2px; background: #3A4546; border: none; }"
    "QSlider::handle:horizontal { width: 9px; height: 9px; margin: -3.5px 0;"
    "  background: #D7E2E0; border: none; border-radius: 4px; }"
    "QSlider::handle:horizontal:hover { background: #F2F7F6; }"
    "QSlider::sub-page:horizontal { background: #00A991; border: none; }";

namespace {
QFrame* makeDivider(QWidget* parent) {
    auto* divider = new QFrame(parent);
    divider->setFrameShape(QFrame::VLine);
    divider->setFixedSize(1, 26);
    divider->setStyleSheet("background:#313A3B; border:none;");
    return divider;
}
}

QPushButton* ControlBar::makeBtn(const QString& svg, const QString& tip, int size) {
    auto* btn = new QPushButton();
    btn->setToolTip(tip);
    btn->setFixedSize(size, size);
    btn->setFlat(true);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFocusPolicy(Qt::NoFocus);
    btn->setIcon(QIcon(svg));
    btn->setIconSize(QSize(size >= 38 ? 19 : 16, size >= 38 ? 19 : 16));
    btn->setStyleSheet(
        "QPushButton { background:transparent; border:1px solid transparent; border-radius:4px; }"
        "QPushButton:hover { background:#232C2D; border-color:#3A4A4B; }"
        "QPushButton:pressed { background:#2B3A3A; border-color:#00D4B4; }");
    return btn;
}

ControlBar::ControlBar(QWidget* parent) : QWidget(parent) {
    // 기본 상태는 전체 폭의 한 줄이며, 영상 화면의 하단에만 붙는다.
    // 재생/오디오 제어의 기존 signal 연결과 공식 SVG 리소스는 유지한다.
    setFixedHeight(64);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setStyleSheet("background: transparent; border: none;");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    seekSlider_ = new ClickSeekSlider(Qt::Horizontal, this);
    seekSlider_->setRange(0, 10000);
    seekSlider_->setValue(0);
    seekSlider_->setFixedHeight(6);
    seekSlider_->setStyleSheet(SEEK_STYLE);
    seekSlider_->setFocusPolicy(Qt::NoFocus);
    seekSlider_->setCursor(Qt::PointingHandCursor);
    seekSlider_->setTracking(true);
    connect(seekSlider_, &QSlider::sliderPressed, [this]() {
        seeking_ = true;
        emit seeked((seekSlider_->value() / 10000.0) * totalDuration_);
    });
    connect(seekSlider_, &QSlider::sliderMoved, this, &ControlBar::onSeekMoved);
    connect(seekSlider_, &QSlider::sliderReleased, [this]() {
        seeking_ = false;
        emit seeked((seekSlider_->value() / 10000.0) * totalDuration_);
    });
    root->addWidget(seekSlider_);

    auto* transportSurface = new QWidget(this);
    transportSurface->setObjectName("transportSurface");
    transportSurface->setFixedHeight(58);
    transportSurface->setStyleSheet("QWidget#transportSurface { background:transparent; border:none; }");
    transportRow_ = new QHBoxLayout(transportSurface);
    transportRow_->setContentsMargins(9, 0, 9, 0);
    transportRow_->setSpacing(6);

    // 한 상태에는 하나의 기본 제어만 표시한다. 이전/다음은 그 좌우에,
    // 정지는 별도 구분선 뒤 낮은 우선순위로 둔다.
    btnPrev_ = makeBtn(":/icons/prev.svg", "이전 항목", 32);
    btnPlay_ = makeBtn(":/icons/play.svg", "재생/일시정지 (Space)", 38);
    btnPlay_->setStyleSheet(
        "QPushButton { background:#152221; border:1px solid #00D4B4; border-radius:19px; }"
        "QPushButton:hover { background:#1D3532; border-color:#2AE2C5; }"
        "QPushButton:pressed { background:#244542; border-color:#00D4B4; }");
    btnNext_ = makeBtn(":/icons/next.svg", "다음 항목", 32);
    btnStop_ = makeBtn(":/icons/stop.svg", "정지", 30);
    btnTracks_ = makeBtn(":/icons/audio.svg", "오디오·자막 트랙", 30);
    btnMute_ = makeBtn(":/icons/volume.svg", "음소거 (M)", 30);
    btnQueue_ = makeBtn(":/icons/playlist.svg", "대기열", 30);
    btnSettings_ = makeBtn(":/icons/settings.svg", "환경 설정", 30);

    connect(btnPrev_, &QPushButton::clicked, this, &ControlBar::prevClicked);
    connect(btnPlay_, &QPushButton::clicked, this, &ControlBar::playPauseClicked);
    connect(btnNext_, &QPushButton::clicked, this, &ControlBar::nextClicked);
    connect(btnStop_, &QPushButton::clicked, this, &ControlBar::stopClicked);
    connect(btnTracks_, &QPushButton::clicked, this, [this]() {
        if (!trackSurface_) return;
        const bool willShow = !trackSurface_->isVisible();
        trackSurface_->setVisible(willShow);
        btnTracks_->setProperty("active", willShow);
        btnTracks_->style()->unpolish(btnTracks_);
        btnTracks_->style()->polish(btnTracks_);
    });
    connect(btnMute_, &QPushButton::clicked, this, [this]() {
        muted_ = !muted_;
        btnMute_->setIcon(QIcon(muted_ ? ":/icons/mute.svg" : ":/icons/volume.svg"));
        emit muteToggled(muted_);
    });
    connect(btnQueue_, &QPushButton::clicked, this, &ControlBar::queueRequested);
    connect(btnSettings_, &QPushButton::clicked, this, &ControlBar::settingsRequested);

    timeLabel_ = new QLabel("00:00 / 00:00", transportSurface);
    timeLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    timeLabel_->setStyleSheet(
        "color:#D6E1DF; font-size:11px; font-weight:650;"
        "font-family:'Cascadia Mono','Consolas',monospace; min-width:96px; background:transparent;");

    audioInfoLabel_ = new QLabel(transportSurface);
    audioInfoLabel_->setTextFormat(Qt::RichText);
    audioInfoLabel_->setMinimumWidth(0);
    audioInfoLabel_->setMaximumWidth(260);
    audioInfoLabel_->setStyleSheet(
        "color:#A3B1B0; font-size:10px; font-family:'Cascadia Mono','Consolas',monospace;"
        "background:transparent; border:none; padding:0 3px;");
    audioInfoLabel_->hide();

    mediaInfoLabel_ = new QLabel(transportSurface);
    mediaInfoLabel_->setAlignment(Qt::AlignCenter | Qt::AlignVCenter);
    mediaInfoLabel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    mediaInfoLabel_->setMinimumWidth(100);
    mediaInfoLabel_->setStyleSheet(
        "color:#AAB8B6; font-size:11px; font-weight:550; background:transparent; padding:0 8px;");
    mediaInfoLabel_->setText(QStringLiteral("재생할 파일을 열어 주세요"));

    volSlider_ = new QSlider(Qt::Horizontal, transportSurface);
    volSlider_->setRange(0, 200);
    volSlider_->setValue(100);
    volSlider_->setFixedWidth(94);
    volSlider_->setFocusPolicy(Qt::NoFocus);
    volSlider_->setCursor(Qt::PointingHandCursor);
    volSlider_->setStyleSheet(VOL_STYLE);
    connect(volSlider_, &QSlider::valueChanged, this, &ControlBar::onVolMoved);
    volLabel_ = new QLabel("100%", transportSurface);
    volLabel_->setStyleSheet(
        "color:#B7C5C3; font-size:10px; font-family:'Cascadia Mono','Consolas',monospace;"
        "min-width:34px; background:transparent;");

    transportRow_->addWidget(btnPrev_);
    transportRow_->addWidget(btnPlay_);
    transportRow_->addWidget(btnNext_);
    transportRow_->addWidget(makeDivider(transportSurface));
    transportRow_->addWidget(btnStop_);
    transportRow_->addWidget(makeDivider(transportSurface));
    transportRow_->addWidget(timeLabel_);
    transportRow_->addWidget(audioInfoLabel_);
    transportRow_->addWidget(mediaInfoLabel_, 1);

    // 트랙 선택은 기본 한 줄을 유지하다가 사용자가 요청할 때만 같은 줄에서 펼친다.
    trackSurface_ = new QWidget(transportSurface);
    trackSurface_->setObjectName("trackSurface");
    trackSurface_->setMinimumWidth(240);
    trackSurface_->setStyleSheet(
        "QWidget#trackSurface { background:#101A1A; border:1px solid #273B39; border-radius:5px; }");
    trackRow_ = new QHBoxLayout(trackSurface_);
    trackRow_->setContentsMargins(6, 1, 6, 1);
    trackRow_->setSpacing(4);
    trackRow_->addStretch(1);
    trackSurface_->hide();
    transportRow_->addWidget(trackSurface_);

    transportRow_->addWidget(btnTracks_);
    transportRow_->addWidget(makeDivider(transportSurface));
    transportRow_->addWidget(btnMute_);
    transportRow_->addWidget(volSlider_);
    transportRow_->addWidget(volLabel_);
    transportRow_->addWidget(makeDivider(transportSurface));
    transportRow_->addWidget(btnQueue_);
    transportRow_->addWidget(btnSettings_);
    root->addWidget(transportSurface);
}

void ControlBar::embedTrackSelector(TrackSelector* selector) {
    if (!selector || !trackRow_) return;
    selector->setParent(trackSurface_);
    trackRow_->insertWidget(0, selector, 1);
}

void ControlBar::connectMpv(MpvCore* core) {
    connect(core, &MpvCore::audioFormatChanged, this, &ControlBar::onAudioFormatChanged);
    connect(core, &MpvCore::videoInfoChanged, this, &ControlBar::onVideoInfoChanged);
    connect(core, &MpvCore::playbackStopped, this, &ControlBar::onPlaybackStopped);
}

void ControlBar::setMediaDetails(const QString& context, const QString& title, const QString& nextTitle) {
    if (!mediaInfoLabel_) return;
    QString detail = title.trimmed().isEmpty() ? QStringLiteral("재생할 파일을 열어 주세요") : title.trimmed();
    if (!context.trimmed().isEmpty()) detail += QStringLiteral("  ·  ") + context.trimmed();
    if (!nextTitle.trimmed().isEmpty()) detail += QStringLiteral("  ·  ") + nextTitle.trimmed();
    mediaInfoLabel_->setText(detail);
    mediaInfoLabel_->setToolTip(detail);
}

void ControlBar::onAudioFormatChanged(const QString& codec, int channels,
                                       int sampleRate, const QString& output) {
    if (codec.trimmed().isEmpty() || output.trimmed().isEmpty()) {
        audioInfoLabel_->clear();
        audioInfoLabel_->hide();
        return;
    }
    audioInfoLabel_->show();
    const bool passthrough = output == QStringLiteral("BITSTREAM");
    const QString mode = passthrough ? QStringLiteral("THRU") : QStringLiteral("PCM");
    const QString modeColor = passthrough ? SorinuriUi::Mint : QStringLiteral("#F5B942");
    const QString channelText = formatChannels(channels);
    const QString sampleRateText = sampleRate > 0 ? QString("  %1Hz").arg(sampleRate) : QString();
    audioInfoLabel_->setText(
        QString("<span style='color:%1;font-weight:800;'>%2</span>"
                "<span style='color:#C2D0CE;'>  %3%4%5</span>"
                "<span style='color:#00D4B4;font-weight:700;'>  → %6</span>")
        .arg(modeColor, mode, getDisplayCodec(codec),
             channelText.isEmpty() ? QString() : "  " + channelText,
             sampleRateText, output));
}

void ControlBar::onVideoInfoChanged(int, int, double, const QString&) {}

void ControlBar::onPlaybackStopped() {
    audioInfoLabel_->clear();
    audioInfoLabel_->hide();
}

void ControlBar::setPlaying(bool playing) {
    btnPlay_->setIcon(QIcon(playing ? ":/icons/pause.svg" : ":/icons/play.svg"));
    btnPlay_->setToolTip(playing ? QStringLiteral("일시정지 (Space)")
                                 : QStringLiteral("재생 (Space)"));
}

void ControlBar::setPosition(double pos, double dur) {
    if (!seeking_ && dur > 0)
        seekSlider_->setValue(static_cast<int>((pos / dur) * 10000));
    timeLabel_->setText(formatTime(pos) + " / " + formatTime(dur));
}

void ControlBar::setDuration(double dur) { totalDuration_ = dur; }

void ControlBar::setVolume(int vol) {
    volSlider_->blockSignals(true);
    volSlider_->setValue(vol);
    volSlider_->blockSignals(false);
    volLabel_->setText(QString("%1%").arg(vol));
}

void ControlBar::setMuted(bool muted) {
    muted_ = muted;
    btnMute_->setIcon(QIcon(muted ? ":/icons/mute.svg" : ":/icons/volume.svg"));
}

void ControlBar::onSeekMoved(int value) {
    if (totalDuration_ > 0)
        timeLabel_->setText(formatTime((value / 10000.0) * totalDuration_)
                            + " / " + formatTime(totalDuration_));
}

void ControlBar::onVolMoved(int value) {
    volLabel_->setText(QString("%1%").arg(value));
    emit volumeChanged(value);
}

QString ControlBar::formatTime(double s) const {
    if (s < 0) s = 0;
    const int t = static_cast<int>(s);
    const int h = t / 3600;
    const int m = (t % 3600) / 60;
    const int sec = t % 60;
    if (h > 0)
        return QString("%1:%2:%3").arg(h).arg(m, 2, 10, QChar('0')).arg(sec, 2, 10, QChar('0'));
    return QString("%1:%2").arg(m, 2, 10, QChar('0')).arg(sec, 2, 10, QChar('0'));
}

QString ControlBar::getDisplayCodec(const QString& codec) const {
    const QString c = codec.toUpper();
    if (c.contains("TRUEHD") && c.contains("ATMOS")) return "TrueHD Atmos";
    if (c.contains("TRUEHD")) return "TrueHD";
    if (c.contains("EAC3") && c.contains("ATMOS")) return "DD+ Atmos";
    if (c.contains("EAC3")) return "DD+";
    if (c.contains("AC3")) return "DD";
    if (c.contains("DTS-HD") || c.contains("DTSHD")) return "DTS-HD MA";
    if (c.contains("DTS")) return "DTS";
    if (c.contains("FLAC")) return "FLAC";
    if (c.contains("PCM")) return "PCM";
    if (c.contains("AAC")) return "AAC";
    if (c.contains("MP3")) return "MP3";
    return codec.isEmpty() ? "" : codec.left(12);
}

QString ControlBar::formatChannels(int ch) const {
    switch (ch) {
    case 1: return "1.0";
    case 2: return "2.0";
    case 6: return "5.1";
    case 8: return "7.1";
    default: return ch > 0 ? QString("%1ch").arg(ch) : "";
    }
}

void ControlBar::setChapters(const QVector<ChapterMark>& chapters, double duration) {
    chapters_ = chapters;
    if (!seekSlider_ || chapters.isEmpty() || duration <= 0) return;
    for (auto* widget : seekSlider_->findChildren<QWidget*>("chapterMarker"))
        widget->deleteLater();
    for (const auto& chapter : chapters) {
        const double ratio = chapter.startSec / duration;
        if (ratio <= 0.0 || ratio >= 1.0) continue;
        auto* marker = new QWidget(seekSlider_);
        marker->setObjectName("chapterMarker");
        marker->setFixedSize(2, seekSlider_->height() > 0 ? seekSlider_->height() : 6);
        marker->setStyleSheet("background:rgba(255,255,255,0.38); border-radius:1px;");
        marker->setToolTip(chapter.title);
        marker->setAttribute(Qt::WA_TransparentForMouseEvents);
        const int handleHalf = 5;
        const int usable = seekSlider_->width() - handleHalf * 2;
        const int x = static_cast<int>(handleHalf + ratio * usable) - 1;
        marker->move(x, 0);
        marker->show();
    }
}
