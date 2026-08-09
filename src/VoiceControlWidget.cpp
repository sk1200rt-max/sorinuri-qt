#include "VoiceControlWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QProgressBar>
#include <QListWidget>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QTimer>
#include <QDateTime>

VoiceControlWidget::VoiceControlWidget(QWidget* parent) : QWidget(parent) {
    buildUI();

    tempAudioPath_ = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                     + "/sorinuri_voice_chunk.wav";

    listenTimer_ = new QTimer(this);
    listenTimer_->setInterval(chunkDuration_ * 1000 + 500);
    connect(listenTimer_, &QTimer::timeout, this, &VoiceControlWidget::onListenTimer);
}

VoiceControlWidget::~VoiceControlWidget() {
    stopListening();
}

void VoiceControlWidget::buildUI() {
    setStyleSheet("background: #111; color: #ccc;");
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    // 헤더
    auto* lblHdr = new QLabel("🎙  AI 음성 명령 제어");
    lblHdr->setStyleSheet("color: #00D4B4; font-size: 14px; font-weight: bold;");
    root->addWidget(lblHdr);

    // 모델 선택
    auto* rowModel = new QHBoxLayout;
    rowModel->addWidget(new QLabel("Whisper 모델:"));
    cmbModel_ = new QComboBox;
    cmbModel_->addItem("tiny (빠름, 정확도 낮음)", "tiny");
    cmbModel_->addItem("base (균형)", "base");
    cmbModel_->addItem("small (정확도 높음)", "small");
    cmbModel_->setCurrentIndex(1);
    cmbModel_->setStyleSheet(
        "QComboBox { background: #1a1a1a; border: 1px solid #2a2a2a; border-radius: 4px;"
        "  padding: 4px 8px; color: #e0e0e0; }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView { background: #1a1a1a; color: #e0e0e0; }");
    rowModel->addWidget(cmbModel_, 1);
    root->addLayout(rowModel);

    // 자동 청취 옵션
    chkAutoListen_ = new QCheckBox("재생 중 자동 청취");
    chkAutoListen_->setStyleSheet("color: #aaa; font-size: 12px;");
    root->addWidget(chkAutoListen_);

    // 시작/정지 버튼
    btnToggle_ = new QPushButton("🎙  음성 인식 시작");
    btnToggle_->setFixedHeight(40);
    btnToggle_->setStyleSheet(
        "QPushButton { background: #1a3a2a; color: #00D4B4; border: 1px solid #00D4B4;"
        "  border-radius: 6px; font-size: 13px; font-weight: bold; }"
        "QPushButton:hover { background: #1e4a3a; }"
        "QPushButton:checked { background: #3a1a1a; color: #ff6666; border-color: #ff6666; }");
    btnToggle_->setCheckable(true);
    connect(btnToggle_, &QPushButton::toggled, this, [this](bool on) {
        if (on) startListening();
        else    stopListening();
    });
    root->addWidget(btnToggle_);

    // 상태 표시
    lblStatus_ = new QLabel("대기 중");
    lblStatus_->setStyleSheet("color: #666; font-size: 11px;");
    root->addWidget(lblStatus_);

    // 마지막 인식 명령
    auto* rowCmd = new QHBoxLayout;
    rowCmd->addWidget(new QLabel("마지막 명령:"));
    lblLastCmd_ = new QLabel("—");
    lblLastCmd_->setStyleSheet("color: #00D4B4; font-size: 12px;");
    rowCmd->addWidget(lblLastCmd_, 1);
    root->addLayout(rowCmd);

    // 진행 표시
    progressBar_ = new QProgressBar;
    progressBar_->setRange(0, 0);  // 무한 진행
    progressBar_->setFixedHeight(3);
    progressBar_->setTextVisible(false);
    progressBar_->setStyleSheet(
        "QProgressBar { background: #1a1a1a; border: none; border-radius: 1px; }"
        "QProgressBar::chunk { background: #00D4B4; }");
    progressBar_->hide();
    root->addWidget(progressBar_);

    // 명령 이력
    root->addWidget(new QLabel("인식 이력:"));
    lstHistory_ = new QListWidget;
    lstHistory_->setStyleSheet(
        "QListWidget { background: #0d0d0d; border: 1px solid #1e1e1e; border-radius: 4px;"
        "  color: #aaa; font-size: 11px; }"
        "QListWidget::item { padding: 4px 8px; border-bottom: 1px solid #1a1a1a; }");
    lstHistory_->setMaximumHeight(150);
    root->addWidget(lstHistory_);

    // 지원 명령어 안내
    auto* lblHelp = new QLabel(
        "지원 명령어: 재생 / 정지 / 일시정지 / 다음 곡 / 이전 곡 /\n"
        "볼륨 [숫자] / 음소거 / 전체화면 / [숫자]초 앞으로 / [숫자]초 뒤로");
    lblHelp->setStyleSheet("color: #444; font-size: 10px;");
    lblHelp->setWordWrap(true);
    root->addWidget(lblHelp);

    root->addStretch();
}

void VoiceControlWidget::startListening() {
    if (listening_) return;
    listening_ = true;
    btnToggle_->setText("⏹  음성 인식 중지");
    lblStatus_->setText("청취 중...");
    lblStatus_->setStyleSheet("color: #00D4B4; font-size: 11px;");
    emit statusChanged("음성 인식 시작");
    recordChunk();
}

void VoiceControlWidget::stopListening() {
    if (!listening_) return;
    listening_ = false;
    listenTimer_->stop();
    if (recordProcess_ && recordProcess_->state() != QProcess::NotRunning) {
        recordProcess_->kill();
    }
    if (transcribeProcess_ && transcribeProcess_->state() != QProcess::NotRunning) {
        transcribeProcess_->kill();
    }
    btnToggle_->setChecked(false);
    btnToggle_->setText("🎙  음성 인식 시작");
    lblStatus_->setText("대기 중");
    lblStatus_->setStyleSheet("color: #666; font-size: 11px;");
    progressBar_->hide();
    emit statusChanged("음성 인식 중지");
}

void VoiceControlWidget::recordChunk() {
    if (!listening_) return;

    QString ffmpeg = findFFmpeg();
    if (ffmpeg.isEmpty()) {
        lblStatus_->setText("ffmpeg.exe를 찾을 수 없습니다");
        stopListening();
        return;
    }

    progressBar_->show();
    recordProcess_ = new QProcess(this);
    connect(recordProcess_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &VoiceControlWidget::onRecordFinished);

    // ffmpeg로 마이크 입력 3초 녹음 (Windows DirectShow)
    QStringList args;
    args << "-f" << "dshow"
         << "-i" << "audio=default"
         << "-t" << QString::number(chunkDuration_)
         << "-ar" << "16000"
         << "-ac" << "1"
         << "-y"
         << tempAudioPath_;
    recordProcess_->start(ffmpeg, args);
}

void VoiceControlWidget::onRecordFinished(int exitCode, QProcess::ExitStatus /*status*/) {
    Q_UNUSED(exitCode)
    if (recordProcess_) {
        recordProcess_->deleteLater();
        recordProcess_ = nullptr;
    }
    if (!listening_) return;

    if (QFile::exists(tempAudioPath_)) {
        transcribeChunk(tempAudioPath_);
    } else {
        // 녹음 실패 시 재시도
        QTimer::singleShot(500, this, &VoiceControlWidget::recordChunk);
    }
}

void VoiceControlWidget::transcribeChunk(const QString& audioPath) {
    QString whisper = findWhisper();
    if (whisper.isEmpty()) {
        lblStatus_->setText("whisper.exe를 찾을 수 없습니다");
        stopListening();
        return;
    }

    QString model = cmbModel_->currentData().toString();
    transcribeProcess_ = new QProcess(this);
    transcribeProcess_->setProcessChannelMode(QProcess::MergedChannels);
    connect(transcribeProcess_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &VoiceControlWidget::onTranscribeFinished);

    QStringList args;
    args << "-m" << (QCoreApplication::applicationDirPath() + "/models/ggml-" + model + ".bin")
         << "-f" << audioPath
         << "-l" << "ko"
         << "--no-timestamps"
         << "-otxt";
    transcribeProcess_->start(whisper, args);
}

void VoiceControlWidget::onTranscribeFinished(int exitCode, QProcess::ExitStatus /*status*/) {
    Q_UNUSED(exitCode)
    QString output;
    if (transcribeProcess_) {
        output = QString::fromLocal8Bit(transcribeProcess_->readAllStandardOutput()).trimmed();
        transcribeProcess_->deleteLater();
        transcribeProcess_ = nullptr;
    }

    if (!output.isEmpty()) {
        parseCommand(output);
    }

    // 계속 청취
    if (listening_) {
        QTimer::singleShot(100, this, &VoiceControlWidget::recordChunk);
    }
}

void VoiceControlWidget::parseCommand(const QString& rawText) {
    QString text = rawText.toLower().trimmed();
    // 공백 정규화
    text.replace(QRegularExpression("\\s+"), " ");

    QString cmdName;

    if (text.contains("재생") || text.contains("play")) {
        emit commandPlay();
        cmdName = "재생";
    } else if (text.contains("정지") || text.contains("stop")) {
        emit commandStop();
        cmdName = "정지";
    } else if (text.contains("일시정지") || text.contains("pause") || text.contains("멈춰")) {
        emit commandPause();
        cmdName = "일시정지";
    } else if (text.contains("다음") || text.contains("next")) {
        emit commandNext();
        cmdName = "다음 곡";
    } else if (text.contains("이전") || text.contains("prev")) {
        emit commandPrev();
        cmdName = "이전 곡";
    } else if (text.contains("음소거") || text.contains("mute")) {
        emit commandMute();
        cmdName = "음소거";
    } else if (text.contains("전체화면") || text.contains("fullscreen")) {
        emit commandFullscreen();
        cmdName = "전체화면";
    } else {
        // 볼륨 명령: "볼륨 50", "volume 70"
        QRegularExpression volRe(R"(볼륨\s*(\d+)|volume\s*(\d+))");
        auto m = volRe.match(text);
        if (m.hasMatch()) {
            int vol = m.captured(1).isEmpty() ? m.captured(2).toInt() : m.captured(1).toInt();
            vol = qBound(0, vol, 100);
            emit commandVolume(vol);
            cmdName = QString("볼륨 %1").arg(vol);
        }

        // 탐색 명령: "10초 앞으로", "30초 뒤로"
        QRegularExpression seekRe(R"((\d+)초\s*(앞|뒤|forward|back))");
        auto sm = seekRe.match(text);
        if (sm.hasMatch()) {
            double sec = sm.captured(1).toDouble();
            bool forward = sm.captured(2).contains("앞") || sm.captured(2).contains("forward");
            emit commandSeek(forward ? sec : -sec);
            cmdName = QString("%1초 %2").arg((int)sec).arg(forward ? "앞으로" : "뒤로");
        }
    }

    if (!cmdName.isEmpty()) {
        lblLastCmd_->setText(cmdName);
        QString timeStr = QDateTime::currentDateTime().toString("hh:mm:ss");
        lstHistory_->insertItem(0, QString("[%1] %2 → %3").arg(timeStr, rawText.left(30), cmdName));
        if (lstHistory_->count() > 50) lstHistory_->takeItem(lstHistory_->count()-1);
        emit statusChanged(QString("명령 인식: %1").arg(cmdName));
    }
}

void VoiceControlWidget::onListenTimer() {
    if (listening_) recordChunk();
}

void VoiceControlWidget::setModel(const QString& modelPath) {
    modelPath_ = modelPath;
}

QString VoiceControlWidget::findWhisper() const {
    QString appDir = QCoreApplication::applicationDirPath();
    QStringList candidates = {
        appDir + "/whisper.exe",
        appDir + "/whisper-cli.exe",
        appDir + "/main.exe"  // whisper.cpp 기본 실행 파일명
    };
    for (const QString& c : candidates) {
        if (QFile::exists(c)) return c;
    }
    return {};
}

QString VoiceControlWidget::findFFmpeg() const {
    QString appDir = QCoreApplication::applicationDirPath();
    QString ffmpeg = appDir + "/ffmpeg.exe";
    if (QFile::exists(ffmpeg)) return ffmpeg;
    return "ffmpeg";
}
