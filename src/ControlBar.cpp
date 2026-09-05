#include "ControlBar.h"
#include "UiTheme.h"
#include <QIcon>

static const char* SEEK_STYLE =
    "QSlider::groove:horizontal { height: 4px; background: #273334; border-radius: 2px; }"
    "QSlider::handle:horizontal { width: 12px; height: 12px; margin: -4px 0;"
    "  background: #00D4B4; border: 2px solid #0A0F10; border-radius: 6px; }"
    "QSlider::handle:horizontal:hover { width: 15px; height: 15px; margin: -5.5px 0;"
    "  background: #2AE2C5; border-radius: 7px; }"
    "QSlider::sub-page:horizontal { background: #00A991; border-radius: 2px; }";
static const char* VOL_STYLE =
    "QSlider::groove:horizontal { height: 3px; background: #2B3B3C; border-radius: 1px; }"
    "QSlider::handle:horizontal { width: 9px; height: 9px; margin: -3px 0;"
    "  background: #D7E2E0; border-radius: 4px; }"
    "QSlider::handle:horizontal:hover { background: #F2F7F6; }"
    "QSlider::sub-page:horizontal { background: #71807F; border-radius: 1px; }";

QPushButton* ControlBar::makeBtn(const QString& svg, const QString& tip, int size) {
    auto* btn = new QPushButton();
    btn->setToolTip(tip);
    // 공식 SVG 자체는 바꾸지 않고, 영상 위 어두운 오버레이에서도 조작 대상을
    // 즉시 인식할 수 있는 정사각형 터치 영역과 아이콘 비례만 적용한다.
    btn->setFixedSize(size, size);
    btn->setFlat(true);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFocusPolicy(Qt::NoFocus);
    btn->setIcon(QIcon(svg));
    btn->setIconSize(QSize(size >= 36 ? 22 : 18, size >= 36 ? 22 : 18));
    btn->setStyleSheet(SorinuriUi::iconButtonStyle() +
                       "QPushButton { background:rgba(20,31,32,0.32); }"
                       "QPushButton:hover { background:#1C292A; border-color:#2B3B3C; }"
                       "QPushButton:pressed { background:#233536; border-color:#00D4B4; }");
    return btn;
}

ControlBar::ControlBar(QWidget* parent) : QWidget(parent) {
    // 기존 소리누리 SVG 아이콘과 정확한 출력 표시는 보존한다. 컨트롤은 영상 아래의
    // 전역 바가 아니라 영상 내부 오버레이 선반에 들어가므로 배경·테두리는 부모 덱이 맡는다.
    setMinimumHeight(78);
    setMaximumHeight(88);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setStyleSheet("background: transparent; border: none;");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(5, 1, 5, 1);
    root->setSpacing(1);

    auto* seekRow = new QHBoxLayout();
    seekRow->setContentsMargins(0, 0, 0, 0);
    seekRow->setSpacing(9);
    seekSlider_ = new ClickSeekSlider(Qt::Horizontal, this);
    seekSlider_->setRange(0, 10000);
    seekSlider_->setValue(0);
    seekSlider_->setStyleSheet(SEEK_STYLE);
    seekSlider_->setFocusPolicy(Qt::NoFocus);
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
    timeLabel_ = new QLabel("00:00 / 00:00", this);
    timeLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    timeLabel_->setStyleSheet(
        "color: #A3B1B0; font-size: 11px; font-weight: 600;"
        "font-family: 'Cascadia Mono', 'Consolas', monospace; min-width: 108px; background: transparent;");
    seekRow->addWidget(seekSlider_, 1);
    seekRow->addWidget(timeLabel_);
    root->addLayout(seekRow);

    auto* transportSurface = new QWidget(this);
    transportSurface->setObjectName("transportSurface");
    transportSurface->setStyleSheet(
        "QWidget#transportSurface { background: transparent; border: none; }");
    transportRow_ = new QHBoxLayout(transportSurface);
    transportRow_->setContentsMargins(0, 0, 0, 0);
    transportRow_->setSpacing(5);

    btnPrev_ = makeBtn(":/icons/prev.svg", "이전");
    btnPlay_ = makeBtn(":/icons/play.svg", "재생/일시정지 (Space)", 42);
    btnPlay_->setStyleSheet(SorinuriUi::iconButtonStyle() +
                            "QPushButton { background:#102221; border:1px solid #00A991; border-radius:21px; }"
                            "QPushButton:hover { background:#16443E; border-color:#00D4B4; }");
    // 기존 소리누리 공식 SVG 리소스를 그대로 사용한다. 목업 아이콘으로 교체하지 않는다.
    btnNext_ = makeBtn(":/icons/next.svg", "다음");
    btnStop_ = makeBtn(":/icons/stop.svg", "정지");
    btnTracks_ = makeBtn(":/icons/audio.svg", "오디오·자막 트랙 표시/숨김");
    btnMute_ = makeBtn(":/icons/volume.svg", "음소거 (M)");
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
    connect(btnMute_, &QPushButton::clicked, [this]() {
        muted_ = !muted_;
        btnMute_->setIcon(QIcon(muted_ ? ":/icons/mute.svg" : ":/icons/volume.svg"));
        emit muteToggled(muted_);
    });

    volSlider_ = new QSlider(Qt::Horizontal, this);
    volSlider_->setRange(0, 200);
    volSlider_->setValue(100);
    volSlider_->setFixedWidth(84);
    volSlider_->setFocusPolicy(Qt::NoFocus);
    volSlider_->setStyleSheet(VOL_STYLE);
    connect(volSlider_, &QSlider::valueChanged, this, &ControlBar::onVolMoved);
    volLabel_ = new QLabel("100%", this);
    volLabel_->setStyleSheet(
        "color: #A3B1B0; font-size: 10px; font-family: 'Cascadia Mono', 'Consolas', monospace;"
        "min-width: 34px; background: transparent;");

    audioInfoLabel_ = new QLabel(this);
    audioInfoLabel_->setTextFormat(Qt::RichText);
    audioInfoLabel_->setMinimumWidth(180);
    audioInfoLabel_->setMaximumWidth(480);
    audioInfoLabel_->setStyleSheet(
        "color: #A3B1B0; font-size: 10px; font-family: 'Cascadia Mono', 'Consolas', monospace;"
        "background: transparent; border: none; padding: 0 5px;");
    audioInfoLabel_->hide();

    transportRow_->addWidget(btnPrev_);
    transportRow_->addWidget(btnPlay_);
    transportRow_->addWidget(btnNext_);
    transportRow_->addWidget(btnStop_);
    transportRow_->addWidget(btnTracks_);
    transportRow_->addSpacing(10);
    transportRow_->addWidget(btnMute_);
    transportRow_->addWidget(volSlider_);
    transportRow_->addWidget(volLabel_);
    transportRow_->addSpacing(10);
    transportRow_->addWidget(audioInfoLabel_, 1);
    root->addWidget(transportSurface);

    // 스트림·오디오·자막 트랙 선택은 재생 콘솔의 보조 행에만 둔다.
    trackSurface_ = new QWidget(transportSurface);
    trackSurface_->setObjectName("trackSurface");
    trackSurface_->setMinimumWidth(280);
    trackSurface_->setStyleSheet(
        "QWidget#trackSurface { background: #101A1A; border: 1px solid #273B39; border-radius: 7px; }");
    trackRow_ = new QHBoxLayout(trackSurface_);
    trackRow_->setContentsMargins(7, 1, 7, 1);
    trackRow_->setSpacing(5);
    trackRow_->addStretch(1);
    // 트랙 선택은 별도의 두 번째 줄이 아니라 같은 재생 덱의 요청형 컨텍스트 제어다.
    // 기본 재생 화면에는 기존 아이콘 조작부를 우선 보여 주고, 필요할 때만 연다.
    trackSurface_->hide();
    transportRow_->insertWidget(5, trackSurface_, 1);
}

void ControlBar::embedTrackSelector(TrackSelector* selector) {
    if (!selector || !trackRow_) return;
    selector->setParent(this);
    trackRow_->insertWidget(0, selector, 1);
}

void ControlBar::connectMpv(MpvCore* core) {
    connect(core, &MpvCore::audioFormatChanged, this, &ControlBar::onAudioFormatChanged);
    connect(core, &MpvCore::videoInfoChanged, this, &ControlBar::onVideoInfoChanged);
    connect(core, &MpvCore::playbackStopped, this, &ControlBar::onPlaybackStopped);
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
                "<span style='color:#A3B1B0;'>  %3%4%5</span>"
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
        marker->setFixedSize(2, seekSlider_->height() > 0 ? seekSlider_->height() : 16);
        marker->setStyleSheet("background: rgba(255,255,255,0.35); border-radius: 1px;");
        marker->setToolTip(chapter.title);
        marker->setAttribute(Qt::WA_TransparentForMouseEvents);
        const int handleHalf = 6;
        const int usable = seekSlider_->width() - handleHalf * 2;
        const int x = static_cast<int>(handleHalf + ratio * usable) - 1;
        marker->move(x, 0);
        marker->show();
    }
}
