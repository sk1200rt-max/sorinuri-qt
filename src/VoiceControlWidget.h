#pragma once
#include <QWidget>
#include <QProcess>
#include <QTimer>
#include <QStringList>

class QPushButton;
class QLabel;
class QComboBox;
class QCheckBox;
class QProgressBar;
class QListWidget;

/**
 * VoiceControlWidget - Whisper 기반 로컬 음성 명령 제어
 *
 * - 마이크 입력을 Whisper.cpp로 실시간 인식
 * - "재생", "정지", "다음 곡", "이전 곡", "볼륨 XX" 등 명령 처리
 * - 인터넷 연결 불필요 (완전 로컬 처리)
 */
class VoiceControlWidget : public QWidget {
    Q_OBJECT
public:
    explicit VoiceControlWidget(QWidget* parent = nullptr);
    ~VoiceControlWidget() override;

    bool isListening() const { return listening_; }

public slots:
    void startListening();
    void stopListening();
    void setModel(const QString& modelPath);

signals:
    // 인식된 명령 시그널
    void commandPlay();
    void commandPause();
    void commandStop();
    void commandNext();
    void commandPrev();
    void commandSeek(double seconds);
    void commandVolume(int volume);
    void commandMute();
    void commandFullscreen();
    void statusChanged(const QString& msg);

private slots:
    void onRecordFinished(int exitCode, QProcess::ExitStatus status);
    void onTranscribeFinished(int exitCode, QProcess::ExitStatus status);
    void onListenTimer();

private:
    void buildUI();
    void recordChunk();
    void transcribeChunk(const QString& audioPath);
    void parseCommand(const QString& text);
    QString findWhisper() const;
    QString findFFmpeg() const;

    QPushButton*  btnToggle_     = nullptr;
    QLabel*       lblStatus_     = nullptr;
    QLabel*       lblLastCmd_    = nullptr;
    QComboBox*    cmbModel_      = nullptr;
    QCheckBox*    chkAutoListen_ = nullptr;
    QProgressBar* progressBar_   = nullptr;
    QListWidget*  lstHistory_    = nullptr;

    QProcess*     recordProcess_     = nullptr;
    QProcess*     transcribeProcess_ = nullptr;
    QTimer*       listenTimer_       = nullptr;

    bool          listening_     = false;
    QString       modelPath_;
    QString       tempAudioPath_;
    int           chunkDuration_ = 3;  // 3초 단위 녹음
};
