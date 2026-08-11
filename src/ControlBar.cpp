#include "ControlBar.h"
#include "UiTheme.h"

static const char* SEEK_STYLE =
    "QSlider::groove:horizontal { height: 4px; background: #2a2a2a; border-radius: 2px; }"
    "QSlider::handle:horizontal { width: 12px; height: 12px; margin: -4px 0;"
    "  background: #00D4B4; border-radius: 6px; }"
    "QSlider::handle:horizontal:hover { width: 16px; height: 16px; margin: -6px 0;"
    "  background: #20E0C3; border-radius: 8px; }"
    "QSlider::sub-page:horizontal { background: #063B35; border-radius: 2px; }"
    "QSlider::sub-page:horizontal:hover { background: #00D4B4; }";
static const char* VOL_STYLE =
    "QSlider::groove:horizontal { height: 3px; background: #2a2a2a; border-radius: 1px; }"
    "QSlider::handle:horizontal { width: 10px; height: 10px; margin: -4px 0;"
    "  background: #aaa; border-radius: 5px; }"
    "QSlider::handle:horizontal:hover { background: #fff; }"
    "QSlider::sub-page:horizontal { background: #666; border-radius: 1px; }";

QPushButton* ControlBar::makeBtn(const QString& svg, const QString& tip, int size) {
    auto* btn = new QPushButton();
    btn->setToolTip(tip);
    btn->setFixedSize(size, 30);
    btn->setFlat(true);
    btn->setCursor(Qt::ArrowCursor);
    btn->setFocusPolicy(Qt::NoFocus);  // HiDPI: 버튼 클릭 후 포커스가 MainWindow에 유지되도록
    btn->setIcon(QIcon(svg));
    btn->setIconSize(QSize(size == 36 ? 20 : 16, size == 36 ? 20 : 16));
    btn->setStyleSheet(SorinuriUi::iconButtonStyle());
    return btn;
}

QPushButton* ControlBar::makeModeBtn(const QString& text, const QString& tip) {
    auto* btn = new QPushButton(text);
    btn->setToolTip(tip);
    btn->setFixedHeight(22);
    btn->setCursor(Qt::ArrowCursor);
    btn->setFocusPolicy(Qt::NoFocus);  // HiDPI: 버튼 클릭 후 포커스가 MainWindow에 유지되도록
    btn->setStyleSheet(SorinuriUi::modeButtonStyle());
    return btn;
}

ControlBar::ControlBar(QWidget* parent) : QWidget(parent) {
    // 재생 제어와 트랙 선택을 분리해 250% 배율에서도 라벨·버튼이 겹치지 않게 한다.
    setFixedHeight(94);
    setStyleSheet(QString("background: %1; border-top: 1px solid %2;")
                  .arg(SorinuriUi::Surface, SorinuriUi::BorderSoft));

    auto* mainLayout = new QVBoxLayout(this);
    // 상단 여백 5px: 타임라인 슬라이더 핸들이 영상 영역에 겹치지 않도록
    mainLayout->setContentsMargins(8, 5, 8, 4);
    mainLayout->setSpacing(2);

    // 시크 바 행
    auto* seekRow = new QHBoxLayout();
    seekRow->setSpacing(6);

        seekSlider_ = new ClickSeekSlider(Qt::Horizontal, this);
    seekSlider_->setRange(0, 10000);
    seekSlider_->setValue(0);
    seekSlider_->setStyleSheet(SEEK_STYLE);
    seekSlider_->setFocusPolicy(Qt::NoFocus);  // 키 이벤트가 MainWindow로 전달되도록
    // 시크바 클릭 즉시 이동 (드래그 전에도 클릭한 위치로 이동)
    seekSlider_->setTracking(true);
    connect(seekSlider_, &QSlider::sliderPressed,  [this]() {
        seeking_ = true;
        // 클릭 위치로 즉시 이동 (QStyle 통해 정확한 위치 계산)
        emit seeked((seekSlider_->value() / 10000.0) * totalDuration_);
    });
    connect(seekSlider_, &QSlider::sliderMoved,    this, &ControlBar::onSeekMoved);
    connect(seekSlider_, &QSlider::sliderReleased, [this]() {
        seeking_ = false;
        emit seeked((seekSlider_->value() / 10000.0) * totalDuration_);
    });
    timeLabel_ = new QLabel("00:00 / 00:00", this);
    timeLabel_->setStyleSheet(
        "color: #8C9A99; font-size: 11px; font-family: 'Consolas', monospace;"
        "min-width: 110px; background: transparent;");
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
    volSlider_->setFocusPolicy(Qt::NoFocus);  // 키 이벤트가 MainWindow로 전달되도록
    connect(volSlider_, &QSlider::valueChanged, this, &ControlBar::onVolMoved);

    volLabel_ = new QLabel("100%", this);
    volLabel_->setStyleSheet(
        "color: #444; font-size: 10px; font-family: 'Consolas', monospace;"
        "min-width: 34px; background: transparent;");

    // 인라인 오디오 정보 (AudioInfoBar 대체)
    audioInfoLabel_ = new QLabel(this);
    audioInfoLabel_->setStyleSheet(
        "color: #8C9A99; font-size: 10px; font-family: 'Consolas', monospace;"
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
    btnRow_->addWidget(audioInfoLabel_, 1);
    btnRow_->addStretch(1);
    btnRow_->addWidget(btnSettings_);
    mainLayout->addLayout(btnRow_);

    // 트랙 선택은 재생 제어와 독립 행으로 배치한다. 짧은 노트북 폭과 250% DPI에서도
    // 출력 상태·오디오·자막·모드 버튼이 서로 밀거나 잘리지 않는다.
    trackRow_ = new QHBoxLayout();
    trackRow_->setSpacing(4);
    trackRow_->setContentsMargins(0, 0, 0, 0);
    trackRow_->addStretch(1);  // TrackSelector는 embedTrackSelector()에서 앞에 삽입
    trackRow_->addWidget(btnPlayerMode_);
    trackRow_->addSpacing(2);
    trackRow_->addWidget(btnOttMode_);
    mainLayout->addLayout(trackRow_);
}

void ControlBar::embedTrackSelector(TrackSelector* selector) {
    if (!selector || !trackRow_) return;
    selector->setParent(this);
    // 두 번째 행의 좌측 공간을 트랙 선택기에 우선 배정한다.
    trackRow_->insertWidget(0, selector, 2);
}

void ControlBar::connectMpv(MpvCore* core) {
    connect(core, &MpvCore::audioFormatChanged, this, &ControlBar::onAudioFormatChanged);
    connect(core, &MpvCore::videoInfoChanged,   this, &ControlBar::onVideoInfoChanged);
    connect(core, &MpvCore::playbackStopped,    this, &ControlBar::onPlaybackStopped);
}

void ControlBar::onAudioFormatChanged(const QString& codec, int channels,
                                       int sampleRate, const QString& output) {
    const bool isPassthrough = output == QStringLiteral("BITSTREAM");
    const QString mode = isPassthrough ? QStringLiteral("THRU") : QStringLiteral("PCM");
    const QString modeColor = isPassthrough ? SorinuriUi::Mint : QStringLiteral("#F5B942");
    const QString ch = formatChannels(channels);
    const QString sr = sampleRate > 0 ? QString("  %1Hz").arg(sampleRate) : QString();
    const QString codecStr = getDisplayCodec(codec);
    const QString actualOutput = output.isEmpty() ? QStringLiteral("출력 협상 중") : output;

    audioInfoLabel_->setText(
        QString("<span style='color:%1;font-weight:700;'>%2</span>"
                "<span style='color:#8C9A99;'>  %3%4%5</span>"
                "<span style='color:#00D4B4;'>  → %6</span>")
        .arg(modeColor, mode, codecStr,
             ch.isEmpty() ? QString() : "  " + ch, sr, actualOutput));
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

void ControlBar::setChapters(const QVector<ChapterMark>& chapters, double duration) {
    chapters_ = chapters;
    if (!seekSlider_ || chapters.isEmpty() || duration <= 0) return;

    // 기존 챕터 마커 위젯 제거
    for (auto* w : seekSlider_->findChildren<QWidget*>("chapterMarker"))
        w->deleteLater();

    // 챕터 마커 오버레이 위젯 생성
    for (const auto& ch : chapters) {
        double ratio = ch.startSec / duration;
        if (ratio <= 0.0 || ratio >= 1.0) continue;

        auto* marker = new QWidget(seekSlider_);
        marker->setObjectName("chapterMarker");
        marker->setFixedSize(2, seekSlider_->height() > 0 ? seekSlider_->height() : 16);
        marker->setStyleSheet("background: rgba(255,255,255,0.35); border-radius: 1px;");
        marker->setToolTip(ch.title);
        marker->setAttribute(Qt::WA_TransparentForMouseEvents);

        // 슬라이더 핸들 여백 고려한 위치 계산
        int handleHalf = 6;
        int usable = seekSlider_->width() - handleHalf * 2;
        int x = static_cast<int>(handleHalf + ratio * usable) - 1;
        marker->move(x, 0);
        marker->show();
    }
}
