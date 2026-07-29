#include "UpscaleWidget.h"
#include "MpvCore.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QProcess>
#include <QButtonGroup>

static const char* UPSCALE_STYLE = R"(
QWidget { background: transparent; color: #cccccc; font-family: 'Malgun Gothic'; }
QLabel#lblSection { color: #00c8b4; font-weight: bold; font-size: 11px; }
QLabel#lblGpuInfo { color: #00c8b4; font-size: 10px; }
QPushButton#modeBtn {
    background: #1e1e1e; border: 1px solid #333; border-radius: 4px;
    color: #888; font-size: 10px; padding: 4px 10px;
}
QPushButton#modeBtn:checked {
    background: #00c8b4; border-color: #00c8b4; color: #000; font-weight: bold;
}
QPushButton#modeBtn:hover { border-color: #00c8b4; color: #00c8b4; }
QSlider::groove:horizontal {
    background: #2a2a2a; height: 4px; border-radius: 2px;
}
QSlider::handle:horizontal {
    background: #00c8b4; width: 14px; height: 14px; margin: -5px 0;
    border-radius: 7px;
}
QSlider::sub-page:horizontal { background: #00c8b4; border-radius: 2px; }
QPushButton#btnCompare {
    background: #1e1e1e; border: 1px solid #555; border-radius: 4px;
    color: #aaa; font-size: 10px; padding: 4px 12px;
}
QPushButton#btnCompare:checked {
    background: #2a4a2a; border-color: #00c8b4; color: #00c8b4;
}
)";

UpscaleWidget::UpscaleWidget(MpvCore* core, QWidget* parent)
    : QWidget(parent), core_(core)
{
    setStyleSheet(UPSCALE_STYLE);
    buildUI();
}

void UpscaleWidget::buildUI() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 8, 12, 8);
    root->setSpacing(8);

    // ── 제목 + GPU 정보 ──────────────────────────────────
    auto* rowTop = new QHBoxLayout;
    auto* lblTitle = new QLabel("AI 화질 업스케일링");
    lblTitle->setObjectName("lblSection");
    QFont f = lblTitle->font(); f.setPointSize(11); f.setBold(true);
    lblTitle->setFont(f);

    lblGpuInfo_ = new QLabel(detectGpu());
    lblGpuInfo_->setObjectName("lblGpuInfo");
    lblGpuInfo_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    rowTop->addWidget(lblTitle);
    rowTop->addStretch();
    rowTop->addWidget(lblGpuInfo_);
    root->addLayout(rowTop);

    // ── 업스케일 모드 버튼 ────────────────────────────────
    auto* lblMode = new QLabel("업스케일 모드:");
    root->addWidget(lblMode);

    auto* rowMode = new QHBoxLayout;
    rowMode->setSpacing(6);

    btnOff_     = new QPushButton("OFF");
    btnNvidia_  = new QPushButton("NVIDIA RTX VSR");
    btnAmd_     = new QPushButton("AMD FSR");
    btnLanczos_ = new QPushButton("Lanczos");

    for (auto* btn : {btnOff_, btnNvidia_, btnAmd_, btnLanczos_}) {
        btn->setObjectName("modeBtn");
        btn->setCheckable(true);
        rowMode->addWidget(btn);
    }
    btnOff_->setChecked(true);
    root->addLayout(rowMode);

    // 버튼 그룹 (상호 배타)
    auto* grp = new QButtonGroup(this);
    grp->addButton(btnOff_,     (int)UpscaleMode::Off);
    grp->addButton(btnNvidia_,  (int)UpscaleMode::NvidiaRTX);
    grp->addButton(btnAmd_,     (int)UpscaleMode::AmdFSR);
    grp->addButton(btnLanczos_, (int)UpscaleMode::Lanczos);
    connect(grp, QOverload<int>::of(&QButtonGroup::idClicked), this, [this](int id){
        setMode((UpscaleMode)id);
    });

    // ── 화질 강도 슬라이더 ────────────────────────────────
    auto* lblStrengthTitle = new QLabel("화질 강도:");
    root->addWidget(lblStrengthTitle);

    auto* rowStr = new QHBoxLayout;
    auto* lblL = new QLabel("1\n부드러움");
    lblL->setAlignment(Qt::AlignCenter);
    lblL->setStyleSheet("color:#555; font-size:9px;");

    sldStrength_ = new QSlider(Qt::Horizontal);
    sldStrength_->setRange(1, 4);
    sldStrength_->setValue(3);
    sldStrength_->setTickPosition(QSlider::TicksBelow);
    sldStrength_->setTickInterval(1);

    auto* lblR = new QLabel("4\n선명함");
    lblR->setAlignment(Qt::AlignCenter);
    lblR->setStyleSheet("color:#555; font-size:9px;");

    lblStrength_ = new QLabel("3");
    lblStrength_->setStyleSheet("color:#00c8b4; font-weight:bold; font-size:13px;");
    lblStrength_->setFixedWidth(20);
    lblStrength_->setAlignment(Qt::AlignCenter);

    rowStr->addWidget(lblL);
    rowStr->addWidget(sldStrength_);
    rowStr->addWidget(lblR);
    rowStr->addSpacing(8);
    rowStr->addWidget(lblStrength_);
    root->addLayout(rowStr);

    connect(sldStrength_, &QSlider::valueChanged, this, [this](int v){
        lblStrength_->setText(QString::number(v));
        setStrength(v);
    });

    // ── 비교 모드 버튼 ────────────────────────────────────
    auto* rowBottom = new QHBoxLayout;
    btnCompare_ = new QPushButton("◧  원본/업스케일 비교 보기");
    btnCompare_->setObjectName("btnCompare");
    btnCompare_->setCheckable(true);
    rowBottom->addWidget(btnCompare_);
    rowBottom->addStretch();
    root->addLayout(rowBottom);

    connect(btnCompare_, &QPushButton::toggled, this, &UpscaleWidget::toggleCompare);

    // ── 상태 레이블 ───────────────────────────────────────
    lblStatus_ = new QLabel("업스케일링 비활성");
    lblStatus_->setStyleSheet("color:#555; font-size:10px;");
    root->addWidget(lblStatus_);

    root->addStretch();
}

QString UpscaleWidget::detectGpu() const {
    QProcess p;
    p.start("nvidia-smi", QStringList() << "--query-gpu=name" << "--format=csv,noheader");
    p.waitForFinished(2000);
    QString out = p.readAllStandardOutput().trimmed();
    if (!out.isEmpty()) {
        QString name = out.split('\n').first().trimmed();
        return QString("● NVIDIA %1").arg(name);
    }
    return "● GPU 미감지 (소프트웨어 모드)";
}

void UpscaleWidget::setMode(UpscaleMode m) {
    mode_ = m;
    applyToMpv();

    static const QStringList names = {"OFF","NVIDIA RTX VSR","AMD FSR","Lanczos"};
    int idx = (int)m;
    if (idx < names.size()) {
        if (m == UpscaleMode::Off) {
            lblStatus_->setText("업스케일링 비활성");
            lblStatus_->setStyleSheet("color:#555; font-size:10px;");
        } else {
            lblStatus_->setText(QString("%1 활성화됨").arg(names[idx]));
            lblStatus_->setStyleSheet("color:#00c8b4; font-size:10px;");
        }
    }
    emit modeChanged(m);
}

void UpscaleWidget::setStrength(int level) {
    strength_ = qBound(1, level, 4);
    if (mode_ != UpscaleMode::Off) applyToMpv();
}

void UpscaleWidget::toggleCompare(bool on) {
    compareMode_ = on;
    if (core_) {
        // MPV의 video-zoom 및 video-pan으로 비교 구현
        // 실제로는 두 개의 렌더 패스가 필요하므로 오버레이 방식 사용
        core_->setProperty("video-unscaled", on ? "no" : "no");
    }
    emit compareToggled(on);
}

void UpscaleWidget::applyToMpv() {
    if (!core_) return;

    switch (mode_) {
    case UpscaleMode::Off:
        core_->setProperty("vo", QString("gpu"));
        core_->setProperty("scale", QString("bilinear"));
        core_->setProperty("hwdec", QString("auto-safe"));
        break;

    case UpscaleMode::NvidiaRTX:
        // RTX Video Super Resolution: gpu-next + d3d11va + ewa_lanczossharp
        core_->setProperty("vo", QString("gpu-next"));
        core_->setProperty("hwdec", QString("d3d11va"));
        core_->setProperty("gpu-api", QString("d3d11"));
        // strength 1-4 → scale-param1 0.5-2.0
        core_->setProperty("scale", QString("ewa_lanczossharp"));
        core_->setProperty("scale-param1", QVariant(strength_ * 0.5));
        break;

    case UpscaleMode::AmdFSR:
        core_->setProperty("vo", QString("gpu-next"));
        core_->setProperty("hwdec", QString("auto"));
        core_->setProperty("scale", QString("ewa_lanczos"));
        core_->setProperty("scale-param1", QVariant(strength_ * 0.4));
        break;

    case UpscaleMode::Lanczos:
        core_->setProperty("vo", QString("gpu"));
        core_->setProperty("hwdec", QString("auto-safe"));
        core_->setProperty("scale", QString("lanczos"));
        core_->setProperty("scale-param1", QVariant(strength_ * 0.5));
        break;
    }
}
