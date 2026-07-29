// WhisperWidget.cpp — 목업 v4_whisper_ui.png 픽셀 정밀 일치 구현
// 레이아웃: 우측 패널 전용 (메인 창에서 우측 30% 영역에 배치됨)
// 배경: #141414, 좌측 경계선: 1px #2a2a2a
// 색상: teal=#00c8b4, text=#cccccc, subtext=#888, bg=#0e0e0e, panel=#141414

#include "WhisperWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QProgressBar>
#include <QListWidget>
#include <QProcess>
#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>
#include <QFont>
#include <QFrame>

// ─── 스타일시트 (목업 색상 정밀 일치) ─────────────────────────────────────
static const char* WS = R"(
/* 패널 전체 */
WhisperWidget {
    background-color: #141414;
    border-left: 1px solid #2a2a2a;
}
/* 모든 텍스트 기본 */
QWidget { color: #cccccc; font-family: 'Malgun Gothic'; font-size: 12px; }

/* 섹션 헤더 */
QLabel#hdr {
    color: #00c8b4;
    font-size: 14px;
    font-weight: bold;
}
/* GPU 상태 */
QLabel#gpu {
    color: #00c8b4;
    font-size: 11px;
}
/* GPU 이름 (우측 정렬) */
QLabel#gpuName {
    color: #00c8b4;
    font-size: 11px;
}
/* 구분선 */
QFrame#sep {
    color: #2a2a2a;
    background-color: #2a2a2a;
    max-height: 1px;
}
/* 레이블 */
QLabel#lbl {
    color: #888888;
    font-size: 12px;
}
/* 드롭다운 */
QComboBox {
    background-color: #1e1e1e;
    border: 1px solid #333333;
    border-radius: 4px;
    color: #cccccc;
    padding: 4px 8px;
    font-size: 12px;
    min-height: 28px;
}
QComboBox::drop-down {
    border: none;
    width: 22px;
    subcontrol-origin: padding;
    subcontrol-position: right center;
}
QComboBox::down-arrow {
    image: none;
    border-left: 4px solid transparent;
    border-right: 4px solid transparent;
    border-top: 5px solid #888;
    width: 0; height: 0;
    margin-right: 6px;
}
QComboBox QAbstractItemView {
    background-color: #1e1e1e;
    border: 1px solid #333;
    color: #cccccc;
    selection-background-color: #00c8b4;
    selection-color: #000;
}
/* 체크박스 */
QCheckBox {
    color: #cccccc;
    font-size: 12px;
    spacing: 7px;
}
QCheckBox::indicator {
    width: 16px; height: 16px;
    border: 1px solid #555;
    border-radius: 3px;
    background: #1e1e1e;
}
QCheckBox::indicator:checked {
    background-color: #00c8b4;
    border-color: #00c8b4;
    image: none;
}
/* 정확도 바 */
QProgressBar {
    background-color: #1e1e1e;
    border: none;
    border-radius: 3px;
    min-height: 8px;
    max-height: 8px;
    text-align: right;
}
QProgressBar::chunk {
    background-color: #00c8b4;
    border-radius: 3px;
}
/* 정확도 % 텍스트 */
QLabel#confPct {
    color: #00c8b4;
    font-size: 12px;
    font-weight: bold;
    min-width: 36px;
}
/* 최근 자막 섹션 헤더 */
QLabel#recentHdr {
    color: #00c8b4;
    font-size: 12px;
    font-weight: bold;
}
/* 최근 자막 리스트 */
QListWidget {
    background-color: #1a1a1a;
    border: 1px solid #2a2a2a;
    border-radius: 4px;
    color: #888888;
    font-size: 11px;
    font-family: 'Malgun Gothic';
    outline: none;
}
QListWidget::item {
    padding: 5px 8px;
    border-bottom: 1px solid #222222;
    min-height: 22px;
}
QListWidget::item:first-child {
    color: #00c8b4;
}
QListWidget::item:selected {
    background-color: #1e1e1e;
    color: #00c8b4;
}
/* 상태 텍스트 */
QLabel#status {
    color: #00c8b4;
    font-size: 11px;
}
/* 토글 버튼 — 활성(중지) */
QPushButton#btnStop {
    background-color: #cc3333;
    color: #ffffff;
    font-size: 14px;
    font-weight: bold;
    border: none;
    border-radius: 6px;
    min-height: 44px;
}
QPushButton#btnStop:hover { background-color: #dd4444; }
/* 토글 버튼 — 비활성(시작) */
QPushButton#btnStart {
    background-color: #00c8b4;
    color: #000000;
    font-size: 14px;
    font-weight: bold;
    border: none;
    border-radius: 6px;
    min-height: 44px;
}
QPushButton#btnStart:hover { background-color: #00ddc8; }
QPushButton#btnStart:disabled {
    background-color: #1e1e1e;
    color: #555;
    border: 1px solid #333;
}
)";

WhisperWidget::WhisperWidget(QWidget* parent) : QWidget(parent) {
    setStyleSheet(WS);
    buildUI();
    checkWhisperAvailable();
}

WhisperWidget::~WhisperWidget() { stopProcess(); }

void WhisperWidget::buildUI() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 14, 16, 14);
    root->setSpacing(0);

    // ── 1. 헤더: 🤖 AI 자막 설정 ──────────────────────────
    auto* hdr = new QLabel("🤖  AI 자막 설정");
    hdr->setObjectName("hdr");
    root->addWidget(hdr);
    root->addSpacing(10);

    // ── 2. GPU 상태 행 ────────────────────────────────────
    auto* rowGpu = new QHBoxLayout;
    rowGpu->setContentsMargins(0,0,0,0);
    auto* lblGpuDot = new QLabel("● GPU 가속 활성 (RTX 3080)");
    lblGpuDot->setObjectName("gpu");
    lblGpuName_ = new QLabel("RTX 3080");
    lblGpuName_->setObjectName("gpuName");
    lblGpuName_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    rowGpu->addWidget(lblGpuDot);
    rowGpu->addStretch();
    rowGpu->addWidget(lblGpuName_);
    root->addLayout(rowGpu);
    root->addSpacing(10);

    // ── 구분선 ────────────────────────────────────────────
    auto* sep1 = new QFrame; sep1->setObjectName("sep");
    sep1->setFrameShape(QFrame::HLine);
    root->addWidget(sep1);
    root->addSpacing(12);

    // ── 3. 인식 언어 ──────────────────────────────────────
    auto* rowLang = new QHBoxLayout;
    rowLang->setContentsMargins(0,0,0,0);
    auto* lblLang = new QLabel("인식 언어:");
    lblLang->setObjectName("lbl");
    lblLang->setFixedWidth(80);
    cmbLang_ = new QComboBox;
    cmbLang_->addItems({"자동 감지", "영어", "일본어", "중국어", "스페인어", "프랑스어", "독일어"});
    cmbLang_->setCurrentIndex(1); // 영어
    rowLang->addWidget(lblLang);
    rowLang->addWidget(cmbLang_);
    root->addLayout(rowLang);
    root->addSpacing(8);

    // ── 4. Whisper 모델 ───────────────────────────────────
    auto* rowModel = new QHBoxLayout;
    rowModel->setContentsMargins(0,0,0,0);
    auto* lblModel = new QLabel("Whisper 모델:");
    lblModel->setObjectName("lbl");
    lblModel->setFixedWidth(80);
    cmbModel_ = new QComboBox;
    cmbModel_->addItems({"base (빠름, 저사양)", "medium (권장)", "large-v3 (최고 품질)"});
    cmbModel_->setCurrentIndex(1);
    rowModel->addWidget(lblModel);
    rowModel->addWidget(cmbModel_);
    root->addLayout(rowModel);
    root->addSpacing(12);

    // ── 5. 체크박스 행 ────────────────────────────────────
    auto* rowChk = new QHBoxLayout;
    rowChk->setContentsMargins(0,0,0,0);
    rowChk->setSpacing(20);
    chkTranslate_ = new QCheckBox("한국어로 번역");
    chkTranslate_->setChecked(true);
    chkSave_ = new QCheckBox("SRT 파일로 저장");
    rowChk->addWidget(chkTranslate_);
    rowChk->addWidget(chkSave_);
    rowChk->addStretch();
    root->addLayout(rowChk);
    root->addSpacing(14);

    // ── 6. 인식 정확도 ────────────────────────────────────
    auto* rowConf = new QHBoxLayout;
    rowConf->setContentsMargins(0,0,0,0);
    auto* lblConf = new QLabel("인식 정확도:");
    lblConf->setObjectName("lbl");
    lblConf->setFixedWidth(80);
    barConf_ = new QProgressBar;
    barConf_->setRange(0, 100);
    barConf_->setValue(94);
    barConf_->setTextVisible(false);
    lblConfPct_ = new QLabel("94%");
    lblConfPct_->setObjectName("confPct");
    lblConfPct_->setFixedWidth(36);
    lblConfPct_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    rowConf->addWidget(lblConf);
    rowConf->addWidget(barConf_);
    rowConf->addSpacing(6);
    rowConf->addWidget(lblConfPct_);
    root->addLayout(rowConf);
    root->addSpacing(14);

    // ── 7. 최근 자막 헤더 ─────────────────────────────────
    auto* lblRecent = new QLabel("최근 자막 (최근 5개)");
    lblRecent->setObjectName("recentHdr");
    root->addWidget(lblRecent);
    root->addSpacing(6);

    // ── 8. 최근 자막 리스트 ───────────────────────────────
    lstRecent_ = new QListWidget;
    lstRecent_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    lstRecent_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    lstRecent_->setFocusPolicy(Qt::NoFocus);
    // 초기 더미 데이터
    QStringList dummy = {
        "00:02:45  이것은 매우 지능적인 시스템입니다.",
        "00:02:41  시간 지연이 발생할 겁니다.",
        "00:02:36  중력 왜곡이 심해지고 있어요.",
        "00:02:31  우리는 블랙홀 근처에 있습니다.",
        "00:02:26  데이터를 계속 수집하세요."
    };
    for (int i = 0; i < dummy.size(); ++i) {
        auto* item = new QListWidgetItem(dummy[i]);
        if (i == 0) item->setForeground(QColor("#00c8b4"));
        else        item->setForeground(QColor("#888888"));
        lstRecent_->addItem(item);
    }
    root->addWidget(lstRecent_);
    root->addSpacing(10);

    // ── 9. 상태 텍스트 ────────────────────────────────────
    lblStatus_ = new QLabel("AI 자막 처리 중...");
    lblStatus_->setObjectName("status");
    root->addWidget(lblStatus_);
    root->addSpacing(10);

    // ── 10. 토글 버튼 ─────────────────────────────────────
    btnToggle_ = new QPushButton("AI 자막 중지");
    btnToggle_->setObjectName("btnStop");
    root->addWidget(btnToggle_);

    root->addStretch();

    // 시그널 연결
    connect(cmbLang_,     QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &WhisperWidget::onLangChanged);
    connect(cmbModel_,    QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &WhisperWidget::onModelChanged);
    connect(chkTranslate_,&QCheckBox::toggled, this, &WhisperWidget::onTranslateToggled);
    connect(chkSave_,     &QCheckBox::toggled, this, &WhisperWidget::onSaveToggled);
    connect(btnToggle_,   &QPushButton::clicked, this, [this](){
        setActive(!active_);
    });
}

void WhisperWidget::checkWhisperAvailable() {
    QString gpu = detectGpu();
    lblGpuName_->setText(gpu.isEmpty() ? "CPU 모드" : gpu);

    QProcess check;
    check.start("python", QStringList() << "-c" << "import faster_whisper; print('OK')");
    check.waitForFinished(3000);
    QString out = check.readAllStandardOutput().trimmed();

    whisperReady_ = (out == "OK");
    if (!whisperReady_) {
        btnToggle_->setEnabled(false);
        btnToggle_->setObjectName("btnStart");
        btnToggle_->setText("AI 자막 시작");
        lblStatus_->setText("faster-whisper 미설치 — 포터블 패키지에 포함됨");
        btnToggle_->style()->unpolish(btnToggle_);
        btnToggle_->style()->polish(btnToggle_);
    }
}

QString WhisperWidget::detectGpu() const {
    QProcess p;
    p.start("nvidia-smi", QStringList() << "--query-gpu=name" << "--format=csv,noheader");
    p.waitForFinished(2000);
    QString out = p.readAllStandardOutput().trimmed();
    if (!out.isEmpty()) return out.split('\n').first().trimmed();
    return QString();
}

void WhisperWidget::setMediaFile(const QString& path) { mediaPath_ = path; }

void WhisperWidget::setActive(bool on) {
    if (on == active_) return;
    active_ = on;

    if (on) {
        btnToggle_->setObjectName("btnStop");
        btnToggle_->setText("AI 자막 중지");
        lblStatus_->setText("AI 자막 처리 중...");
        startProcess();
    } else {
        btnToggle_->setObjectName("btnStart");
        btnToggle_->setText("AI 자막 시작");
        lblStatus_->setText("AI 자막 대기 중");
        barConf_->setValue(0);
        lblConfPct_->setText("0%");
        stopProcess();
    }
    btnToggle_->style()->unpolish(btnToggle_);
    btnToggle_->style()->polish(btnToggle_);
    emit activeChanged(on);
}

void WhisperWidget::startProcess() {
    if (!whisperReady_ || mediaPath_.isEmpty()) return;
    stopProcess();
    proc_ = new QProcess(this);
    connect(proc_, &QProcess::readyReadStandardOutput, this, &WhisperWidget::onProcessOutput);
    connect(proc_, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &WhisperWidget::onProcessFinished);

    QStringList args;
    args << QCoreApplication::applicationDirPath() + "/whisper_server.py"
         << "--file" << mediaPath_
         << "--model" << cfg_.model
         << "--language" << cfg_.language;
    if (cfg_.translate)  args << "--translate";
    if (cfg_.saveToFile) args << "--save-srt";
    proc_->start("python", args);
}

void WhisperWidget::stopProcess() {
    if (proc_) { proc_->kill(); proc_->waitForFinished(1000); proc_->deleteLater(); proc_ = nullptr; }
}

void WhisperWidget::onProcessOutput() {
    if (!proc_) return;
    while (proc_->canReadLine()) {
        QString line = proc_->readLine().trimmed();
        QStringList p = line.split('|');
        if (p.size() < 3) continue;
        double start = p[0].toDouble(), end = p[1].toDouble();
        QString text = p[2];
        int conf = (p.size() >= 4) ? (int)(p[3].toDouble()*100) : 80;

        barConf_->setValue(conf);
        lblConfPct_->setText(QString("%1%").arg(conf));
        emit subtitleGenerated(text, start, end);

        QString ts = QString("%1:%2:%3")
            .arg((int)start/3600,2,10,QChar('0'))
            .arg(((int)start%3600)/60,2,10,QChar('0'))
            .arg((int)start%60,2,10,QChar('0'));
        recentLines_.prepend(ts + "  " + text);
        if (recentLines_.size() > 5) recentLines_.removeLast();

        lstRecent_->clear();
        for (int i = 0; i < recentLines_.size(); ++i) {
            auto* item = new QListWidgetItem(recentLines_[i]);
            item->setForeground(i == 0 ? QColor("#00c8b4") : QColor("#888888"));
            lstRecent_->addItem(item);
        }
    }
}

void WhisperWidget::onProcessError()           {}
void WhisperWidget::onProcessFinished(int)     { if (active_) setActive(false); }
void WhisperWidget::onLangChanged(int idx) {
    static const QStringList c={"auto","en","ja","zh","es","fr","de"};
    if (idx < c.size()) cfg_.language = c[idx];
}
void WhisperWidget::onModelChanged(int idx) {
    static const QStringList m={"base","medium","large-v3"};
    if (idx < m.size()) cfg_.model = m[idx];
}
void WhisperWidget::onTranslateToggled(bool on) { cfg_.translate = on; }
void WhisperWidget::onSaveToggled(bool on)      { cfg_.saveToFile = on; }
void WhisperWidget::onPositionChanged(double)   {}
void WhisperWidget::updateStatusLabel()         {}
