#include "ControlBar.h"
#include <QTime>

static const QString BTN_STYLE = R"(
    QPushButton {
        background: transparent;
        border: none;
        color: #ccc;
        font-size: 16px;
        padding: 4px 8px;
        min-width: 32px;
        min-height: 32px;
        border-radius: 4px;
    }
    QPushButton:hover { background: #2a2a2a; color: #fff; }
    QPushButton:pressed { background: #333; }
)";

static const QString SLIDER_STYLE = R"(
    QSlider::groove:horizontal {
        height: 4px;
        background: #2a2a2a;
        border-radius: 2px;
    }
    QSlider::sub-page:horizontal {
        background: #4fc3f7;
        border-radius: 2px;
    }
    QSlider::handle:horizontal {
        width: 12px;
        height: 12px;
        margin: -4px 0;
        background: #fff;
        border-radius: 6px;
    }
    QSlider::handle:horizontal:hover {
        background: #4fc3f7;
    }
)";

ControlBar::ControlBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(80);
    setStyleSheet("background: #0d0d0d; border-top: 1px solid #1e1e1e;");

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 6, 12, 6);
    mainLayout->setSpacing(4);

    // ── 진행바 ────────────────────────────────────────────────────
    QHBoxLayout* seekLayout = new QHBoxLayout();
    seekLayout->setSpacing(8);

    seekSlider_ = new QSlider(Qt::Horizontal, this);
    seekSlider_->setRange(0, 1000);
    seekSlider_->setStyleSheet(SLIDER_STYLE);
    connect(seekSlider_, &QSlider::sliderMoved, this, &ControlBar::onSeekSliderMoved);
    connect(seekSlider_, &QSlider::sliderPressed, [this]() { seeking_ = true; });
    connect(seekSlider_, &QSlider::sliderReleased, [this]() {
        seeking_ = false;
        double pos = (seekSlider_->value() / 1000.0) * totalDuration_;
        emit seeked(pos);
    });

    timeLabel_ = new QLabel("0:00:00 / 0:00:00", this);
    timeLabel_->setStyleSheet("color: #888; font-size: 11px; min-width: 130px;");
    timeLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    seekLayout->addWidget(seekSlider_, 1);
    seekLayout->addWidget(timeLabel_);
    mainLayout->addLayout(seekLayout);

    // ── 버튼 영역 ─────────────────────────────────────────────────
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(2);

    // 파일 열기
    btnOpen_ = new QPushButton("📂", this);
    btnOpen_->setStyleSheet(BTN_STYLE);
    btnOpen_->setToolTip("파일 열기 (Ctrl+O)");
    connect(btnOpen_, &QPushButton::clicked, this, &ControlBar::openFileClicked);

    // 이전/재생/다음/정지
    btnPrev_ = new QPushButton("⏮", this);
    btnPlay_ = new QPushButton("▶", this);
    btnNext_ = new QPushButton("⏭", this);
    btnStop_ = new QPushButton("⏹", this);

    for (auto* btn : {btnPrev_, btnPlay_, btnNext_, btnStop_})
        btn->setStyleSheet(BTN_STYLE);

    btnPlay_->setStyleSheet(BTN_STYLE + "QPushButton { font-size: 20px; }");

    connect(btnPrev_, &QPushButton::clicked, this, &ControlBar::prevClicked);
    connect(btnPlay_, &QPushButton::clicked, this, &ControlBar::playPauseClicked);
    connect(btnNext_, &QPushButton::clicked, this, &ControlBar::nextClicked);
    connect(btnStop_, &QPushButton::clicked, this, &ControlBar::stopClicked);

    // 볼륨
    btnMute_ = new QPushButton("🔊", this);
    btnMute_->setStyleSheet(BTN_STYLE);
    connect(btnMute_, &QPushButton::clicked, [this]() {
        muted_ = !muted_;
        btnMute_->setText(muted_ ? "🔇" : "🔊");
        emit muteToggled(muted_);
    });

    volSlider_ = new QSlider(Qt::Horizontal, this);
    volSlider_->setRange(0, 200);
    volSlider_->setValue(100);
    volSlider_->setFixedWidth(90);
    volSlider_->setStyleSheet(SLIDER_STYLE);
    connect(volSlider_, &QSlider::valueChanged, this, &ControlBar::onVolumeSliderMoved);

    volLabel_ = new QLabel("100%", this);
    volLabel_->setStyleSheet("color: #888; font-size: 11px; min-width: 36px;");
    volLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    // 배속
    speedCombo_ = new QComboBox(this);
    speedCombo_->addItems({"0.5x", "0.75x", "1.0x", "1.25x", "1.5x", "2.0x"});
    speedCombo_->setCurrentIndex(2);
    speedCombo_->setStyleSheet(R"(
        QComboBox {
            background: #1a1a1a;
            color: #ccc;
            border: 1px solid #333;
            border-radius: 3px;
            padding: 2px 6px;
            font-size: 11px;
            min-width: 60px;
        }
        QComboBox::drop-down { border: none; }
        QComboBox QAbstractItemView {
            background: #1a1a1a;
            color: #ccc;
            selection-background-color: #1a3a5c;
        }
    )");
    connect(speedCombo_, &QComboBox::currentTextChanged, [this](const QString& text) {
        double speed = text.chopped(1).toDouble();
        emit speedChanged(speed);
    });

    // 전체화면
    btnFullscreen_ = new QPushButton("⛶", this);
    btnFullscreen_->setStyleSheet(BTN_STYLE);
    btnFullscreen_->setToolTip("전체화면 (F)");
    connect(btnFullscreen_, &QPushButton::clicked, this, &ControlBar::fullscreenToggled);

    // 레이아웃 조립
    btnLayout->addWidget(btnOpen_);
    btnLayout->addSpacing(8);
    btnLayout->addWidget(btnPrev_);
    btnLayout->addWidget(btnPlay_);
    btnLayout->addWidget(btnNext_);
    btnLayout->addWidget(btnStop_);
    btnLayout->addSpacing(8);
    btnLayout->addWidget(btnMute_);
    btnLayout->addWidget(volSlider_);
    btnLayout->addWidget(volLabel_);
    btnLayout->addStretch();
    btnLayout->addWidget(speedCombo_);
    btnLayout->addSpacing(8);
    btnLayout->addWidget(btnFullscreen_);

    mainLayout->addLayout(btnLayout);
}

void ControlBar::setPlaying(bool playing) {
    btnPlay_->setText(playing ? "⏸" : "▶");
}

void ControlBar::setPosition(double pos, double dur) {
    if (!seeking_ && dur > 0) {
        seekSlider_->setValue(static_cast<int>((pos / dur) * 1000));
    }
    timeLabel_->setText(formatTime(pos) + " / " + formatTime(dur));
}

void ControlBar::setDuration(double dur) {
    totalDuration_ = dur;
}

void ControlBar::setVolume(int vol) {
    volSlider_->blockSignals(true);
    volSlider_->setValue(vol);
    volSlider_->blockSignals(false);
    volLabel_->setText(QString("%1%").arg(vol));
}

void ControlBar::updateTracks(MpvCore* /*core*/) {
    // TODO: 오디오/자막 트랙 콤보박스 업데이트
}

void ControlBar::onSeekSliderMoved(int value) {
    if (totalDuration_ > 0) {
        double pos = (value / 1000.0) * totalDuration_;
        timeLabel_->setText(formatTime(pos) + " / " + formatTime(totalDuration_));
    }
}

void ControlBar::onVolumeSliderMoved(int value) {
    volLabel_->setText(QString("%1%").arg(value));
    emit volumeChanged(value);
}

QString ControlBar::formatTime(double seconds) const {
    if (seconds < 0) seconds = 0;
    int h = static_cast<int>(seconds) / 3600;
    int m = (static_cast<int>(seconds) % 3600) / 60;
    int s = static_cast<int>(seconds) % 60;
    return QString("%1:%2:%3")
        .arg(h)
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'));
}
