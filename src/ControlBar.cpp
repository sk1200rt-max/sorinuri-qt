#include "ControlBar.h"
#include <QIcon>

static const QString SEEK_STYLE = R"(
QSlider::groove:horizontal {
    height: 3px; background: #1e1e1e; border-radius: 2px;
}
QSlider::sub-page:horizontal {
    background: #4fc3f7; border-radius: 2px;
}
QSlider::handle:horizontal {
    width: 10px; height: 10px; margin: -4px 0;
    background: #ffffff; border-radius: 5px;
}
QSlider::handle:horizontal:hover { background: #4fc3f7; }
)";

static const QString VOL_STYLE = R"(
QSlider::groove:horizontal {
    height: 3px; background: #1e1e1e; border-radius: 2px;
}
QSlider::sub-page:horizontal {
    background: #4fc3f7; border-radius: 2px;
}
QSlider::handle:horizontal {
    width: 8px; height: 8px; margin: -3px 0;
    background: #4fc3f7; border-radius: 4px;
}
)";

QPushButton* ControlBar::makeBtn(const QString& svg, const QString& tip, int size) {
    auto* btn = new QPushButton();
    btn->setToolTip(tip);
    btn->setFixedSize(size, size);
    btn->setFlat(true);
    btn->setCursor(Qt::ArrowCursor);
    btn->setIcon(QIcon(svg));
    btn->setIconSize(QSize(size - 10, size - 10));
    btn->setStyleSheet(
        "QPushButton { background: transparent; border: none; border-radius: 3px; }"
        "QPushButton:hover { background: #1e1e1e; }"
        "QPushButton:pressed { background: #2a2a2a; }");
    return btn;
}

ControlBar::ControlBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(68);
    setStyleSheet("background: #0a0a0a; border-top: 1px solid #181818;");

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(14, 5, 14, 5);
    mainLayout->setSpacing(5);

    // ── 진행바 ─────────────────────────────────────────────────
    auto* seekRow = new QHBoxLayout();
    seekRow->setSpacing(10);

    seekSlider_ = new QSlider(Qt::Horizontal, this);
    seekSlider_->setRange(0, 10000);
    seekSlider_->setStyleSheet(SEEK_STYLE);
    connect(seekSlider_, &QSlider::sliderMoved,   this, &ControlBar::onSeekMoved);
    connect(seekSlider_, &QSlider::sliderPressed,  [this]() { seeking_ = true; });
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

    // ── 버튼 행 ────────────────────────────────────────────────
    auto* btnRow = new QHBoxLayout();
    btnRow->setSpacing(2);

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

    // 볼륨 슬라이더
    volSlider_ = new QSlider(Qt::Horizontal, this);
    volSlider_->setRange(0, 200);
    volSlider_->setValue(100);
    volSlider_->setFixedWidth(90);
    volSlider_->setStyleSheet(VOL_STYLE);
    connect(volSlider_, &QSlider::valueChanged, this, &ControlBar::onVolMoved);

    volLabel_ = new QLabel("100%", this);
    volLabel_->setStyleSheet(
        "color: #444; font-size: 10px; font-family: 'Consolas', monospace;"
        "min-width: 36px; background: transparent;");

    btnRow->addWidget(btnOpen_);
    btnRow->addSpacing(6);
    btnRow->addWidget(btnPrev_);
    btnRow->addWidget(btnPlay_);
    btnRow->addWidget(btnNext_);
    btnRow->addWidget(btnStop_);
    btnRow->addSpacing(10);
    btnRow->addWidget(btnMute_);
    btnRow->addWidget(volSlider_);
    btnRow->addWidget(volLabel_);
    btnRow->addStretch();

    mainLayout->addLayout(btnRow);
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
