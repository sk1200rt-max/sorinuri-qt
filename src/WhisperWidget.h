#pragma once
#include <QWidget>
#include <QProcess>
#include <QTimer>
#include <QStringList>

class QLabel;
class QComboBox;
class QCheckBox;
class QPushButton;
class QProgressBar;
class QListWidget;
class QSlider;

struct WhisperConfig {
    QString language   = "auto";   // auto, en, ja, zh, ko ...
    bool    translate  = true;     // 한국어 번역 여부
    QString model      = "medium"; // base, small, medium, large-v3
    bool    realtime   = true;     // 실시간 vs 오프라인 사전 생성
    bool    saveToFile = false;    // SRT 파일 저장
};

class WhisperWidget : public QWidget {
    Q_OBJECT
public:
    explicit WhisperWidget(QWidget* parent = nullptr);
    ~WhisperWidget();

    void setMediaFile(const QString& path);
    bool isActive() const { return active_; }
    WhisperConfig config() const { return cfg_; }

public slots:
    void setActive(bool on);
    void onPositionChanged(double sec);

signals:
    void subtitleGenerated(const QString& text, double startSec, double endSec);
    void statusChanged(const QString& msg);
    void activeChanged(bool on);

private slots:
    void onProcessOutput();
    void onProcessError();
    void onProcessFinished(int code);
    void onLangChanged(int idx);
    void onModelChanged(int idx);
    void onTranslateToggled(bool on);
    void onSaveToggled(bool on);
    void checkWhisperAvailable();

private:
    void buildUI();
    void startProcess();
    void stopProcess();
    void updateStatusLabel();
    QString detectGpu() const;

    // UI
    QLabel*       lblStatus_    = nullptr;
    QLabel*       lblGpu_       = nullptr;
    QComboBox*    cmbLang_      = nullptr;
    QComboBox*    cmbModel_     = nullptr;
    QCheckBox*    chkTranslate_ = nullptr;
    QCheckBox*    chkSave_      = nullptr;
    QProgressBar* barConfidence_= nullptr;
    QListWidget*  lstRecent_    = nullptr;
    QPushButton*  btnToggle_    = nullptr;

    // State
    WhisperConfig cfg_;
    QProcess*     proc_         = nullptr;
    QString       mediaPath_;
    bool          active_       = false;
    bool          whisperReady_ = false;
    QStringList   recentLines_;
};
