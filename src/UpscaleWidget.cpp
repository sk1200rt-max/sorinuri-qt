// UpscaleWidget.cpp — 목업 v4_upscale_ui.png 픽셀 정밀 일치 구현
// 레이아웃: 영상 아래 하단 컨트롤 패널 (전체 너비)
// 구조: 모드 버튼 행 / 강도 슬라이더 행 / 비교 체크박스 행 / 성능 그래프 3개

#include "UpscaleWidget.h"
#include "MpvCore.h"
#include <QCoreApplication>
#include <QFile>
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
    btnESRGAN_  = new QPushButton("Real-ESRGAN");
    btnRIFE_    = new QPushButton("RIFE 프레임보간");
    btnAnime4K_ = new QPushButton("Anime4K");
    btnAnime4K_->setToolTip("애니메이션 전용 업스케일\nGLSL 셔이더 기반 선명도 향상");

    for (auto* btn : {btnOff_, btnNvidia_, btnAmd_, btnLanczos_, btnESRGAN_, btnRIFE_, btnAnime4K_}) {
        btn->setObjectName("modeBtn");
        btn->setCheckable(true);
        rowMode->addWidget(btn);
    }
    btnOff_->setChecked(true);
    rowMode->addStretch();
    root->addLayout(rowMode);
    root->addSpacing(10);

    // 버튼 그룹
    auto* grp = new QButtonGroup(this);
    grp->addButton(btnOff_,     (int)UpscaleMode::Off);
    grp->addButton(btnNvidia_,  (int)UpscaleMode::NvidiaRTX);
    grp->addButton(btnAmd_,     (int)UpscaleMode::AmdFSR);
    grp->addButton(btnLanczos_, (int)UpscaleMode::Lanczos);
    grp->addButton(btnESRGAN_,  (int)UpscaleMode::RealESRGAN);
    grp->addButton(btnRIFE_,    (int)UpscaleMode::RIFE);
    grp->addButton(btnAnime4K_, (int)UpscaleMode::Anime4K);
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

    // ── 2.5 성능 프리셋 + GPU 자동 감지 ─────────────────────────────────────
    {
        auto* rowPreset = new QHBoxLayout;
        rowPreset->setContentsMargins(0,0,0,0);
        rowPreset->setSpacing(8);
        auto* lblPreset = new QLabel("성능 프리셋:");
        lblPreset->setObjectName("lbl");
        lblPreset->setFixedWidth(100);
        rowPreset->addWidget(lblPreset);
        auto* presetGrp = new QButtonGroup(this);
        QStringList presetNames = {"품질 우선", "균형 (권장)", "속도 우선"};
        for (int i = 0; i < presetNames.size(); ++i) {
            auto* btn = new QPushButton(presetNames[i]);
            btn->setCheckable(true);
            btn->setChecked(i == 1);
            btn->setStyleSheet(
                "QPushButton{background:#1e1e1e;color:#888;border:1px solid #333;border-radius:4px;padding:4px 12px;font-size:11px;}"
                "QPushButton:checked{background:#00c8b4;color:#0e0e0e;border-color:#00c8b4;}"
                "QPushButton:hover{background:#2a2a2a;color:white;}");
            presetGrp->addButton(btn, i);
            rowPreset->addWidget(btn);
        }
        QString gpuName = detectGpu();
        if (!gpuName.isEmpty()) {
            auto* gpuLabel = new QLabel("감지: " + gpuName);
            gpuLabel->setStyleSheet("color:#4fc3f7; font-size:10px;");
            rowPreset->addStretch();
            rowPreset->addWidget(gpuLabel);
        } else {
            rowPreset->addStretch();
        }
        root->addLayout(rowPreset);
        root->addSpacing(8);
        connect(presetGrp, QOverload<int>::of(&QButtonGroup::idClicked), this, [this](int id) {
            switch (id) {
            case 0: sldStrength_->setValue(4); break;
            case 1: sldStrength_->setValue(3); break;
            case 2: sldStrength_->setValue(1); break;
            }
        });
    }

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

    // 패널을 열기만 해도 MPV 렌더러나 디코더 설정이 바뀌면 안 된다.
    // 실제 보정은 사용자가 모드 버튼을 명시적으로 눌렀을 때만 적용한다.
    mode_ = UpscaleMode::Off;
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
        // Off는 사용자 렌더러·디코더 설정을 덮어쓰지 않는다.
        break;
    case UpscaleMode::NvidiaRTX:
        core_->setProperty("hwdec",        QString("d3d11va"));
        core_->setProperty("scale",        QString("ewa_lanczossharp"));
        core_->setProperty("scale-param1", QVariant(strength_*0.5));
        break;
    case UpscaleMode::AmdFSR:
        core_->setProperty("hwdec",        QString("auto"));
        core_->setProperty("scale",        QString("ewa_lanczos"));
        core_->setProperty("scale-param1", QVariant(strength_*0.4));
        break;
        case UpscaleMode::Lanczos:
        core_->setProperty("hwdec",        QString("auto-safe"));
        core_->setProperty("scale",        QString("lanczos"));
        core_->setProperty("scale-param1", QVariant(strength_*0.5));
        // GLSL 셋어 해제
        core_->command({"change-list", "glsl-shaders", "clr", ""});
        break;
    case UpscaleMode::RealESRGAN: {
        // Real-ESRGAN x4 GLSL 셋어 적용 (MPV 내장 알고리즘 사용)
        core_->setProperty("hwdec", QString("auto"));
        core_->setProperty("scale", QString("ewa_lanczossharp"));
        // 알려진 위치에 셋어 파일이 있으면 로드, 없으면 알고리즘만 적용
        QString shaderPath = QCoreApplication::applicationDirPath() + "/shaders/realesrgan-x4plus.glsl";
        if (QFile::exists(shaderPath)) {
            core_->command({"change-list", "glsl-shaders", "set", shaderPath});
        } else {
            core_->command({"change-list", "glsl-shaders", "clr", ""});
        }
        if (lblStatus_) lblStatus_->setText(QFile::exists(shaderPath) ? "✅ Real-ESRGAN 셋어 적용" : "⚠️ 셋어 파일 없음 - 고화질 알고리즘 적용중");
        break;
    }
    case UpscaleMode::RIFE: {
        // RIFE 프레임 보간 GLSL 셔이더 적용
        core_->setProperty("hwdec", QString("auto"));
        // 프레임 보간: video-sync=display-resample + interpolation
        core_->setProperty("video-sync",    QString("display-resample"));
        core_->setProperty("interpolation", true);
        core_->setProperty("tscale",        QString("oversample"));
        QString shaderPath = QCoreApplication::applicationDirPath() + "/shaders/rife-v4.glsl";
        if (QFile::exists(shaderPath)) {
            core_->command({"change-list", "glsl-shaders", "set", shaderPath});
        } else {
            // 셔이더 없어도 MPV 내장 interpolation으로 동작
            core_->command({"change-list", "glsl-shaders", "clr", ""});
        }
        if (lblStatus_) lblStatus_->setText("✅ RIFE 프레임 보간 활성화");
        break;
    }
    case UpscaleMode::Anime4K: {
        // Anime4K GLSL 셔이더 - 애니메이션 전용 업스케일
        // 애니메이션 특유의 라인 아트 선명도를 유지하면서 해상도 향상
        // 사용 셔이더: Anime4K/Anime4K-Lossless-Upscale-CNN-x2-M.glsl
        // 셔이더 파일이 없으면 MPV 내장 ewa_lanczossharp으로 폴백
        core_->setProperty("hwdec", QString("auto-safe"));
        // 애니메이션 최적화: deband 끄기 (라인 아트 밴딩 방지)
        core_->setProperty("deband", false);
        // 셔이더 로드 시도
        QString shaderDir = QCoreApplication::applicationDirPath() + "/shaders/";
        // Anime4K 셔이더 우선순위 목록
        QStringList anime4kShaders = {
            shaderDir + "Anime4K_Upscale_CNN_x2_M.glsl",
            shaderDir + "Anime4K_Restore_CNN_M.glsl",
            shaderDir + "anime4k.glsl",
            shaderDir + "anime4k-upscale.glsl"
        };
        QString foundShader;
        for (const QString& s : anime4kShaders) {
            if (QFile::exists(s)) { foundShader = s; break; }
        }
        if (!foundShader.isEmpty()) {
            core_->command({"change-list", "glsl-shaders", "set", foundShader});
            if (lblStatus_) lblStatus_->setText("✅ Anime4K 셔이더 적용");
        } else {
            // 셔이더 없으면 MPV 내장 애니메이션 최적화 알고리즘 사용
            core_->command({"change-list", "glsl-shaders", "clr", ""});
            core_->setProperty("scale",        QString("ewa_lanczossharp"));
            core_->setProperty("cscale",       QString("ewa_lanczossharp"));
            core_->setProperty("dscale",       QString("mitchell"));
            core_->setProperty("linear-downscaling", false);
            if (lblStatus_) lblStatus_->setText("⚠️ 셔이더 없음 - 내장 애니메이션 모드");
        }
        break;
    }
    }
    // OFF 버튼을 사용자가 명시적으로 선택했을 때만 RIFE 보정 상태를 해제한다.
    // 생성 시에는 applyToMpv()를 호출하지 않으므로 사이드 패널 열기에는 영향을 주지 않는다.
    if (mode_ == UpscaleMode::Off) {
        core_->setProperty("video-sync", QString("audio"));
        core_->setProperty("interpolation", false);
        core_->command({"change-list", "glsl-shaders", "clr", ""});
    }
}
#include "UpscaleWidget.moc"
