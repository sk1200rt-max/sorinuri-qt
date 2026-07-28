#include "ControlBar.h"
#include <QTime>
#include <QPainter>
#include <QStyleOption>

// ── 공통 스타일 ────────────────────────────────────────────────────
static const QString BTN_STYLE = R"(
    QPushButton {
        background: transparent;
        border: none;
        color: #bbb;
        font-size: 15px;
        padding: 0;
        min-width: 34px;
        min-height: 34px;
        border-radius: 4px;
    }
    QPushButton:hover  { background: #252525; color: #fff; }
    QPushButton:pressed{ background: #303030; }
)";

static const QString SEEK_STYLE = R"(
    QSlider::groove:horizontal {
        height: 4px;
        background: #252525;
        border-radius: 2px;
    }
    QSlider::sub-page:horizontal {
        background: #4fc3f7;
        border-radius: 2px;
    }
    QSlider::handle:horizontal {
        width: 12px; height: 12px;
        margin: -4px 0;
        background: #fff;
        border-radius: 6px;
    }
    QSlider::handle:horizontal:hover { background: #4fc3f7; }
)";

static const QString VOL_STYLE = R"(
    QSlider::groove:horizontal {
        height: 3px;
        background: #252525;
        border-radius: 2px;
    }
    QSlider::sub-page:horizontal {
        background: #4fc3f7;
        border-radius: 2px;
    }
    QSlider::handle:horizontal {
        width: 10px; height: 10px;
        margin: -4px 0;
        background: #4fc3f7;
        border-radius: 5px;
    }
)";

ControlBar::ControlBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(76);
    setStyleSheet("background: #0d0d0d; border-top: 1px solid #1c1c1c;");

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 5, 12, 5);
    mainLayout->setSpacing(4);

    // ── 진행바 ────────────────────────────────────────────────────
    QHBoxLayout* seekRow = new QHBoxLayout();
    seekRow->setSpacing(8);

    seekSlider_ = new QSlider(Qt::Horizontal, this);
    seekSlider_->setRange(0, 10000);
    seekSlider_->setStyleSheet(SEEK_STYLE);
    connect(seekSlider_, &QSlider::sliderMoved,   this, &ControlBar::onSeekSliderMoved);
    connect(seekSlider_, &QSlider::sliderPressed,  [this]() { seeking_ = true; });
    connect(seekSlider_, &QSlider::sliderReleased, [this]() {
        seeking_ = false;
        emit seeked((seekSlider_->value() / 10000.0) * totalDuration_);
    });

    timeLabel_ = new QLabel("00:00 / 00:00", this);
    timeLabel_->setStyleSheet(
        "color: #666; font-size: 11px; font-family: 'Consolas','Courier New',monospace;"
        "min-width: 120px;");
    timeLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    seekRow->addWidget(seekSlider_, 1);
    seekRow->addWidget(timeLabel_);
    mainLayout->addLayout(seekRow);

    // ── 버튼 영역 ─────────────────────────────────────────────────
    QHBoxLayout* btnRow = new QHBoxLayout();
    btnRow->setSpacing(2);

    // 파일 열기
    btnOpen_ = new QPushButton(this);
    btnOpen_->setIcon(style()->standardIcon(QStyle::SP_DirOpenIcon));
    btnOpen_->setIconSize(QSize(16, 16));
    btnOpen_->setToolTip("파일 열기 (Ctrl+O)");
    btnOpen_->setStyleSheet(BTN_STYLE);
    connect(btnOpen_, &QPushButton::clicked, this, &ControlBar::openFileClicked);

    // 이전 / 재생 / 다음 / 정지
    btnPrev_ = new QPushButton("⏮", this);
    btnPlay_ = new QPushButton("▶", this);
    btnNext_ = new QPushButton("⏭", this);
    btnStop_ = new QPushButton("■", this);

    btnPrev_->setToolTip("이전 (←)");
    btnPlay_->setToolTip("재생/일시정지 (Space)");
    btnNext_->setToolTip("다음 (→)");
    btnStop_->setToolTip("정지");

    for (auto* b : {btnPrev_, btnPlay_, btnNext_, btnStop_})
        b->setStyleSheet(BTN_STYLE);

    btnPlay_->setStyleSheet(BTN_STYLE +
        "QPushButton { font-size: 18px; min-width: 40px; min-height: 40px; }");

    connect(btnPrev_, &QPushButton::clicked, this, &ControlBar::prevClicked);
    connect(btnPlay_, &QPushButton::clicked, this, &ControlBar::playPauseClicked);
    connect(btnNext_, &QPushButton::clicked, this, &ControlBar::nextClicked);
    connect(btnStop_, &QPushButton::clicked, this, &ControlBar::stopClicked);

    // 볼륨
    btnMute_ = new QPushButton("🔊", this);
    btnMute_->setToolTip("음소거 (M)");
    btnMute_->setStyleSheet(BTN_STYLE);
    connect(btnMute_, &QPushButton::clicked, [this]() {
        muted_ = !muted_;
        btnMute_->setText(muted_ ? "🔇" : "🔊");
        emit muteToggled(muted_);
    });

    volSlider_ = new QSlider(Qt::Horizontal, this);
    volSlider_->setRange(0, 200);
    volSlider_->setValue(100);
    volSlider_->setFixedWidth(80);
    volSlider_->setStyleSheet(VOL_STYLE);
    connect(volSlider_, &QSlider::valueChanged, this, &ControlBar::onVolumeSliderMoved);

    volLabel_ = new QLabel("100%", this);
    volLabel_->setStyleSheet(
        "color: #666; font-size: 11px; font-family: 'Consolas','Courier New',monospace;"
        "min-width: 36px;");
    volLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    // 배속
    speedCombo_ = new QComboBox(this);
    speedCombo_->addItems({"0.25x","0.5x","0.75x","1.0x","1.25x","1.5x","2.0x","3.0x","4.0x"});
    speedCombo_->setCurrentIndex(3);
    speedCombo_->setFixedWidth(64);
    speedCombo_->setStyleSheet(R"(
        QComboBox {
            background: #1a1a1a; color: #888;
            border: 1px solid #2a2a2a; border-radius: 3px;
            padding: 2px 6px; font-size: 11px;
        }
        QComboBox::drop-down { border: none; width: 14px; }
        QComboBox QAbstractItemView {
            background: #1a1a1a; color: #ccc;
            selection-background-color: #1a3a5c;
            font-size: 11px;
        }
    )");
    connect(speedCombo_, &QComboBox::currentTextChanged, [this](const QString& t) {
        emit speedChanged(t.chopped(1).toDouble());
    });

    // 레이아웃 조립
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
    btnRow->addWidget(speedCombo_);

    mainLayout->addLayout(btnRow);
}

void ControlBar::setPlaying(bool playing) {
    btnPlay_->setText(playing ? "⏸" : "▶");
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

void ControlBar::updateTracks(MpvCore*) {}

void ControlBar::onSeekSliderMoved(int value) {
    if (totalDuration_ > 0)
        timeLabel_->setText(formatTime((value / 10000.0) * totalDuration_)
                            + " / " + formatTime(totalDuration_));
}

void ControlBar::onVolumeSliderMoved(int value) {
    volLabel_->setText(QString("%1%").arg(value));
    emit volumeChanged(value);
}

QString ControlBar::formatTime(double seconds) const {
    if (seconds < 0) seconds = 0;
    int total = static_cast<int>(seconds);
    int h = total / 3600, m = (total % 3600) / 60, s = total % 60;
    if (h > 0)
        return QString("%1:%2:%3").arg(h).arg(m,2,10,QChar('0')).arg(s,2,10,QChar('0'));
    return QString("%1:%2").arg(m,2,10,QChar('0')).arg(s,2,10,QChar('0'));
}
