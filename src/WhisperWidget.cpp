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
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QFont>

static const char* STYLE_PANEL = R"(
QWidget { background: transparent; color: #cccccc; font-family: 'Malgun Gothic'; }
QLabel#lblSection { color: #00c8b4; font-weight: bold; font-size: 11px; }
QLabel#lblGpu { color: #00c8b4; font-size: 10px; }
QComboBox {
    background: #1e1e1e; border: 1px solid #333; border-radius: 4px;
    color: #cccccc; padding: 3px 8px; font-size: 11px;
}
QComboBox::drop-down { border: none; width: 20px; }
QComboBox QAbstractItemView { background: #1e1e1e; color: #cccccc; selection-background-color: #00c8b4; }
QCheckBox { color: #cccccc; font-size: 11px; spacing: 6px; }
QCheckBox::indicator { width: 14px; height: 14px; border: 1px solid #555; border-radius: 3px; background: #1e1e1e; }
QCheckBox::indicator:checked { background: #00c8b4; border-color: #00c8b4; }
QProgressBar {
    background: #1e1e1e; border: 1px solid #333; border-radius: 3px;
    height: 6px; text-align: right; color: #888; font-size: 9px;
}
QProgressBar::chunk { background: #00c8b4; border-radius: 3px; }
QListWidget {
    background: #141414; border: 1px solid #2a2a2a; border-radius: 4px;
    color: #888; font-size: 10px; font-family: 'Malgun Gothic';
}
QListWidget::item { padding: 2px 6px; border-bottom: 1px solid #1e1e1e; }
QListWidget::item:selected { background: #1e1e1e; color: #00c8b4; }
QPushButton#btnToggle {
    background: #00c8b4; color: #000; font-weight: bold; font-size: 11px;
    border: none; border-radius: 5px; padding: 5px 16px;
}
QPushButton#btnToggle:checked {
    background: #cc4444; color: #fff;
}
QPushButton#btnToggle:hover { opacity: 0.85; }
)";

WhisperWidget::WhisperWidget(QWidget* parent) : QWidget(parent) {
    setStyleSheet(STYLE_PANEL);
    buildUI();
    checkWhisperAvailable();
}

WhisperWidget::~WhisperWidget() {
    stopProcess();
}

void WhisperWidget::buildUI() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 8, 12, 8);
    root->setSpacing(8);

    // ── 상단: 상태 + GPU 배지 ──────────────────────────────
    auto* rowTop = new QHBoxLayout;
    auto* lblTitle = new QLabel("AI 자막 설정");
    lblTitle->setObjectName("lblSection");
    QFont f = lblTitle->font(); f.setPointSize(11); f.setBold(true);
    lblTitle->setFont(f);

    lblGpu_ = new QLabel("GPU 감지 중...");
    lblGpu_->setObjectName("lblGpu");
    lblGpu_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    rowTop->addWidget(lblTitle);
    rowTop->addStretch();
    rowTop->addWidget(lblGpu_);
    root->addLayout(rowTop);

    // ── 설정 그리드 ───────────────────────────────────────
    auto* grid = new QGridLayout;
    grid->setColumnStretch(1, 1);
    grid->setVerticalSpacing(6);
    grid->setHorizontalSpacing(10);

    // 인식 언어
    auto* lblLang = new QLabel("인식 언어:");
    cmbLang_ = new QComboBox;
    cmbLang_->addItems({"자동 감지", "영어", "일본어", "중국어", "스페인어", "프랑스어", "독일어"});
    grid->addWidget(lblLang, 0, 0);
    grid->addWidget(cmbLang_, 0, 1);

    // Whisper 모델
    auto* lblModel = new QLabel("Whisper 모델:");
    cmbModel_ = new QComboBox;
    cmbModel_->addItems({"base (빠름, 저사양)", "medium (권장)", "large-v3 (최고 품질)"});
    cmbModel_->setCurrentIndex(1);
    grid->addWidget(lblModel, 1, 0);
    grid->addWidget(cmbModel_, 1, 1);

    root->addLayout(grid);

    // ── 토글 옵션 ─────────────────────────────────────────
    chkTranslate_ = new QCheckBox("한국어로 번역");
    chkTranslate_->setChecked(true);
    chkSave_ = new QCheckBox("SRT 파일로 저장");

    auto* rowChk = new QHBoxLayout;
    rowChk->addWidget(chkTranslate_);
    rowChk->addSpacing(16);
    rowChk->addWidget(chkSave_);
    rowChk->addStretch();
    root->addLayout(rowChk);

    // ── 인식 정확도 ───────────────────────────────────────
    auto* lblConf = new QLabel("인식 정확도:");
    barConfidence_ = new QProgressBar;
    barConfidence_->setRange(0, 100);
    barConfidence_->setValue(0);
    barConfidence_->setFormat("%p%");
    barConfidence_->setFixedHeight(14);

    auto* rowConf = new QHBoxLayout;
    rowConf->addWidget(lblConf);
    rowConf->addWidget(barConfidence_);
    root->addLayout(rowConf);

    // ── 최근 자막 목록 ────────────────────────────────────
    auto* lblRecent = new QLabel("최근 자막 (최근 5개)");
    lblRecent->setObjectName("lblSection");
    root->addWidget(lblRecent);

    lstRecent_ = new QListWidget;
    lstRecent_->setFixedHeight(90);
    lstRecent_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    root->addWidget(lstRecent_);

    // ── 상태 레이블 ───────────────────────────────────────
    lblStatus_ = new QLabel("faster-whisper 확인 중...");
    lblStatus_->setStyleSheet("color:#666; font-size:10px;");
    root->addWidget(lblStatus_);

    // ── 활성화 버튼 ───────────────────────────────────────
    btnToggle_ = new QPushButton("AI 자막 시작");
    btnToggle_->setObjectName("btnToggle");
    btnToggle_->setCheckable(true);
    btnToggle_->setEnabled(false);
    root->addWidget(btnToggle_);

    root->addStretch();

    // 시그널 연결
    connect(cmbLang_,     QOverload<int>::of(&QComboBox::currentIndexChanged), this, &WhisperWidget::onLangChanged);
    connect(cmbModel_,    QOverload<int>::of(&QComboBox::currentIndexChanged), this, &WhisperWidget::onModelChanged);
    connect(chkTranslate_,&QCheckBox::toggled, this, &WhisperWidget::onTranslateToggled);
    connect(chkSave_,     &QCheckBox::toggled, this, &WhisperWidget::onSaveToggled);
    connect(btnToggle_,   &QPushButton::toggled, this, &WhisperWidget::setActive);
}

void WhisperWidget::checkWhisperAvailable() {
    // faster-whisper 설치 여부 확인 (Python 스크립트 경로)
    QString appDir = QCoreApplication::applicationDirPath();
    QString scriptPath = appDir + "/whisper_server.py";

    // GPU 감지
    QString gpuInfo = detectGpu();
    lblGpu_->setText(gpuInfo);

    // Python + faster-whisper 확인
    QProcess check;
    check.start("python", QStringList() << "-c" << "import faster_whisper; print('OK')");
    check.waitForFinished(3000);
    QString out = check.readAllStandardOutput().trimmed();

    if (out == "OK") {
        whisperReady_ = true;
        btnToggle_->setEnabled(true);
        lblStatus_->setText("faster-whisper 준비 완료");
        lblStatus_->setStyleSheet("color:#00c8b4; font-size:10px;");
    } else {
        whisperReady_ = false;
        btnToggle_->setEnabled(false);
        lblStatus_->setText("faster-whisper 미설치 — 포터블 패키지에 포함됨");
        lblStatus_->setStyleSheet("color:#888; font-size:10px;");
    }
}

QString WhisperWidget::detectGpu() const {
    // NVIDIA 감지
    QProcess p;
    p.start("nvidia-smi", QStringList() << "--query-gpu=name" << "--format=csv,noheader");
    p.waitForFinished(2000);
    QString out = p.readAllStandardOutput().trimmed();
    if (!out.isEmpty()) {
        return QString("● GPU 가속 활성 (%1)").arg(out.split('\n').first().trimmed());
    }
    return "● CPU 모드 (GPU 미감지)";
}

void WhisperWidget::setMediaFile(const QString& path) {
    mediaPath_ = path;
}

void WhisperWidget::setActive(bool on) {
    if (on == active_) return;
    active_ = on;
    btnToggle_->setChecked(on);
    btnToggle_->setText(on ? "AI 자막 중지" : "AI 자막 시작");

    if (on) {
        startProcess();
    } else {
        stopProcess();
    }
    emit activeChanged(on);
}

void WhisperWidget::startProcess() {
    if (!whisperReady_ || mediaPath_.isEmpty()) return;

    stopProcess();
    proc_ = new QProcess(this);
    connect(proc_, &QProcess::readyReadStandardOutput, this, &WhisperWidget::onProcessOutput);
    connect(proc_, &QProcess::readyReadStandardError,  this, &WhisperWidget::onProcessError);
    connect(proc_, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &WhisperWidget::onProcessFinished);

    QStringList args;
    args << QCoreApplication::applicationDirPath() + "/whisper_server.py"
         << "--file" << mediaPath_
         << "--model" << cfg_.model
         << "--language" << cfg_.language
         << (cfg_.translate ? "--translate" : "")
         << (cfg_.saveToFile ? "--save-srt" : "");
    args.removeAll("");

    proc_->start("python", args);
    lblStatus_->setText("AI 자막 처리 중...");
    lblStatus_->setStyleSheet("color:#00c8b4; font-size:10px;");
}

void WhisperWidget::stopProcess() {
    if (proc_) {
        proc_->kill();
        proc_->waitForFinished(1000);
        proc_->deleteLater();
        proc_ = nullptr;
    }
    lblStatus_->setText("AI 자막 대기 중");
    lblStatus_->setStyleSheet("color:#666; font-size:10px;");
    barConfidence_->setValue(0);
}

void WhisperWidget::onProcessOutput() {
    if (!proc_) return;
    while (proc_->canReadLine()) {
        QString line = proc_->readLine().trimmed();
        // 형식: START_SEC|END_SEC|TEXT|CONFIDENCE
        QStringList parts = line.split('|');
        if (parts.size() >= 3) {
            double start = parts[0].toDouble();
            double end   = parts[1].toDouble();
            QString text = parts[2];
            int conf = (parts.size() >= 4) ? (int)(parts[3].toDouble() * 100) : 80;

            barConfidence_->setValue(conf);
            emit subtitleGenerated(text, start, end);

            // 최근 자막 목록 업데이트
            QString timeStr = QString("%1:%2")
                .arg((int)start/60, 2, 10, QChar('0'))
                .arg((int)start%60, 2, 10, QChar('0'));
            recentLines_.prepend(timeStr + "  " + text);
            if (recentLines_.size() > 5) recentLines_.removeLast();

            lstRecent_->clear();
            for (int i = 0; i < recentLines_.size(); ++i) {
                lstRecent_->addItem(recentLines_[i]);
                if (i == 0) {
                    lstRecent_->item(0)->setForeground(QColor("#00c8b4"));
                }
            }
        }
    }
}

void WhisperWidget::onProcessError() {
    // 오류 로그 (디버그용)
}

void WhisperWidget::onProcessFinished(int code) {
    if (active_) {
        lblStatus_->setText(QString("처리 완료 (종료 코드: %1)").arg(code));
        active_ = false;
        btnToggle_->setChecked(false);
        btnToggle_->setText("AI 자막 시작");
    }
}

void WhisperWidget::onLangChanged(int idx) {
    static const QStringList codes = {"auto","en","ja","zh","es","fr","de"};
    if (idx < codes.size()) cfg_.language = codes[idx];
}

void WhisperWidget::onModelChanged(int idx) {
    static const QStringList models = {"base","medium","large-v3"};
    if (idx < models.size()) cfg_.model = models[idx];
}

void WhisperWidget::onTranslateToggled(bool on) { cfg_.translate = on; }
void WhisperWidget::onSaveToggled(bool on)      { cfg_.saveToFile = on; }
void WhisperWidget::onPositionChanged(double)   {}
void WhisperWidget::updateStatusLabel()         {}
