#include "ControlBar.h"
#include "UiTheme.h"

static const char* SEEK_STYLE =
    "QSlider::groove:horizontal { height: 5px; background: #1C292A; border-radius: 2px; }"
    "QSlider::handle:horizontal { width: 12px; height: 12px; margin: -4px 0;"
    "  background: #00D4B4; border: 2px solid #0A0F10; border-radius: 6px; }"
    "QSlider::handle:horizontal:hover { width: 16px; height: 16px; margin: -6px 0;"
    "  background: #2AE2C5; border-radius: 8px; }"
    "QSlider::sub-page:horizontal { background: #0A4940; border-radius: 2px; }"
    "QSlider::sub-page:horizontal:hover { background: #00D4B4; }";
static const char* VOL_STYLE =
    "QSlider::groove:horizontal { height: 4px; background: #2B3B3C; border-radius: 2px; }"
    "QSlider::handle:horizontal { width: 10px; height: 10px; margin: -3px 0;"
    "  background: #A3B1B0; border-radius: 5px; }"
    "QSlider::handle:horizontal:hover { background: #F2F7F6; }"
    "QSlider::sub-page:horizontal { background: #71807F; border-radius: 2px; }";

QPushButton* ControlBar::makeBtn(const QString& svg, const QString& tip, int size) {
    auto* btn = new QPushButton();
    btn->setToolTip(tip);
    btn->setFixedSize(size, 30);
    btn->setFlat(true);
    btn->setCursor(Qt::PointingHandCursor);
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
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFocusPolicy(Qt::NoFocus);  // HiDPI: 버튼 클릭 후 포커스가 MainWindow에 유지되도록
    btn->setStyleSheet(SorinuriUi::modeButtonStyle());
    return btn;
}

ControlBar::ControlBar(QWidget* parent) : QWidget(parent) {
    // 시간축·재생 제어·트랙 선택을 독립된 계층으로 분리한다. 고배율과 넓은
    // 프로젝터 화면에서도 한 줄에 모든 요소를 밀어 넣지 않는다.
    setMinimumHeight(118);
    setMaximumHeight(132);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setStyleSheet(QString("background: %1; border-top: 1px solid %2;")
                  .arg(SorinuriUi::SurfaceAlt, SorinuriUi::BorderSoft));

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(14, 8, 14, 8);
    mainLayout->setSpacing(7);

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
        "color: #A3B1B0; font-size: 11px; font-weight: 600; font-family: 'Cascadia Mono', 'Consolas', monospace;"
        "min-width: 110px; background: transparent;");
    timeLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    seekRow->addWidget(seekSlider_, 1);
    seekRow->addWidget(timeLabel_);
    mainLayout->addLayout(seekRow);

    // 재생 제어는 카드 외곽선으로 둘러싸지 않고, 낮은 대비의 단일 표면으로 정돈한다.
    // 영상 위에서 테두리가 중첩돼 보이지 않으면서도 제어 영역은 명확히 구분된다.
    auto* transportCard = new QWidget(this);
    transportCard->setObjectName("transportCard");
    transportCard->setStyleSheet(QString(
        "QWidget#transportCard { background: %1; border: none; border-radius: 9px; }")
        .arg(SorinuriUi::SurfaceRaised));
    btnRow_ = new QHBoxLayout(transportCard);
    btnRow_->setContentsMargins(7, 3, 7, 3);
    btnRow_->setSpacing(3);

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
        "color: #A3B1B0; font-size: 10px; font-family: 'Cascadia Mono', 'Consolas', monospace;"
        "min-width: 34px; background: transparent;");

    // 인라인 오디오 정보 (AudioInfoBar 대체)
    audioInfoLabel_ = new QLabel(this);
    audioInfoLabel_->setStyleSheet(
        "color: #A3B1B0; font-size: 10px; font-family: 'Cascadia Mono', 'Consolas', monospace;"
        "background: #151F20; border: 1px solid #2B3B3C; border-radius: 7px; padding: 2px 8px;");
    audioInfoLabel_->setTextFormat(Qt::RichText);
    audioInfoLabel_->setMinimumWidth(180);
    audioInfoLabel_->setMaximumWidth(460);
    // 실제 코덱·출력 형식이 확인되기 전에는 '협상 중' 배지를 노출하지 않는다.
    audioInfoLabel_->hide();

    // 모드 버튼
    btnPlayerMode_ = makeModeBtn("파일", "파일 플레이어 모드");
    btnPlayerMode_->setProperty("active", true);
    btnPlayerMode_->style()->unpolish(btnPlayerMode_);
    btnPlayerMode_->style()->polish(btnPlayerMode_);
    btnOttMode_    = makeModeBtn("OTT", "OTT 스트리밍 (Netflix · Disney+ · YouTube)");

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
    mainLayout->addWidget(transportCard);

    // 트랙·모드 선택은 별도 테두리 카드 대신 얇은 구분선과 같은 표면으로 유지한다.
    // 고배율 노트북 폭에서도 오디오·자막·모드 제어가 한 몸처럼 보이게 한다.
    auto* trackCard = new QWidget(this);
    trackCard->setObjectName("trackCard");
    trackCard->setStyleSheet(QString(
        "QWidget#trackCard { background: %1; border: none; border-top: 1px solid %2; border-radius: 0; }")
        .arg(SorinuriUi::SurfaceAlt, SorinuriUi::BorderSoft));
    trackRow_ = new QHBoxLayout(trackCard);
    trackRow_->setSpacing(6);
    trackRow_->setContentsMargins(8, 3, 8, 3);
    trackRow_->addStretch(1);  // TrackSelector는 embedTrackSelector()에서 앞에 삽입
    trackRow_->addWidget(btnPlayerMode_);
    trackRow_->addSpacing(2);
    trackRow_->addWidget(btnOttMode_);
    mainLayout->addWidget(trackCard);
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
    // 파일을 열기 전 또는 출력 장치가 아직 확정되지 않은 순간에는 빈 상태를
    // 강조하지 않는다. 실제 형식이 잡힌 뒤에만 상태 카드를 표시한다.
    if (codec.trimmed().isEmpty() || output.trimmed().isEmpty()) {
        audioInfoLabel_->clear();
        audioInfoLabel_->hide();
        return;
    }
    audioInfoLabel_->show();
    const bool isPassthrough = output == QStringLiteral("BITSTREAM");
    const QString mode = isPassthrough ? QStringLiteral("THRU") : QStringLiteral("PCM");
    const QString modeColor = isPassthrough ? SorinuriUi::Mint : QStringLiteral("#F5B942");
    const QString ch = formatChannels(channels);
    const QString sr = sampleRate > 0 ? QString("  %1Hz").arg(sampleRate) : QString();
    const QString codecStr = getDisplayCodec(codec);
    const QString actualOutput = output.isEmpty() ? QStringLiteral("출력 협상 중") : output;

    audioInfoLabel_->setText(
        QString("<span style='color:%1;font-weight:800;'>%2</span>"
                "<span style='color:#A3B1B0;'>  %3%4%5</span>"
                "<span style='color:#00D4B4;font-weight:700;'>  → %6</span>")
        .arg(modeColor, mode, codecStr,
             ch.isEmpty() ? QString() : "  " + ch, sr, actualOutput));
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
