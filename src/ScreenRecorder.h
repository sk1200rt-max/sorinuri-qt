#pragma once
#include <QWidget>
#include <QProcess>
#include <QTimer>

class QLabel;
class QPushButton;
class QComboBox;
class QSpinBox;
class QLineEdit;

/**
 * ScreenRecorder - 화면 녹화 위젯
 *
 * ffmpeg QProcess 파이프라인을 사용하여 소리누리 창 또는
 * 전체 화면을 MP4/MKV로 녹화합니다.
 *
 * 구현 방식:
 *   - Windows: ffmpeg -f gdigrab -i title="소리누리" 또는 -i desktop
 *   - 오디오: -f dshow -i audio="스테레오 믹스" (선택적)
 *   - 출력: H.264 + AAC, 사용자 지정 경로
 *
 * 단축키: Ctrl+Shift+R (MainWindow에서 처리)
 */
class ScreenRecorder : public QWidget {
    Q_OBJECT
public:
    explicit ScreenRecorder(QWidget* parent = nullptr);
    ~ScreenRecorder();

    bool isRecording() const { return recording_; }

public slots:
    void toggleRecording();
    void startRecording();
    void stopRecording();

signals:
    void recordingStarted(const QString& outputPath);
    void recordingStopped(const QString& outputPath);
    void recordingError(const QString& error);

private slots:
    void onProcessError();
    void onProcessFinished(int code, QProcess::ExitStatus status);
    void onTimerTick();
    void onBrowseOutput();

private:
    void buildUI();
    QString buildFfmpegArgs() const;
    QString defaultOutputPath() const;

    QProcess*    proc_         = nullptr;
    QTimer*      timer_        = nullptr;
    bool         recording_    = false;
    int          elapsedSec_   = 0;

    // UI
    QLabel*      lblStatus_    = nullptr;
    QLabel*      lblTime_      = nullptr;
    QPushButton* btnRecord_    = nullptr;
    QComboBox*   cmbSource_    = nullptr;   // 창 / 전체 화면
    QComboBox*   cmbFps_       = nullptr;   // 30 / 60
    QComboBox*   cmbQuality_   = nullptr;   // 고화질 / 보통 / 저용량
    QCheckBox*   chkAudio_     = nullptr;   // 오디오 포함 여부
    QLineEdit*   editOutput_   = nullptr;   // 출력 경로
    QPushButton* btnBrowse_    = nullptr;
};
