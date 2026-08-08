#include "ScreenRecorder.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QFileDialog>
#include <QMessageBox>
#include <QStandardPaths>
#include <QDateTime>
#include <QDir>
#include <QProcess>
#include <QTimer>
#include <QCoreApplication>
#include <QFileInfo>

static const char* SR_STYLE = R"(
QWidget { background: #141414; color: #e0e0e0; font-size: 12px; }
QGroupBox {
    border: 1px solid #2a2a2a;
    border-radius: 6px;
    margin-top: 8px;
    padding: 8px;
    color: #888;
    font-size: 11px;
}
QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }
QComboBox, QLineEdit {
    background: #1e1e1e;
    border: 1px solid #333;
    border-radius: 4px;
    padding: 5px 8px;
    color: #ccc;
}
QComboBox:hover, QLineEdit:hover { border-color: #555; }
QCheckBox { color: #aaa; }
QCheckBox::indicator { width: 14px; height: 14px; border: 1px solid #444; border-radius: 3px; background: #1e1e1e; }
QCheckBox::indicator:checked { background: #00c8b4; border-color: #00c8b4; }
QPushButton#btnRecord {
    background: #c62828;
    color: #fff;
    border: none;
    border-radius: 6px;
    padding: 10px 24px;
    font-size: 14px;
    font-weight: 700;
    min-width: 140px;
}
QPushButton#btnRecord:hover { background: #e53935; }
QPushButton#btnRecord[recording="true"] {
    background: #1a1a1a;
    border: 2px solid #c62828;
    color: #c62828;
}
QPushButton#btnBrowse {
    background: #2a2a2a;
    color: #aaa;
    border: 1px solid #444;
    border-radius: 4px;
    padding: 5px 12px;
}
QPushButton#btnBrowse:hover { background: #333; color: #fff; }
QLabel#lblStatus { color: #888; font-size: 11px; }
QLabel#lblTime { color: #00c8b4; font-size: 18px; font-weight: 700; font-family: monospace; }
QLabel#lblRec { color: #c62828; font-size: 11px; font-weight: 700; }
)";

ScreenRecorder::ScreenRecorder(QWidget* parent)
    : QWidget(parent)
{
    setStyleSheet(SR_STYLE);
    buildUI();

    timer_ = new QTimer(this);
    timer_->setInterval(1000);
    connect(timer_, &QTimer::timeout, this, &ScreenRecorder::onTimerTick);
}

ScreenRecorder::~ScreenRecorder() {
    if (recording_) stopRecording();
}

void ScreenRecorder::buildUI() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 12, 16, 12);
    root->setSpacing(12);

    // 헤더
    auto* header = new QLabel("화면 녹화");
    header->setStyleSheet("color:#e0e0e0; font-size:14px; font-weight:700;");
    root->addWidget(header);

    auto* desc = new QLabel("소리누리 창 또는 전체 화면을 MP4로 녹화합니다.\n"
                            "ffmpeg이 설치되어 있어야 합니다.");
    desc->setStyleSheet("color:#666; font-size:11px;");
    desc->setWordWrap(true);
    root->addWidget(desc);

    // 녹화 상태 표시
    auto* statusRow = new QHBoxLayout;
    auto* lblRec = new QLabel("●");
    lblRec->setObjectName("lblRec");
    lblRec->setVisible(false);
    statusRow->addWidget(lblRec);

    lblTime_ = new QLabel("00:00:00");
    lblTime_->setObjectName("lblTime");
    statusRow->addWidget(lblTime_);
    statusRow->addStretch();
    root->addLayout(statusRow);

    // 설정 그룹
    auto* grpSettings = new QGroupBox("녹화 설정");
    auto* grid = new QGridLayout(grpSettings);
    grid->setSpacing(8);

    // 소스
    grid->addWidget(new QLabel("녹화 대상:"), 0, 0);
    cmbSource_ = new QComboBox;
    cmbSource_->addItem("소리누리 창", "window");
    cmbSource_->addItem("전체 화면 (주 모니터)", "desktop");
    grid->addWidget(cmbSource_, 0, 1, 1, 2);

    // FPS
    grid->addWidget(new QLabel("프레임 레이트:"), 1, 0);
    cmbFps_ = new QComboBox;
    cmbFps_->addItem("30 fps", 30);
    cmbFps_->addItem("60 fps", 60);
    grid->addWidget(cmbFps_, 1, 1);

    // 화질
    grid->addWidget(new QLabel("화질:"), 1, 2);
    // (화질 콤보박스는 행 2에 배치)
    cmbQuality_ = new QComboBox;
    cmbQuality_->addItem("고화질 (CRF 18)", 18);
    cmbQuality_->addItem("보통 (CRF 23)", 23);
    cmbQuality_->addItem("저용량 (CRF 28)", 28);
    cmbQuality_->setCurrentIndex(1);
    grid->addWidget(cmbQuality_, 2, 1);

    // 오디오
    chkAudio_ = new QCheckBox("오디오 포함 (스테레오 믹스)");
    chkAudio_->setChecked(false);
    grid->addWidget(chkAudio_, 3, 0, 1, 3);

    root->addWidget(grpSettings);

    // 출력 경로
    auto* grpOutput = new QGroupBox("저장 위치");
    auto* outLay = new QHBoxLayout(grpOutput);
    editOutput_ = new QLineEdit(defaultOutputPath());
    outLay->addWidget(editOutput_, 1);
    btnBrowse_ = new QPushButton("찾아보기");
    btnBrowse_->setObjectName("btnBrowse");
    btnBrowse_->setFocusPolicy(Qt::NoFocus);
    connect(btnBrowse_, &QPushButton::clicked, this, &ScreenRecorder::onBrowseOutput);
    outLay->addWidget(btnBrowse_);
    root->addWidget(grpOutput);

    // 녹화 버튼
    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch();
    btnRecord_ = new QPushButton("● 녹화 시작");
    btnRecord_->setObjectName("btnRecord");
    btnRecord_->setFocusPolicy(Qt::NoFocus);
    connect(btnRecord_, &QPushButton::clicked, this, &ScreenRecorder::toggleRecording);
    btnRow->addWidget(btnRecord_);
    btnRow->addStretch();
    root->addLayout(btnRow);

    // 상태 레이블
    lblStatus_ = new QLabel("ffmpeg이 설치되어 있는지 확인하세요.");
    lblStatus_->setObjectName("lblStatus");
    lblStatus_->setWordWrap(true);
    lblStatus_->setAlignment(Qt::AlignCenter);
    root->addWidget(lblStatus_);

    root->addStretch();
}

QString ScreenRecorder::defaultOutputPath() const {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    if (dir.isEmpty()) dir = QDir::homePath();
    QString ts = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    return QDir::toNativeSeparators(dir + "/Sorinuri_" + ts + ".mp4");
}

void ScreenRecorder::onBrowseOutput() {
    QString path = QFileDialog::getSaveFileName(
        this, "녹화 파일 저장 위치",
        editOutput_->text(),
        "MP4 파일 (*.mp4);;MKV 파일 (*.mkv)"
    );
    if (!path.isEmpty()) editOutput_->setText(path);
}

QString ScreenRecorder::buildFfmpegArgs() const {
    QStringList args;
    int fps = cmbFps_->currentData().toInt();
    int crf = cmbQuality_->currentData().toInt();
    QString source = cmbSource_->currentData().toString();

    // 비디오 입력
    args << "-f" << "gdigrab";
    args << "-framerate" << QString::number(fps);
    if (source == "window") {
        args << "-i" << "title=소리누리";
    } else {
        args << "-i" << "desktop";
    }

    // 오디오 입력 (선택적)
    if (chkAudio_->isChecked()) {
        args << "-f" << "dshow"
             << "-i" << "audio=스테레오 믹스";
    }

    // 출력 설정
    args << "-c:v" << "libx264"
         << "-preset" << "ultrafast"
         << "-crf" << QString::number(crf)
         << "-pix_fmt" << "yuv420p";

    if (chkAudio_->isChecked()) {
        args << "-c:a" << "aac" << "-b:a" << "192k";
    }

    args << "-y"  // 덮어쓰기
         << editOutput_->text();

    return args.join(" ");
}

void ScreenRecorder::toggleRecording() {
    if (recording_) stopRecording();
    else startRecording();
}

void ScreenRecorder::startRecording() {
    // ffmpeg 실행 파일 경로 확인
    QString ffmpegPath = QCoreApplication::applicationDirPath() + "/ffmpeg.exe";
    if (!QFileInfo::exists(ffmpegPath)) {
        ffmpegPath = "ffmpeg";  // PATH에서 찾기
    }

    // 출력 경로 갱신 (새 타임스탬프)
    QString outPath = editOutput_->text();
    if (outPath.isEmpty()) {
        outPath = defaultOutputPath();
        editOutput_->setText(outPath);
    }

    proc_ = new QProcess(this);
    connect(proc_, &QProcess::errorOccurred, this, &ScreenRecorder::onProcessError);
    connect(proc_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ScreenRecorder::onProcessFinished);

    int fps = cmbFps_->currentData().toInt();
    int crf = cmbQuality_->currentData().toInt();
    QString source = cmbSource_->currentData().toString();

    QStringList args;
    args << "-f" << "gdigrab"
         << "-framerate" << QString::number(fps);
    if (source == "window") {
        args << "-i" << "title=소리누리";
    } else {
        args << "-i" << "desktop";
    }
    if (chkAudio_->isChecked()) {
        args << "-f" << "dshow"
             << "-i" << "audio=스테레오 믹스";
    }
    args << "-c:v" << "libx264"
         << "-preset" << "ultrafast"
         << "-crf" << QString::number(crf)
         << "-pix_fmt" << "yuv420p";
    if (chkAudio_->isChecked()) {
        args << "-c:a" << "aac" << "-b:a" << "192k";
    }
    args << "-y" << outPath;

    proc_->start(ffmpegPath, args);
    if (!proc_->waitForStarted(3000)) {
        if (lblStatus_) lblStatus_->setText(
            "⚠ ffmpeg을 시작할 수 없습니다.\n"
            "ffmpeg.exe를 소리누리 폴더에 복사하거나 PATH에 추가하세요.");
        proc_->deleteLater();
        proc_ = nullptr;
        return;
    }

    recording_ = true;
    elapsedSec_ = 0;
    timer_->start();

    btnRecord_->setText("■ 녹화 중지");
    btnRecord_->setProperty("recording", true);
    btnRecord_->style()->unpolish(btnRecord_);
    btnRecord_->style()->polish(btnRecord_);

    if (lblStatus_) lblStatus_->setText("녹화 중: " + outPath);
    emit recordingStarted(outPath);
}

void ScreenRecorder::stopRecording() {
    if (!proc_) return;
    // ffmpeg에 'q' 키 전송 (정상 종료)
    proc_->write("q");
    proc_->waitForFinished(5000);
    if (proc_->state() != QProcess::NotRunning) {
        proc_->terminate();
        proc_->waitForFinished(2000);
    }
    proc_->deleteLater();
    proc_ = nullptr;

    recording_ = false;
    timer_->stop();

    btnRecord_->setText("● 녹화 시작");
    btnRecord_->setProperty("recording", false);
    btnRecord_->style()->unpolish(btnRecord_);
    btnRecord_->style()->polish(btnRecord_);

    QString outPath = editOutput_->text();
    if (lblStatus_) lblStatus_->setText("녹화 완료: " + outPath);

    // 다음 녹화를 위해 파일명 갱신
    editOutput_->setText(defaultOutputPath());

    emit recordingStopped(outPath);
}

void ScreenRecorder::onTimerTick() {
    ++elapsedSec_;
    int h = elapsedSec_ / 3600;
    int m = (elapsedSec_ % 3600) / 60;
    int s = elapsedSec_ % 60;
    if (lblTime_) lblTime_->setText(
        QString("%1:%2:%3")
        .arg(h, 2, 10, QChar('0'))
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0')));
}

void ScreenRecorder::onProcessError() {
    recording_ = false;
    timer_->stop();
    btnRecord_->setText("● 녹화 시작");
    if (lblStatus_) lblStatus_->setText("⚠ 녹화 오류: ffmpeg 실행 실패");
    emit recordingError("ffmpeg 실행 실패");
}

void ScreenRecorder::onProcessFinished(int /*code*/, QProcess::ExitStatus /*status*/) {
    if (recording_) stopRecording();
}
