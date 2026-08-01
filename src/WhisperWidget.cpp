// WhisperWidget.cpp — v4.2 탭 분리형 + 오버레이형 혼합 구현
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
#include <QListWidgetItem>
#include <QLineEdit>
#include <QStackedWidget>
#include <QProcess>
#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QClipboard>
#include <QApplication>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QTimer>
#include <QRandomGenerator>
#include <QFont>
#include <QFrame>
#include <cmath>

// ═══════════════════════════════════════════════════════════════════════════
// ConfidenceGauge — 원형 신뢰도 게이지
// ═══════════════════════════════════════════════════════════════════════════
ConfidenceGauge::ConfidenceGauge(QWidget* parent) : QWidget(parent) {
    setFixedSize(88, 88);
}

void ConfidenceGauge::setValue(int pct) {
    value_ = qBound(0, pct, 100);
    update();
}

void ConfidenceGauge::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int cx = width()/2, cy = height()/2, r = 36;

    // 배경 링
    p.setPen(QPen(QColor(0x2a,0x2a,0x2a), 8, Qt::SolidLine, Qt::RoundCap));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QPoint(cx,cy), r, r);

    // 값 링 (색상: 90+초록, 80~89노란, 75미만주황)
    QColor ringColor;
    if      (value_ >= 90) ringColor = QColor(0x00,0xc8,0xb4);
    else if (value_ >= 80) ringColor = QColor(0xf0,0xc0,0x40);
    else                   ringColor = QColor(0xe0,0x70,0x20);

    p.setPen(QPen(ringColor, 8, Qt::SolidLine, Qt::RoundCap));
    int span = -(int)(value_ / 100.0 * 360 * 16);
    p.drawArc(QRect(cx-r, cy-r, r*2, r*2), 90*16, span);

    // 중앙 텍스트
    p.setPen(ringColor);
    p.setFont(QFont("Malgun Gothic", 16, QFont::Bold));
    p.drawText(QRect(cx-30, cy-18, 60, 24), Qt::AlignCenter,
               QString::number(value_) + "%");

    // 하단 레이블
    QString label;
    if      (value_ >= 90) label = "높음";
    else if (value_ >= 80) label = "보통";
    else                   label = "낮음";
    p.setFont(QFont("Malgun Gothic", 9));
    p.setPen(QColor(0x88,0x88,0x88));
    p.drawText(QRect(cx-20, cy+8, 40, 14), Qt::AlignCenter, label);
}

// ═══════════════════════════════════════════════════════════════════════════
// AudioWaveform — 오디오 파형 시각화
// ═══════════════════════════════════════════════════════════════════════════
AudioWaveform::AudioWaveform(QWidget* parent) : QWidget(parent) {
    setFixedHeight(44);
    // 더미 파형 데이터 생성
    for (int i = 0; i < 120; ++i) {
        float v = (float)QRandomGenerator::global()->bounded(20, 90) / 100.0f;
        samples_.append(v);
    }
    animTimer_ = new QTimer(this);
    connect(animTimer_, &QTimer::timeout, this, [this](){
        if (!samples_.isEmpty()) {
            samples_.removeFirst();
            samples_.append((float)QRandomGenerator::global()->bounded(20,90)/100.0f);
        }
        update();
    });
    animTimer_->start(80);
}

void AudioWaveform::setPosition(double pos, double dur) {
    pos_ = pos; duration_ = dur; update();
}

void AudioWaveform::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor(0x0e,0x0e,0x0e));

    if (samples_.isEmpty()) return;

    int W = width(), H = height();
    int midY = H / 2;
    double step = (double)W / samples_.size();

    // 현재 위치 계산
    int posX = (duration_ > 0) ? (int)((pos_ / duration_) * W) : 0;

    for (int i = 0; i < samples_.size(); ++i) {
        int x = (int)(i * step);
        int barH = (int)(samples_[i] * (H * 0.42));
        bool isPast = (x <= posX);

        QColor c = isPast ? QColor(0x00,0xc8,0xb4,200) : QColor(0x44,0x44,0x44,180);
        p.fillRect(x, midY - barH, qMax(1,(int)step-1), barH*2, c);
    }

    // 현재 위치 선
    p.setPen(QPen(QColor(0x00,0xc8,0xb4), 1.5));
    p.drawLine(posX, 0, posX, H);

    // 시간 표시
    if (duration_ > 0) {
        auto fmt = [](double s) {
            int m=(int)s/60, sec=(int)s%60;
            return QString("%1:%2").arg(m,2,10,QChar('0')).arg(sec,2,10,QChar('0'));
        };
        p.setFont(QFont("Consolas", 8));
        p.setPen(QColor(0x55,0x55,0x55));
        p.drawText(4, H-4, fmt(pos_));
        p.drawText(W-36, H-4, fmt(duration_));
    }
}

void AudioWaveform::mousePressEvent(QMouseEvent* e) {
    if (duration_ <= 0) return;
    double sec = (double)e->pos().x() / width() * duration_;
    emit seekRequested(qBound(0.0, sec, duration_));
}

// ═══════════════════════════════════════════════════════════════════════════
// 스타일시트
// ═══════════════════════════════════════════════════════════════════════════
static const char* WS2 = R"(
WhisperWidget { background: #141414; border-left: 1px solid #2a2a2a; }
QWidget { color: #cccccc; font-family: 'Malgun Gothic'; font-size: 12px; }

/* 탭 버튼 */
QPushButton#tab {
    background: transparent; border: none; border-bottom: 2px solid transparent;
    color: #666; font-size: 13px; padding: 8px 16px;
}
QPushButton#tab:checked { color: #00c8b4; border-bottom: 2px solid #00c8b4; }
QPushButton#tab:hover:!checked { color: #aaa; }

/* GPU 배지 */
QLabel#gpuBadge {
    background: #1a2a1a; border: 1px solid #00c8b4; border-radius: 10px;
    color: #00c8b4; font-size: 10px; padding: 2px 8px;
}
/* 레이블 */
QLabel#lbl { color: #888; font-size: 11px; }
/* 드롭다운 */
QComboBox {
    background: #1e1e1e; border: 1px solid #333; border-radius: 4px;
    color: #ccc; padding: 4px 8px; font-size: 12px; min-height: 28px;
}
QComboBox::drop-down { border: none; width: 20px; }
QComboBox::down-arrow {
    border-left: 4px solid transparent; border-right: 4px solid transparent;
    border-top: 5px solid #888; width: 0; height: 0; margin-right: 6px;
}
QComboBox QAbstractItemView {
    background: #1e1e1e; border: 1px solid #333; color: #ccc;
    selection-background-color: #00c8b4; selection-color: #000;
}
/* 체크박스 */
QCheckBox { color: #ccc; font-size: 12px; spacing: 6px; }
QCheckBox::indicator { width: 15px; height: 15px; border: 1px solid #555; border-radius: 3px; background: #1e1e1e; }
QCheckBox::indicator:checked { background: #00c8b4; border-color: #00c8b4; }
/* 진행률 바 */
QProgressBar { background: #1e1e1e; border: none; border-radius: 3px; min-height: 6px; max-height: 6px; }
QProgressBar::chunk { background: #00c8b4; border-radius: 3px; }
/* 자막 미리보기 */
QLabel#preview {
    background: #0e0e0e; border: 1px solid #2a2a2a; border-radius: 6px;
    color: #ffffff; font-size: 14px; font-weight: bold;
    padding: 10px 12px; min-height: 44px;
}
QLabel#previewConf { color: #00c8b4; font-size: 10px; }
/* 토글 버튼 */
QPushButton#btnStop {
    background: #cc3333; color: #fff; font-size: 14px; font-weight: bold;
    border: none; border-radius: 6px; min-height: 42px;
}
QPushButton#btnStop:hover { background: #dd4444; }
QPushButton#btnStart {
    background: #00c8b4; color: #000; font-size: 14px; font-weight: bold;
    border: none; border-radius: 6px; min-height: 42px;
}
QPushButton#btnStart:hover { background: #00ddc8; }
QPushButton#btnStart:disabled { background: #1e1e1e; color: #555; border: 1px solid #333; }
/* 검색창 */
QLineEdit {
    background: #1e1e1e; border: 1px solid #333; border-radius: 4px;
    color: #ccc; padding: 6px 10px; font-size: 12px;
}
QLineEdit:focus { border-color: #00c8b4; }
/* 필터 버튼 */
QPushButton#filterBtn {
    background: #1e1e1e; border: 1px solid #333; border-radius: 12px;
    color: #888; font-size: 11px; padding: 3px 12px; min-height: 24px;
}
QPushButton#filterBtn:checked { background: #00c8b4; border-color: #00c8b4; color: #000; font-weight: bold; }
/* 히스토리 리스트 */
QListWidget {
    background: #0e0e0e; border: none; color: #ccc; font-size: 12px; outline: none;
}
QListWidget::item { padding: 7px 10px; border-bottom: 1px solid #1a1a1a; min-height: 26px; }
QListWidget::item:selected { background: #1a2a28; }
QListWidget::item:hover:!selected { background: #161616; }
/* 하단 액션 버튼 */
QPushButton#actionBtn {
    background: transparent; border: 1px solid #00c8b4; border-radius: 4px;
    color: #00c8b4; font-size: 12px; padding: 6px 14px; min-height: 32px;
}
QPushButton#actionBtn:hover { background: #0a2a28; }
QFrame#sep { background: #2a2a2a; max-height: 1px; }
QLabel#statVal { color: #00c8b4; font-size: 11px; font-weight: bold; }
QLabel#statLbl { color: #555; font-size: 10px; }
)";

// ═══════════════════════════════════════════════════════════════════════════
// WhisperWidget
// ═══════════════════════════════════════════════════════════════════════════
WhisperWidget::WhisperWidget(QWidget* parent) : QWidget(parent) {
    setStyleSheet(WS2);
    buildUI();
    checkWhisperAvailable();
}

WhisperWidget::~WhisperWidget() { stopProcess(); }

void WhisperWidget::buildUI() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── 탭 헤더 ───────────────────────────────────────────
    auto* tabBar = new QWidget;
    tabBar->setFixedHeight(42);
    tabBar->setStyleSheet("background:#141414; border-bottom: 1px solid #2a2a2a;");
    auto* tabLayout = new QHBoxLayout(tabBar);
    tabLayout->setContentsMargins(8, 0, 8, 0);
    tabLayout->setSpacing(0);

    tabSettings_ = new QPushButton("설정");
    tabSettings_->setObjectName("tab");
    tabSettings_->setCheckable(true);
    tabSettings_->setChecked(true);

    tabHistory_ = new QPushButton("자막 히스토리");
    tabHistory_->setObjectName("tab");
    tabHistory_->setCheckable(true);

    tabLayout->addWidget(tabSettings_);
    tabLayout->addWidget(tabHistory_);
    tabLayout->addStretch();
    root->addWidget(tabBar);

    // ── 스택 위젯 ─────────────────────────────────────────
    stack_ = new QStackedWidget;
    root->addWidget(stack_);

    // 설정 탭 페이지
    auto* settingsPage = new QWidget;
    buildSettingsTab(settingsPage);
    stack_->addWidget(settingsPage);

    // 히스토리 탭 페이지
    auto* historyPage = new QWidget;
    buildHistoryTab(historyPage);
    stack_->addWidget(historyPage);

    // 탭 전환 연결
    connect(tabSettings_, &QPushButton::clicked, this, [this](){
        tabSettings_->setChecked(true);
        tabHistory_->setChecked(false);
        stack_->setCurrentIndex(0);
    });
    connect(tabHistory_, &QPushButton::clicked, this, [this](){
        tabHistory_->setChecked(true);
        tabSettings_->setChecked(false);
        stack_->setCurrentIndex(1);
    });
}

void WhisperWidget::buildSettingsTab(QWidget* parent) {
    auto* root = new QVBoxLayout(parent);
    root->setContentsMargins(14, 12, 14, 12);
    root->setSpacing(0);

    // ── GPU 배지 ──────────────────────────────────────────
    auto* rowGpu = new QHBoxLayout;
    rowGpu->setContentsMargins(0,0,0,0);
    auto* lblGpuLbl = new QLabel("GPU");
    lblGpuLbl->setObjectName("lbl");
    lblGpuName_ = new QLabel("감지 중...");
    lblGpuName_->setObjectName("gpuBadge");
    rowGpu->addStretch();
    rowGpu->addWidget(lblGpuLbl);
    rowGpu->addSpacing(6);
    rowGpu->addWidget(lblGpuName_);
    root->addLayout(rowGpu);
    root->addSpacing(10);

    // ── 언어 + 모델 (2열) ─────────────────────────────────
    auto* rowLM = new QHBoxLayout;
    rowLM->setContentsMargins(0,0,0,0);
    rowLM->setSpacing(8);

    auto* colLang = new QVBoxLayout;
    colLang->setSpacing(3);
    auto* lblLang = new QLabel("언어");
    lblLang->setObjectName("lbl");
    cmbLang_ = new QComboBox;
    cmbLang_->addItems({
        "자동 감지",   // auto
        "영어",        // en
        "한국어",      // ko
        "일본어",      // ja
        "중국어",      // zh
        "스페인어",    // es
        "프랑스어",    // fr
        "독일어",      // de
        "이탈리아어",  // it
        "포르투갈어",  // pt
        "러시아어",    // ru
        "아랍어",      // ar
        "힌디어",      // hi
        "태국어",      // th
        "베트남어",    // vi
    });
    cmbLang_->setCurrentIndex(1);  // 영어 기본
    colLang->addWidget(lblLang);
    colLang->addWidget(cmbLang_);

    auto* colModel = new QVBoxLayout;
    colModel->setSpacing(3);
    auto* lblModel = new QLabel("모델");
    lblModel->setObjectName("lbl");
    cmbModel_ = new QComboBox;
    cmbModel_->addItems({"base (빠름)", "medium (권장)", "large-v3 (최고)"});
    cmbModel_->setCurrentIndex(1);
    colModel->addWidget(lblModel);
    colModel->addWidget(cmbModel_);

    rowLM->addLayout(colLang);
    rowLM->addLayout(colModel);
    root->addLayout(rowLM);
    root->addSpacing(10);

    // ── 체크박스 2열 ──────────────────────────────────────
    auto* rowChk1 = new QHBoxLayout;
    rowChk1->setContentsMargins(0,0,0,0);
    rowChk1->setSpacing(16);
    chkTranslate_ = new QCheckBox("한국어 번역");
    chkTranslate_->setChecked(true);
    chkSpeaker_ = new QCheckBox("화자 구분");
    rowChk1->addWidget(chkTranslate_);
    rowChk1->addWidget(chkSpeaker_);
    rowChk1->addStretch();
    root->addLayout(rowChk1);
    root->addSpacing(4);

    auto* rowChk2 = new QHBoxLayout;
    rowChk2->setContentsMargins(0,0,0,0);
    rowChk2->setSpacing(16);
    chkSave_ = new QCheckBox("SRT 파일 저장");
    chkTimestamp_ = new QCheckBox("타임스탬프 포함");
    chkTimestamp_->setChecked(true);
    rowChk2->addWidget(chkSave_);
    rowChk2->addWidget(chkTimestamp_);
    rowChk2->addStretch();
    root->addLayout(rowChk2);
    root->addSpacing(12);

    // ── 신뢰도 게이지 + 처리 진행률 ──────────────────────
    auto* rowStats = new QHBoxLayout;
    rowStats->setContentsMargins(0,0,0,0);
    rowStats->setSpacing(12);

    // 원형 게이지
    auto* gaugeCol = new QVBoxLayout;
    gaugeCol->setSpacing(2);
    auto* lblGaugeLbl = new QLabel("신뢰도 (평균)");
    lblGaugeLbl->setObjectName("lbl");
    lblGaugeLbl->setAlignment(Qt::AlignCenter);
    gauge_ = new ConfidenceGauge;
    gauge_->setValue(94);
    gaugeCol->addWidget(lblGaugeLbl);
    gaugeCol->addWidget(gauge_);
    rowStats->addLayout(gaugeCol);

    // 진행률 + 시간 정보
    auto* progressCol = new QVBoxLayout;
    progressCol->setSpacing(6);

    auto* lblProgLbl = new QLabel("처리 진행률");
    lblProgLbl->setObjectName("lbl");
    barProgress_ = new QProgressBar;
    barProgress_->setRange(0, 100);
    barProgress_->setValue(68);
    barProgress_->setTextVisible(false);
    auto* lblPct = new QLabel("68%");
    lblPct->setObjectName("statVal");
    auto* rowProg = new QHBoxLayout;
    rowProg->addWidget(barProgress_);
    rowProg->addWidget(lblPct);

    auto* rowTime = new QGridLayout;
    rowTime->setSpacing(4);
    rowTime->addWidget(new QLabel("경과 시간"), 0, 0);
    lblElapsed_ = new QLabel("00:02:18");
    lblElapsed_->setObjectName("statVal");
    rowTime->addWidget(lblElapsed_, 0, 1);
    rowTime->addWidget(new QLabel("남은 시간"), 1, 0);
    lblRemaining_ = new QLabel("00:01:05");
    lblRemaining_->setObjectName("statVal");
    rowTime->addWidget(lblRemaining_, 1, 1);
    for (int i=0;i<rowTime->count();++i) {
        if (auto* lbl = qobject_cast<QLabel*>(rowTime->itemAt(i)->widget()))
            if (lbl->objectName().isEmpty()) lbl->setObjectName("lbl");
    }

    progressCol->addWidget(lblProgLbl);
    progressCol->addLayout(rowProg);
    progressCol->addLayout(rowTime);
    progressCol->addStretch();
    rowStats->addLayout(progressCol);
    root->addLayout(rowStats);
    root->addSpacing(10);

    // ── 오디오 파형 ───────────────────────────────────────
    auto* lblWave = new QLabel("오디오 파형");
    lblWave->setObjectName("lbl");
    root->addWidget(lblWave);
    root->addSpacing(3);
    waveform_ = new AudioWaveform;
    root->addWidget(waveform_);
    root->addSpacing(10);

    // ── 실시간 자막 미리보기 ──────────────────────────────
    auto* rowPrev = new QHBoxLayout;
    rowPrev->setContentsMargins(0,0,0,0);
    auto* lblPrevLbl = new QLabel("실시간 자막 미리보기");
    lblPrevLbl->setObjectName("lbl");
    lblPreviewConf_ = new QLabel("신뢰도 94%");
    lblPreviewConf_->setObjectName("previewConf");
    lblPreviewConf_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    rowPrev->addWidget(lblPrevLbl);
    rowPrev->addStretch();
    rowPrev->addWidget(lblPreviewConf_);
    root->addLayout(rowPrev);
    root->addSpacing(4);

    lblPreview_ = new QLabel("이것은 매우 지능적인 시스템입니다.");
    lblPreview_->setObjectName("preview");
    lblPreview_->setWordWrap(true);
    lblPreview_->setAlignment(Qt::AlignCenter);
    root->addWidget(lblPreview_);
    root->addSpacing(10);

    // ── 토글 버튼 ─────────────────────────────────────────
    btnToggle_ = new QPushButton("▶  시작");
    btnToggle_->setObjectName("btnStart");
    root->addWidget(btnToggle_);
    root->addStretch();

    // 시그널 연결
    connect(cmbLang_,     QOverload<int>::of(&QComboBox::currentIndexChanged), this, &WhisperWidget::onLangChanged);
    connect(cmbModel_,    QOverload<int>::of(&QComboBox::currentIndexChanged), this, &WhisperWidget::onModelChanged);
    connect(chkTranslate_,&QCheckBox::toggled, this, &WhisperWidget::onTranslateToggled);
    connect(chkSave_,     &QCheckBox::toggled, this, &WhisperWidget::onSaveToggled);
    connect(chkSpeaker_,  &QCheckBox::toggled, this, &WhisperWidget::onSpeakerToggled);
    connect(btnToggle_,   &QPushButton::clicked, this, [this](){ setActive(!active_); });
}

void WhisperWidget::buildHistoryTab(QWidget* parent) {
    auto* root = new QVBoxLayout(parent);
    root->setContentsMargins(12, 10, 12, 10);
    root->setSpacing(0);

    // ── 검색창 ────────────────────────────────────────────
    editSearch_ = new QLineEdit;
    editSearch_->setPlaceholderText("🔍  자막 검색...");
    root->addWidget(editSearch_);
    root->addSpacing(8);

    // ── 필터 버튼 ─────────────────────────────────────────
    auto* rowFilter = new QHBoxLayout;
    rowFilter->setContentsMargins(0,0,0,0);
    rowFilter->setSpacing(6);

    btnFilterAll_  = new QPushButton("전체");
    btnFilterHigh_ = new QPushButton("높은 신뢰도");
    btnFilterLow_  = new QPushButton("낮은 신뢰도");

    for (auto* btn : {btnFilterAll_, btnFilterHigh_, btnFilterLow_}) {
        btn->setObjectName("filterBtn");
        btn->setCheckable(true);
        rowFilter->addWidget(btn);
    }
    btnFilterAll_->setChecked(true);
    rowFilter->addStretch();
    root->addLayout(rowFilter);
    root->addSpacing(8);

    // ── 히스토리 리스트 ───────────────────────────────────
    lstHistory_ = new QListWidget;
    lstHistory_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    root->addWidget(lstHistory_);
    root->addSpacing(8);

    // ── 하단 액션 버튼 ────────────────────────────────────
    auto* rowActions = new QHBoxLayout;
    rowActions->setSpacing(8);
    auto* btnSRT  = new QPushButton("SRT 내보내기");
    auto* btnCopy = new QPushButton("전체 복사");
    btnSRT->setObjectName("actionBtn");
    btnCopy->setObjectName("actionBtn");
    rowActions->addWidget(btnSRT);
    rowActions->addWidget(btnCopy);
    root->addLayout(rowActions);

    // 시그널 연결
    connect(editSearch_, &QLineEdit::textChanged, this, &WhisperWidget::onSearchChanged);
    connect(btnFilterAll_,  &QPushButton::clicked, this, [this](){ btnFilterAll_->setChecked(true); btnFilterHigh_->setChecked(false); btnFilterLow_->setChecked(false); onFilterChanged(0); });
    connect(btnFilterHigh_, &QPushButton::clicked, this, [this](){ btnFilterAll_->setChecked(false); btnFilterHigh_->setChecked(true); btnFilterLow_->setChecked(false); onFilterChanged(1); });
    connect(btnFilterLow_,  &QPushButton::clicked, this, [this](){ btnFilterAll_->setChecked(false); btnFilterHigh_->setChecked(false); btnFilterLow_->setChecked(true); onFilterChanged(2); });
    connect(btnSRT,  &QPushButton::clicked, this, &WhisperWidget::exportSRT);
    connect(btnCopy, &QPushButton::clicked, this, &WhisperWidget::copyAll);
    connect(lstHistory_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item){
        int idx = lstHistory_->row(item);
        if (idx >= 0 && idx < entries_.size())
            emit seekToSubtitle(entries_[idx].startSec);
    });
}

void WhisperWidget::checkWhisperAvailable() {
    QString gpu = detectGpu();
    if (lblGpuName_) lblGpuName_->setText(gpu.isEmpty() ? "CPU 모드" : gpu);

    QProcess check;
    check.start("python", QStringList() << "-c" << "import faster_whisper; print('OK')");
    check.waitForFinished(3000);
    whisperReady_ = (check.readAllStandardOutput().trimmed() == "OK");

    if (!whisperReady_ && btnToggle_) {
        btnToggle_->setText("faster-whisper 미설치");
        btnToggle_->setEnabled(false);
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
        btnToggle_->setText("■  중지");
        startProcess();
        elapsedTimer_ = new QTimer(this);
        elapsedSec_ = 0;
        connect(elapsedTimer_, &QTimer::timeout, this, [this](){
            ++elapsedSec_;
            if (lblElapsed_) lblElapsed_->setText(formatTime(elapsedSec_));
        });
        elapsedTimer_->start(1000);
    } else {
        btnToggle_->setObjectName("btnStart");
        btnToggle_->setText("▶  시작");
        stopProcess();
        if (elapsedTimer_) { elapsedTimer_->stop(); elapsedTimer_->deleteLater(); elapsedTimer_=nullptr; }
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
    if (proc_) { proc_->kill(); proc_->waitForFinished(1000); proc_->deleteLater(); proc_=nullptr; }
}

void WhisperWidget::onProcessOutput() {
    if (!proc_) return;
    while (proc_->canReadLine()) {
        QString line = proc_->readLine().trimmed();
        QStringList parts = line.split('|');
        if (parts.size() < 3) continue;

        SubtitleEntry e;
        e.startSec   = parts[0].toDouble();
        e.endSec     = parts[1].toDouble();
        e.text       = parts[2];
        e.confidence = (parts.size() >= 4) ? (int)(parts[3].toDouble()*100) : 80;

        addSubtitleEntry(e);
        emit subtitleGenerated(e.text, e.startSec, e.endSec, e.confidence);

        // 미리보기 업데이트
        if (lblPreview_) lblPreview_->setText(e.text);
        if (lblPreviewConf_) lblPreviewConf_->setText(QString("신뢰도 %1%").arg(e.confidence));
        if (gauge_) gauge_->setValue(e.confidence);
    }
}

void WhisperWidget::addSubtitleEntry(const SubtitleEntry& e) {
    entries_.prepend(e);
    if (entries_.size() > 500) entries_.removeLast();
    refreshHistoryList();
}

void WhisperWidget::refreshHistoryList() {
    if (!lstHistory_) return;
    lstHistory_->clear();
    for (const SubtitleEntry& e : entries_) {
        if (!searchText_.isEmpty() && !e.text.contains(searchText_, Qt::CaseInsensitive)) continue;
        if (filterMode_ == 1 && e.confidence < 90) continue;
        if (filterMode_ == 2 && e.confidence >= 80) continue;

        QString confStr = QString("%1%").arg(e.confidence);
        QString text = QString("%1  %2  %3")
            .arg(formatTime(e.startSec))
            .arg(e.text)
            .arg(confStr);

        auto* item = new QListWidgetItem(text);
        item->setForeground(confidenceColor(e.confidence));
        lstHistory_->addItem(item);
    }
}

void WhisperWidget::onPositionChanged(double sec) {
    currentPos_ = sec;
    if (waveform_) waveform_->setPosition(sec, mediaDuration_);
}

void WhisperWidget::onProcessError()           {}
void WhisperWidget::onProcessFinished(int)     { if (active_) setActive(false); }
void WhisperWidget::onLangChanged(int idx) {
    static const QStringList c={
        "auto",  // 자동 감지
        "en",    // 영어
        "ko",    // 한국어
        "ja",    // 일본어
        "zh",    // 중국어
        "es",    // 스페인어
        "fr",    // 프랑스어
        "de",    // 독일어
        "it",    // 이탈리아어
        "pt",    // 포르투갈어
        "ru",    // 러시아어
        "ar",    // 아랍어
        "hi",    // 힌디어
        "th",    // 태국어
        "vi",    // 베트남어
    };
    if (idx < c.size()) cfg_.language = c[idx];
}
void WhisperWidget::onModelChanged(int idx) {
    static const QStringList m={"base","medium","large-v3"};
    if (idx < m.size()) cfg_.model = m[idx];
}
void WhisperWidget::onTranslateToggled(bool on) { cfg_.translate = on; }
void WhisperWidget::onSaveToggled(bool on)      { cfg_.saveToFile = on; }
void WhisperWidget::onSpeakerToggled(bool on)   { cfg_.speakerDiarize = on; }
void WhisperWidget::onSearchChanged(const QString& t) { searchText_ = t; refreshHistoryList(); }
void WhisperWidget::onFilterChanged(int f)      { filterMode_ = f; refreshHistoryList(); }

void WhisperWidget::exportSRT() {
    if (entries_.isEmpty() || mediaPath_.isEmpty()) return;
    QString srtPath = QFileInfo(mediaPath_).dir().filePath(
        QFileInfo(mediaPath_).baseName() + ".srt");
    QFile f(srtPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QTextStream out(&f);
    QVector<SubtitleEntry> sorted = entries_;
    std::sort(sorted.begin(), sorted.end(), [](const SubtitleEntry& a, const SubtitleEntry& b){ return a.startSec < b.startSec; });
    for (int i = 0; i < sorted.size(); ++i) {
        const auto& e = sorted[i];
        out << (i+1) << "\n";
        out << formatTime(e.startSec).replace(':',':') << " --> " << formatTime(e.endSec) << "\n";
        out << e.text << "\n\n";
    }
}

void WhisperWidget::copyAll() {
    QStringList lines;
    for (const SubtitleEntry& e : entries_)
        lines.prepend(formatTime(e.startSec) + "  " + e.text);
    QApplication::clipboard()->setText(lines.join('\n'));
}

QString WhisperWidget::formatTime(double sec) const {
    int h=(int)sec/3600, m=((int)sec%3600)/60, s=(int)sec%60;
    if (h>0) return QString("%1:%2:%3").arg(h).arg(m,2,10,QChar('0')).arg(s,2,10,QChar('0'));
    return QString("%1:%2").arg(m,2,10,QChar('0')).arg(s,2,10,QChar('0'));
}

QString WhisperWidget::confidenceLabel(int pct) const {
    if (pct >= 90) return "높음";
    if (pct >= 80) return "보통";
    return "낮음";
}

QColor WhisperWidget::confidenceColor(int pct) const {
    if (pct >= 90) return QColor(0x00,0xc8,0xb4);
    if (pct >= 80) return QColor(0xf0,0xc0,0x40);
    return QColor(0xe0,0x70,0x20);
}
