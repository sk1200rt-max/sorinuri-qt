// UpscaleWidget.cpp — 목업 v4_upscale_ui.png 픽셀 정밀 일치 구현
// 레이아웃: 영상 아래 하단 컨트롤 패널 (전체 너비)
// 구조: 모드 버튼 행 / 강도 슬라이더 행 / 비교 체크박스 행 / 성능 그래프 3개

#include "UpscaleWidget.h"
#include "MpvCore.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QCheckBox>
#include <QProcess>
#include <QButtonGroup>
#include <QPainter>
#include <QTimer>
#include <QRandomGenerator>
#include <QFont>
#include <QFrame>

// ─── 성능 그래프 위젯 ─────────────────────────────────────────────────────
class PerfGraph : public QWidget {
    Q_OBJECT
public:
    explicit PerfGraph(const QString& label, const QString& unit, QWidget* parent=nullptr)
        : QWidget(parent), label_(label), unit_(unit)
    {
        setMinimumHeight(70);
        // 더미 데이터 초기화
        for (int i=0;i<60;++i) data_.append(QRandomGenerator::global()->bounded(20,60));
        auto* t = new QTimer(this);
        connect(t, &QTimer::timeout, this, [this](){
            data_.removeFirst();
            data_.append(QRandomGenerator::global()->bounded(20,70));
            currentVal_ = data_.last();
            update();
        });
        t->start(500);
    }
    void setValue(double v, const QString& display) { currentVal_=v; displayStr_=display; update(); }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        // 배경
        p.fillRect(rect(), QColor(0x1a,0x1a,0x1a));
        // 테두리
        p.setPen(QPen(QColor(0x00,0xc8,0xb4), 1));
        p.drawRect(rect().adjusted(0,0,-1,-1));

        // 레이블 + 값
        p.setFont(QFont("Consolas", 11, QFont::Bold));
        p.setPen(QColor(0x00,0xc8,0xb4));
        QString txt = label_ + "  " + (displayStr_.isEmpty() ? QString::number((int)currentVal_)+unit_ : displayStr_);
        p.drawText(QRect(8,6,width()-16,20), Qt::AlignLeft|Qt::AlignVCenter, txt);

        // 파형 그래프
        if (data_.isEmpty()) return;
        int gx=8, gy=30, gw=width()-16, gh=height()-36;
        p.setPen(QPen(QColor(0x00,0xc8,0xb4), 1.5));
        double step = (double)gw / (data_.size()-1);
        QPolygonF poly;
        for (int i=0;i<data_.size();++i) {
            double x = gx + i*step;
            double y = gy + gh - (data_[i]/100.0)*gh;
            poly << QPointF(x,y);
        }
        p.drawPolyline(poly);
    }

private:
    QString label_, unit_, displayStr_;
    QVector<double> data_;
    double currentVal_ = 45.0;
};

// ─── 스타일시트 ───────────────────────────────────────────────────────────
static const char* US = R"(
QWidget { background-color: #141414; color: #cccccc; font-family: 'Malgun Gothic'; font-size: 12px; }
QLabel#lbl { color: #888888; font-size: 12px; }
QLabel#valBig { color: #00c8b4; font-size: 22px; font-weight: bold; }
QLabel#statusLbl { color: #00c8b4; font-size: 11px; }
/* 모드 버튼 */
QPushButton#modeBtn {
    background-color: #1e1e1e;
    border: 1px solid #333333;
    border-radius: 4px;
    color: #888888;
    font-size: 12px;
    padding: 6px 16px;
    min-height: 32px;
}
QPushButton#modeBtn:checked {
    background-color: #00c8b4;
    border-color: #00c8b4;
    color: #000000;
    font-weight: bold;
}
QPushButton#modeBtn:hover:!checked { border-color: #00c8b4; color: #00c8b4; }
/* 슬라이더 */
QSlider::groove:horizontal {
    background: #2a2a2a; height: 4px; border-radius: 2px;
}
QSlider::handle:horizontal {
    background: #00c8b4; width: 16px; height: 16px;
    margin: -6px 0; border-radius: 8px;
}
QSlider::sub-page:horizontal { background: #00c8b4; border-radius: 2px; }
QSlider::tick-mark { background: #444; }
/* 비교 체크박스 버튼 */
QPushButton#btnCmp {
    background-color: #1e1e1e;
    border: 1px solid #555555;
    border-radius: 4px;
    color: #aaaaaa;
    font-size: 12px;
    padding: 5px 14px;
    min-height: 30px;
}
QPushButton#btnCmp:checked {
    background-color: #1a2a1a;
    border-color: #00c8b4;
    color: #00c8b4;
}
QFrame#sep { background-color: #2a2a2a; max-height: 1px; }
)";

UpscaleWidget::UpscaleWidget(MpvCore* core, QWidget* parent)
    : QWidget(parent), core_(core)
{
    setStyleSheet(US);
    buildUI();
}

void UpscaleWidget::buildUI() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 12, 16, 12);
    root->setSpacing(0);

    // ── 1. 업스케일 모드 행 ───────────────────────────────
    auto* rowMode = new QHBoxLayout;
    rowMode->setContentsMargins(0,0,0,0);
    rowMode->setSpacing(8);

    auto* lblMode = new QLabel("업스케일 모드:");
    lblMode->setObjectName("lbl");
    lblMode->setFixedWidth(100);
    rowMode->addWidget(lblMode);

    btnOff_     = new QPushButton("OFF");
    btnNvidia_  = new QPushButton("NVIDIA RTX VSR");
    btnAmd_     = new QPushButton("AMD FSR");
    btnLanczos_ = new QPushButton("Lanczos");

    for (auto* btn : {btnOff_, btnNvidia_, btnAmd_, btnLanczos_}) {
        btn->setObjectName("modeBtn");
        btn->setCheckable(true);
        rowMode->addWidget(btn);
    }
    btnNvidia_->setChecked(true); // 목업: NVIDIA 선택 상태
    rowMode->addStretch();
    root->addLayout(rowMode);
    root->addSpacing(10);

    // 버튼 그룹
    auto* grp = new QButtonGroup(this);
    grp->addButton(btnOff_,    (int)UpscaleMode::Off);
    grp->addButton(btnNvidia_, (int)UpscaleMode::NvidiaRTX);
    grp->addButton(btnAmd_,    (int)UpscaleMode::AmdFSR);
    grp->addButton(btnLanczos_,(int)UpscaleMode::Lanczos);
    connect(grp, QOverload<int>::of(&QButtonGroup::idClicked), this, [this](int id){
        setMode((UpscaleMode)id);
    });

    // ── 2. 화질 강도 슬라이더 행 ──────────────────────────
    auto* rowStr = new QHBoxLayout;
    rowStr->setContentsMargins(0,0,0,0);
    rowStr->setSpacing(10);

    auto* lblStr = new QLabel("화질 강도:");
    lblStr->setObjectName("lbl");
    lblStr->setFixedWidth(100);

    // 왼쪽 레이블
    auto* lblL = new QLabel("1\n부드러움");
    lblL->setAlignment(Qt::AlignCenter);
    lblL->setStyleSheet("color:#555; font-size:10px;");
    lblL->setFixedWidth(44);

    sldStrength_ = new QSlider(Qt::Horizontal);
    sldStrength_->setRange(1, 4);
    sldStrength_->setValue(3);
    sldStrength_->setTickPosition(QSlider::TicksBelow);
    sldStrength_->setTickInterval(1);

    // 오른쪽 레이블
    auto* lblR = new QLabel("4\n선명함");
    lblR->setAlignment(Qt::AlignCenter);
    lblR->setStyleSheet("color:#555; font-size:10px;");
    lblR->setFixedWidth(44);

    lblStrVal_ = new QLabel("3");
    lblStrVal_->setObjectName("valBig");
    lblStrVal_->setFixedWidth(30);
    lblStrVal_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    rowStr->addWidget(lblStr);
    rowStr->addWidget(lblL);
    rowStr->addWidget(sldStrength_);
    rowStr->addWidget(lblR);
    rowStr->addSpacing(12);
    rowStr->addWidget(lblStrVal_);
    root->addLayout(rowStr);
    root->addSpacing(10);

    connect(sldStrength_, &QSlider::valueChanged, this, [this](int v){
        lblStrVal_->setText(QString::number(v));
        setStrength(v);
    });

    // ── 3. 비교 체크박스 + 상태 행 ───────────────────────
    auto* rowCmp = new QHBoxLayout;
    rowCmp->setContentsMargins(0,0,0,0);

    btnCompare_ = new QPushButton("☰  원본/업스케일 비교 보기");
    btnCompare_->setObjectName("btnCmp");
    btnCompare_->setCheckable(true);
    btnCompare_->setChecked(true); // 목업: 체크 상태

    lblStatus_ = new QLabel("업스케일 활성화됨");
    lblStatus_->setObjectName("statusLbl");
    lblStatus_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    rowCmp->addWidget(btnCompare_);
    rowCmp->addStretch();
    rowCmp->addWidget(lblStatus_);
    root->addLayout(rowCmp);
    root->addSpacing(10);

    connect(btnCompare_, &QPushButton::toggled, this, &UpscaleWidget::toggleCompare);

    // ── 4. 성능 그래프 3개 ────────────────────────────────
    auto* rowGraphs = new QHBoxLayout;
    rowGraphs->setContentsMargins(0,0,0,0);
    rowGraphs->setSpacing(8);

    auto* gpuGraph  = new PerfGraph("GPU",     "%");
    auto* vramGraph = new PerfGraph("VRAM",    "GB");
    auto* fpsGraph  = new PerfGraph("FPS  60 / Latency", "ms");

    gpuGraph->setValue(45,  "GPU   45%");
    vramGraph->setValue(2.1,"VRAM  2.1GB");
    fpsGraph->setValue(60,  "FPS  60 / Latency 16ms");

    rowGraphs->addWidget(gpuGraph);
    rowGraphs->addWidget(vramGraph);
    rowGraphs->addWidget(fpsGraph);
    root->addLayout(rowGraphs);

    root->addStretch();

    // 초기 상태 적용
    mode_ = UpscaleMode::NvidiaRTX;
    applyToMpv();
}

QString UpscaleWidget::detectGpu() const {
    QProcess p;
    p.start("nvidia-smi", QStringList() << "--query-gpu=name" << "--format=csv,noheader");
    p.waitForFinished(2000);
    QString out = p.readAllStandardOutput().trimmed();
    if (!out.isEmpty()) return out.split('\n').first().trimmed();
    return QString();
}

void UpscaleWidget::setMode(UpscaleMode m) {
    mode_ = m;
    applyToMpv();
    static const QStringList names={"OFF","NVIDIA RTX VSR","AMD FSR","Lanczos"};
    int idx=(int)m;
    if (m==UpscaleMode::Off) {
        lblStatus_->setText("업스케일링 비활성");
        lblStatus_->setStyleSheet("color:#555; font-size:11px;");
    } else {
        lblStatus_->setText(idx<names.size() ? names[idx]+" 활성화됨" : "활성화됨");
        lblStatus_->setStyleSheet("color:#00c8b4; font-size:11px;");
    }
    emit modeChanged(m);
}

void UpscaleWidget::setStrength(int level) {
    strength_ = qBound(1,level,4);
    if (mode_!=UpscaleMode::Off) applyToMpv();
}

void UpscaleWidget::toggleCompare(bool on) {
    compareMode_ = on;
    emit compareToggled(on);
}

void UpscaleWidget::applyToMpv() {
    if (!core_) return;
    switch (mode_) {
    case UpscaleMode::Off:
        core_->setProperty("vo",    QString("gpu"));
        core_->setProperty("scale", QString("bilinear"));
        core_->setProperty("hwdec", QString("auto-safe"));
        break;
    case UpscaleMode::NvidiaRTX:
        core_->setProperty("vo",           QString("gpu-next"));
        core_->setProperty("hwdec",        QString("d3d11va"));
        core_->setProperty("gpu-api",      QString("d3d11"));
        core_->setProperty("scale",        QString("ewa_lanczossharp"));
        core_->setProperty("scale-param1", QVariant(strength_*0.5));
        break;
    case UpscaleMode::AmdFSR:
        core_->setProperty("vo",           QString("gpu-next"));
        core_->setProperty("hwdec",        QString("auto"));
        core_->setProperty("scale",        QString("ewa_lanczos"));
        core_->setProperty("scale-param1", QVariant(strength_*0.4));
        break;
    case UpscaleMode::Lanczos:
        core_->setProperty("vo",           QString("gpu"));
        core_->setProperty("hwdec",        QString("auto-safe"));
        core_->setProperty("scale",        QString("lanczos"));
        core_->setProperty("scale-param1", QVariant(strength_*0.5));
        break;
    }
}

#include "UpscaleWidget.moc"
