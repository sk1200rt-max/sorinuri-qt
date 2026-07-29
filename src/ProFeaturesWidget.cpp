#include "ProFeaturesWidget.h"
#include <QDir>
#include <QDateTime>
#include <QStandardPaths>
#include <QMessageBox>
#include <QGroupBox>
#include <QFrame>

static QLabel* makeSectionTitle(const QString& text, QWidget* parent) {
    auto* lbl = new QLabel(text, parent);
    lbl->setStyleSheet(
        "color: #888; font-size: 9px; font-weight: 600; font-family: 'Consolas', monospace;"
        "background: transparent; letter-spacing: 1px; padding: 0 4px 2px 0;");
    return lbl;
}

static QPushButton* makeSmallBtn(const QString& text, const QString& tip, QWidget* parent = nullptr) {
    auto* btn = new QPushButton(text, parent);
    btn->setToolTip(tip);
    btn->setFixedHeight(22);
    btn->setStyleSheet(
        "QPushButton { background: #1e1e1e; color: #ccc; border: 1px solid #2a2a2a;"
        "border-radius: 3px; padding: 0 8px; font-size: 11px; }"
        "QPushButton:hover { background: #2a2a2a; border-color: #4fc3f7; }"
        "QPushButton:pressed { background: #1565c0; }"
        "QPushButton:checked { background: #1565c0; border-color: #4fc3f7; color: #fff; }");
    return btn;
}

static QSlider* makeHSlider(int min, int max, int val, QWidget* parent = nullptr) {
    auto* s = new QSlider(Qt::Horizontal, parent);
    s->setRange(min, max);
    s->setValue(val);
    s->setFixedHeight(16);
    s->setStyleSheet(
        "QSlider::groove:horizontal { height: 3px; background: #2a2a2a; border-radius: 1px; }"
        "QSlider::handle:horizontal { width: 10px; height: 10px; margin: -4px 0;"
        "background: #4fc3f7; border-radius: 5px; }"
        "QSlider::sub-page:horizontal { background: #1565c0; border-radius: 1px; }");
    return s;
}

ProFeaturesWidget::ProFeaturesWidget(QWidget* parent) : QWidget(parent) {
    setFixedHeight(88);
    setStyleSheet("background: #060606; border-top: 1px solid #141414;");

    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(12, 6, 12, 6);
    mainLayout->setSpacing(16);

    // ── 섹션 구분선 ────────────────────────────────────────────
    auto makeSep = [this]() {
        auto* sep = new QFrame(this);
        sep->setFrameShape(QFrame::VLine);
        sep->setStyleSheet("color: #1e1e1e;");
        return sep;
    };

    // ── A-B 구간 반복 ─────────────────────────────────────────
    {
        auto* sec = new QWidget(this);
        auto* lay = new QVBoxLayout(sec);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(4);
        lay->addWidget(makeSectionTitle("A-B 반복", sec));

        auto* row = new QHBoxLayout();
        row->setSpacing(4);
        btnSetA_ = makeSmallBtn("[A]", "A 지점 설정 (A 키)", sec);
        btnSetA_->setCheckable(true);
        btnSetB_ = makeSmallBtn("[B]", "B 지점 설정 (B 키)", sec);
        btnSetB_->setCheckable(true);
        btnClearAB_ = makeSmallBtn("✕", "A-B 반복 해제", sec);
        row->addWidget(btnSetA_);
        row->addWidget(btnSetB_);
        row->addWidget(btnClearAB_);
        lay->addLayout(row);

        abLabel_ = new QLabel("--:-- ~ --:--", sec);
        abLabel_->setStyleSheet(
            "color: #444; font-size: 9px; font-family: 'Consolas', monospace;"
            "background: transparent;");
        lay->addWidget(abLabel_);

        connect(btnSetA_, &QPushButton::clicked, this, &ProFeaturesWidget::setAbPointA);
        connect(btnSetB_, &QPushButton::clicked, this, &ProFeaturesWidget::setAbPointB);
        connect(btnClearAB_, &QPushButton::clicked, this, &ProFeaturesWidget::clearAbLoop);

        mainLayout->addWidget(sec);
    }
    mainLayout->addWidget(makeSep());

    // ── 재생속도 ──────────────────────────────────────────────
    {
        auto* sec = new QWidget(this);
        auto* lay = new QVBoxLayout(sec);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(4);
        lay->addWidget(makeSectionTitle("재생속도", sec));

        speedLabel_ = new QLabel("1.00x", sec);
        speedLabel_->setStyleSheet(
            "color: #4fc3f7; font-size: 13px; font-weight: 700; font-family: 'Consolas', monospace;"
            "background: transparent;");
        speedLabel_->setAlignment(Qt::AlignCenter);

        auto* row = new QHBoxLayout();
        row->setSpacing(2);
        for (double sp : {0.25, 0.5, 1.0, 1.5, 2.0, 4.0}) {
            QString label = QString::number(sp, 'g', 3) + "x";
            auto* btn = makeSmallBtn(label, QString("재생속도 %1배").arg(sp), sec);
            if (sp == 1.0) btn->setStyleSheet(btn->styleSheet() +
                "QPushButton { border-color: #1565c0; }");
            connect(btn, &QPushButton::clicked, [this, sp]() { onSpeedPreset(sp); });
            row->addWidget(btn);
        }
        lay->addWidget(speedLabel_);
        lay->addLayout(row);
        mainLayout->addWidget(sec);
    }
    mainLayout->addWidget(makeSep());

    // ── 오디오 딜레이 ─────────────────────────────────────────
    {
        auto* sec = new QWidget(this);
        auto* lay = new QVBoxLayout(sec);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(4);
        lay->addWidget(makeSectionTitle("오디오 딜레이", sec));

        audioDelaySlider_ = makeHSlider(-500, 500, 0, sec);
        audioDelayLabel_  = new QLabel("0 ms", sec);
        audioDelayLabel_->setStyleSheet(
            "color: #888; font-size: 10px; font-family: 'Consolas', monospace;"
            "background: transparent; min-width: 48px;");
        audioDelayLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        auto* row = new QHBoxLayout();
        row->addWidget(audioDelaySlider_, 1);
        row->addWidget(audioDelayLabel_);
        lay->addLayout(row);

        auto* resetBtn = makeSmallBtn("리셋", "오디오 딜레이 초기화", sec);
        connect(resetBtn, &QPushButton::clicked, [this]() {
            audioDelaySlider_->setValue(0);
        });
        lay->addWidget(resetBtn);

        connect(audioDelaySlider_, &QSlider::valueChanged, this, &ProFeaturesWidget::onAudioDelaySlider);
        mainLayout->addWidget(sec);
    }
    mainLayout->addWidget(makeSep());

    // ── 자막 딜레이 ───────────────────────────────────────────
    {
        auto* sec = new QWidget(this);
        auto* lay = new QVBoxLayout(sec);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(4);
        lay->addWidget(makeSectionTitle("자막 딜레이", sec));

        subDelaySlider_ = makeHSlider(-2000, 2000, 0, sec);
        subDelayLabel_  = new QLabel("0 ms", sec);
        subDelayLabel_->setStyleSheet(
            "color: #888; font-size: 10px; font-family: 'Consolas', monospace;"
            "background: transparent; min-width: 48px;");
        subDelayLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        auto* row = new QHBoxLayout();
        row->addWidget(subDelaySlider_, 1);
        row->addWidget(subDelayLabel_);
        lay->addLayout(row);

        auto* resetBtn = makeSmallBtn("리셋", "자막 딜레이 초기화", sec);
        connect(resetBtn, &QPushButton::clicked, [this]() {
            subDelaySlider_->setValue(0);
        });
        lay->addWidget(resetBtn);

        connect(subDelaySlider_, &QSlider::valueChanged, this, &ProFeaturesWidget::onSubDelaySlider);
        mainLayout->addWidget(sec);
    }
    mainLayout->addWidget(makeSep());

    // ── 화면 필터 ─────────────────────────────────────────────
    {
        auto* sec = new QWidget(this);
        auto* lay = new QVBoxLayout(sec);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(2);
        lay->addWidget(makeSectionTitle("화면 필터", sec));

        auto makeFilterRow = [&](const QString& name, QSlider** slider, int min, int max) {
            auto* row = new QHBoxLayout();
            auto* lbl = new QLabel(name, sec);
            lbl->setStyleSheet("color: #555; font-size: 9px; background: transparent; min-width: 24px;");
            *slider = makeHSlider(min, max, 0, sec);
            row->addWidget(lbl);
            row->addWidget(*slider, 1);
            lay->addLayout(row);
        };

        makeFilterRow("밝기", &brightnessSlider_, -100, 100);
        makeFilterRow("대비", &contrastSlider_,   -100, 100);
        makeFilterRow("채도", &saturationSlider_, -100, 100);
        makeFilterRow("감마", &gammaSlider_,      -100, 100);

        auto* resetBtn = makeSmallBtn("리셋", "화면 필터 초기화", sec);
        connect(resetBtn, &QPushButton::clicked, this, &ProFeaturesWidget::resetFilters);
        lay->addWidget(resetBtn);

        connect(brightnessSlider_, &QSlider::valueChanged, this, &ProFeaturesWidget::brightnessChanged);
        connect(contrastSlider_,   &QSlider::valueChanged, this, &ProFeaturesWidget::contrastChanged);
        connect(saturationSlider_, &QSlider::valueChanged, this, &ProFeaturesWidget::saturationChanged);
        connect(gammaSlider_,      &QSlider::valueChanged, this, &ProFeaturesWidget::gammaChanged);

        mainLayout->addWidget(sec);
    }
    mainLayout->addWidget(makeSep());

    // ── 스크린샷 ──────────────────────────────────────────────
    {
        auto* sec = new QWidget(this);
        auto* lay = new QVBoxLayout(sec);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(4);
        lay->addWidget(makeSectionTitle("스크린샷", sec));

        auto* btn = makeSmallBtn("📷  현재 프레임 저장", "현재 프레임을 PNG로 저장 (S 키)", sec);
        btn->setFixedWidth(130);
        connect(btn, &QPushButton::clicked, this, &ProFeaturesWidget::takeScreenshot);
        lay->addWidget(btn);
        lay->addStretch();
        mainLayout->addWidget(sec);
    }

    mainLayout->addStretch();
}

void ProFeaturesWidget::connectMpv(MpvCore* core) {
    mpv_ = core;

    connect(this, &ProFeaturesWidget::speedChanged, core, &MpvCore::setSpeed);

    connect(this, &ProFeaturesWidget::audioDelayChanged, [core](double ms) {
        core->setProperty("audio-delay", ms / 1000.0);
    });
    connect(this, &ProFeaturesWidget::subDelayChanged, [core](double ms) {
        core->setProperty("sub-delay", ms / 1000.0);
    });
    connect(this, &ProFeaturesWidget::brightnessChanged, [core](int val) {
        core->setProperty("brightness", val);
    });
    connect(this, &ProFeaturesWidget::contrastChanged, [core](int val) {
        core->setProperty("contrast", val);
    });
    connect(this, &ProFeaturesWidget::saturationChanged, [core](int val) {
        core->setProperty("saturation", val);
    });
    connect(this, &ProFeaturesWidget::gammaChanged, [core](int val) {
        core->setProperty("gamma", val);
    });
}

void ProFeaturesWidget::setAbPointA() {
    if (!mpv_) return;
    abPointA_ = mpv_->position();
    btnSetA_->setChecked(true);
    mpv_->setProperty("ab-loop-a", abPointA_);

    // B가 설정되어 있으면 루프 활성화
    if (abPointB_ > abPointA_) {
        mpv_->setProperty("ab-loop-b", abPointB_);
        updateAbLabel();
    } else {
        int sec = static_cast<int>(abPointA_);
        abLabel_->setText(QString("A: %1:%2")
            .arg(sec/60, 2, 10, QChar('0'))
            .arg(sec%60, 2, 10, QChar('0')));
    }
}

void ProFeaturesWidget::setAbPointB() {
    if (!mpv_) return;
    abPointB_ = mpv_->position();
    btnSetB_->setChecked(true);
    mpv_->setProperty("ab-loop-b", abPointB_);

    if (abPointA_ >= 0 && abPointB_ > abPointA_) {
        mpv_->setProperty("ab-loop-a", abPointA_);
        updateAbLabel();
    }
}

void ProFeaturesWidget::updateAbLabel() {
    auto fmt = [](double s) {
        int t = static_cast<int>(s);
        return QString("%1:%2").arg(t/60, 2, 10, QChar('0')).arg(t%60, 2, 10, QChar('0'));
    };
    abLabel_->setText(fmt(abPointA_) + " ~ " + fmt(abPointB_));
    abLabel_->setStyleSheet(
        "color: #4caf50; font-size: 9px; font-family: 'Consolas', monospace; background: transparent;");
}

void ProFeaturesWidget::clearAbLoop() {
    abPointA_ = -1;
    abPointB_ = -1;
    btnSetA_->setChecked(false);
    btnSetB_->setChecked(false);
    abLabel_->setText("--:-- ~ --:--");
    abLabel_->setStyleSheet(
        "color: #444; font-size: 9px; font-family: 'Consolas', monospace; background: transparent;");
    if (mpv_) {
        mpv_->setProperty("ab-loop-a", QString("no"));
        mpv_->setProperty("ab-loop-b", QString("no"));
    }
}

void ProFeaturesWidget::setSpeed(double speed) {
    onSpeedPreset(speed);
}

void ProFeaturesWidget::onSpeedPreset(double speed) {
    currentSpeed_ = speed;
    speedLabel_->setText(QString("%1x").arg(speed, 0, 'f', 2));
    emit speedChanged(speed);
}

void ProFeaturesWidget::onAudioDelaySlider(int val) {
    audioDelayLabel_->setText(QString("%1 ms").arg(val));
    emit audioDelayChanged(val);
}

void ProFeaturesWidget::onSubDelaySlider(int val) {
    subDelayLabel_->setText(QString("%1 ms").arg(val));
    emit subDelayChanged(val);
}

void ProFeaturesWidget::resetFilters() {
    brightnessSlider_->setValue(0);
    contrastSlider_->setValue(0);
    saturationSlider_->setValue(0);
    gammaSlider_->setValue(0);
}

void ProFeaturesWidget::takeScreenshot() {
    if (!mpv_) return;
    // MPV screenshot 명령 실행 (현재 프레임을 PNG로 저장)
    mpv_->command({"screenshot", "video"});
}

void ProFeaturesWidget::setAudioDelay(double ms) {
    audioDelaySlider_->setValue(static_cast<int>(ms));
}

void ProFeaturesWidget::setSubDelay(double ms) {
    subDelaySlider_->setValue(static_cast<int>(ms));
}
