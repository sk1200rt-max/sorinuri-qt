#include "ControlBar.h"

static const char* SEEK_STYLE =
    "QSlider::groove:horizontal { height: 3px; background: #1e1e1e; border-radius: 1px; }"
    "QSlider::handle:horizontal { width: 10px; height: 10px; margin: -4px 0;"
    "  background: #4fc3f7; border-radius: 5px; }"
    "QSlider::sub-page:horizontal { background: #1565c0; border-radius: 1px; }";

static const char* VOL_STYLE =
    "QSlider::groove:horizontal { height: 3px; background: #1e1e1e; border-radius: 1px; }"
    "QSlider::handle:horizontal { width: 8px; height: 8px; margin: -3px 0;"
    "  background: #888; border-radius: 4px; }"
    "QSlider::sub-page:horizontal { background: #444; border-radius: 1px; }";

QPushButton* ControlBar::makeBtn(const QString& svg, const QString& tip, int size) {
    auto* btn = new QPushButton();
    btn->setToolTip(tip);
    btn->setFixedSize(size, 30);
    btn->setFlat(true);
    btn->setCursor(Qt::ArrowCursor);
    btn->setIcon(QIcon(svg));
    btn->setIconSize(QSize(size == 36 ? 20 : 16, size == 36 ? 20 : 16));
    btn->setStyleSheet(
        "QPushButton { background: transparent; border: none; border-radius: 3px; }"
        "QPushButton:hover { background: #252525; }"
        "QPushButton:pressed { background: #1565c0; }");
    return btn;
}

QPushButton* ControlBar::makeModeBtn(const QString& text, const QString& tip) {
    auto* btn = new QPushButton(text);
    btn->setToolTip(tip);
    btn->setFixedHeight(22);
    btn->setCursor(Qt::ArrowCursor);
    btn->setStyleSheet(
        "QPushButton { background: #1a1a1a; color: #666; border: 1px solid #2a2a2a;"
        "  border-radius: 3px; padding: 0 10px; font-size: 11px; }"
        "QPushButton:hover { color: #ccc; border-color: #333; }"
        "QPushButton[active=true] { background: #1565c0; color: #fff; border-color: #1976d2; }");
    return btn;
}

ControlBar::ControlBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(62);
    // border-top 제거: 영상과 컨트롤바 사이에 불필요한 업스라인이 보이는 문제 해결
    setStyleSheet("background: #0d0d0d;");

    auto* mainLayout = new QVBoxLayout(this);
    // 상단 여백 5px: 타임라인 슬라이더 핸들이 영상 영역에 겹치지 않도록
    mainLayout->setContentsMargins(8, 5, 8, 4);
    mainLayout->setSpacing(2);

    // 시크 바 행
    auto* seekRow = new QHBoxLayout();
    seekRow->setSpacing(6);

    seekSlider_ = new QSlider(Qt::Horizontal, this);
    seekSlider_->setRange(0, 10000);
    seekSlider_->setValue(0);
    seekSlider_->setStyleSheet(SEEK_STYLE);
    connect(seekSlider_, &QSlider::sliderPressed,  [this]() { seeking_ = true; });
    connect(seekSlider_, &QSlider::sliderMoved,    this, &ControlBar::onSeekMoved);
    connect(seekSlider_, &QSlider::sliderReleased, [this]() {
        seeking_ = false;
        emit seeked((seekSlider_->value() / 10000.0) * totalDuration_);
    });

    timeLabel_ = new QLabel("00:00 / 00:00", this);
    timeLabel_->setStyleSheet(
        "color: #444; font-size: 10px; font-family: 'Consolas', monospace;"
        "min-width: 100px; background: transparent;");
    timeLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    seekRow->addWidget(seekSlider_, 1);
    seekRow->addWidget(timeLabel_);
    mainLayout->addLayout(seekRow);

    // 버튼 행
    btnRow_ = new QHBoxLayout();
    btnRow_->setSpacing(2);

    btnOpen_ = makeBtn(":/icons/open.svg",   "파일 열기 (Ctrl+O)");
    btnPrev_ = makeBtn(":/icons/prev.svg",   "이전");
    btnPlay_ = makeBtn(":/icons/play.svg",   "재생/일시정지 (Space)", 36);
    btnNext_ = makeBtn(":/icons/next.svg",   "다음");
    btnStop_ = makeBtn(":/icons/stop.svg",   "정지");
    btnMute_ = makeBtn(":/icons/volume.svg", "음소거 (M)");

    connect(btnOpen_, &QPushButton::clicked, this, &ControlBar::openFileClicked);
    connect(btnPrev_, &QPushButton::clicked, this, &ControlBar::prevClicked);
    connect(btnPlay_, &QPushButton::clicked, this, &ControlBar::playPauseClicked);
    connect(btnNext_, &QPushButton::clicked, this, &ControlBar::nextClicked);
    connect(btnStop_, &QPushButton::clicked, this, &ControlBar::stopClicked);
    connect(btnMute_, &QPushButton::clicked, [this]() {
        muted_ = !muted_;
        btnMute_->setIcon(QIcon(muted_ ? ":/icons/mute.svg" : ":/icons/volume.svg"));
        emit muteToggled(muted_);
    });

    volSlider_ = new QSlider(Qt::Horizontal, this);
    volSlider_->setRange(0, 200);
    volSlider_->setValue(100);
    volSlider_->setFixedWidth(80);
    volSlider_->setStyleSheet(VOL_STYLE);
    connect(volSlider_, &QSlider::valueChanged, this, &ControlBar::onVolMoved);

    volLabel_ = new QLabel("100%", this);
    volLabel_->setStyleSheet(
        "color: #444; font-size: 10px; font-family: 'Consolas', monospace;"
        "min-width: 34px; background: transparent;");

    // 인라인 오디오 정보 (AudioInfoBar 대체)
    audioInfoLabel_ = new QLabel(this);
    audioInfoLabel_->setStyleSheet(
        "color: #555; font-size: 10px; font-family: 'Consolas', monospace;"
        "background: transparent; padding: 0 8px;");
    audioInfoLabel_->setTextFormat(Qt::RichText);

    // 모드 버튼
    btnPlayerMode_ = makeModeBtn("▶  파일", "파일 플레이어 모드");
    btnPlayerMode_->setProperty("active", true);
    btnPlayerMode_->style()->unpolish(btnPlayerMode_);
    btnPlayerMode_->style()->polish(btnPlayerMode_);
    btnOttMode_    = makeModeBtn("🌐  OTT", "OTT 스트리밍 (Netflix · Disney+ · YouTube)");

    connect(btnPlayerMode_, &QPushButton::clicked, this, &ControlBar::playerModeClicked);
    connect(btnOttMode_,    &QPushButton::clicked, this, &ControlBar::ottModeClicked);

    btnSettings_ = makeBtn(":/icons/settings.svg", "설정");
    connect(btnSettings_, &QPushButton::clicked, this, &ControlBar::settingsClicked);

    btnRow_->addWidget(btnOpen_);
    btnRow_->addSpacing(4);
    btnRow_->addWidget(btnPrev_);
    btnRow_->addWidget(btnPlay_);
    btnRow_->addWidget(btnNext_);
    btnRow_->addWidget(btnStop_);
    btnRow_->addSpacing(8);
    btnRow_->addWidget(btnMute_);
    btnRow_->addWidget(volSlider_);
    btnRow_->addWidget(volLabel_);
    btnRow_->addSpacing(4);
    btnRow_->addWidget(audioInfoLabel_);
    btnRow_->addStretch(1);
    // TrackSelector는 embedTrackSelector()에서 삽입됨
    btnRow_->addStretch(1);
    btnRow_->addWidget(btnPlayerMode_);
    btnRow_->addSpacing(2);
    btnRow_->addWidget(btnOttMode_);
    btnRow_->addSpacing(6);
    btnRow_->addWidget(btnSettings_);

    mainLayout->addLayout(btnRow_);
}

void ControlBar::embedTrackSelector(TrackSelector* selector) {
    if (!selector || !btnRow_) return;
    // 두 stretch 사이에 TrackSelector 삽입
    // 레이아웃 끝: [stretch] [PlayerMode] [2] [OTT] [6] [Settings]
    // 우측 stretch 위치 = count - 6
    int insertPos = btnRow_->count() - 6;
    if (insertPos < 0) insertPos = 0;
    selector->setParent(this);
    btnRow_->insertWidget(insertPos, selector, 2);
}

void ControlBar::connectMpv(MpvCore* core) {
    connect(core, &MpvCore::audioFormatChanged, this, &ControlBar::onAudioFormatChanged);
    connect(core, &MpvCore::videoInfoChanged,   this, &ControlBar::onVideoInfoChanged);
    connect(core, &MpvCore::playbackStopped,    this, &ControlBar::onPlaybackStopped);
}

void ControlBar::onAudioFormatChanged(const QString& codec, int channels,
                                       int sampleRate, const QString& output) {
    bool isPassthrough = !output.isEmpty() &&
        !output.toLower().contains("pcm") &&
        !output.toLower().contains("float") &&
        !output.toLower().contains("s16") &&
        !output.toLower().contains("s32");

    QString mode = isPassthrough ? "THRU" : "DECODE";
    QString modeColor = isPassthrough ? "#4caf50" : "#ff9800";
    QString ch = formatChannels(channels);
    QString sr = sampleRate > 0 ? QString("  %1Hz").arg(sampleRate) : "";
    QString codecStr = getDisplayCodec(codec);

    audioInfoLabel_->setText(
        QString("<span style='color:%1;font-weight:700;'>%2</span>"
                "<span style='color:#555;'>  %3%4%5</span>")
        .arg(modeColor, mode, codecStr,
             ch.isEmpty() ? "" : "  " + ch, sr));
}

void ControlBar::onVideoInfoChanged(int, int, double, const QString&) {}

void ControlBar::onPlaybackStopped() {
    audioInfoLabel_->clear();
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
    int t = static_cast<int>(s);
    int h = t / 3600, m = (t % 3600) / 60, sec = t % 60;
    if (h > 0)
        return QString("%1:%2:%3").arg(h).arg(m,2,10,QChar('0')).arg(sec,2,10,QChar('0'));
    return QString("%1:%2").arg(m,2,10,QChar('0')).arg(sec,2,10,QChar('0'));
}

QString ControlBar::getDisplayCodec(const QString& codec) const {
    QString c = codec.toUpper();
    if (c.contains("TRUEHD") && c.contains("ATMOS")) return "TrueHD Atmos";
    if (c.contains("TRUEHD"))                         return "TrueHD";
    if (c.contains("EAC3")   && c.contains("ATMOS")) return "DD+ Atmos";
    if (c.contains("EAC3"))                           return "DD+";
    if (c.contains("AC3"))                            return "DD";
    if (c.contains("DTS-HD") || c.contains("DTSHD")) return "DTS-HD MA";
    if (c.contains("DTS"))                            return "DTS";
    if (c.contains("FLAC"))                           return "FLAC";
    if (c.contains("PCM"))                            return "PCM";
    if (c.contains("AAC"))                            return "AAC";
    if (c.contains("MP3"))                            return "MP3";
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
