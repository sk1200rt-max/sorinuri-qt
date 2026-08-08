#pragma once
#include <QWidget>
#include <QProcess>
#include <QTimer>
#include <QStringList>
#include <QVector>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class QLabel;
class QComboBox;
class QCheckBox;
class QPushButton;
class QProgressBar;
class QListWidget;
class QLineEdit;
class QStackedWidget;
class QTabBar;

// ─── 자막 항목 ────────────────────────────────────────────────────────────
struct SubtitleEntry {
    double  startSec   = 0.0;
    double  endSec     = 0.0;
    QString text;
    int     confidence = 0;   // 0~100
    int     speaker    = 0;   // 화자 인덱스 (0=기본)
};

// ─── Whisper 설정 ─────────────────────────────────────────────────────────
struct WhisperConfig {
    QString language        = "auto";
    bool    translate       = true;
    QString model           = "medium";
    bool    realtime        = true;
    bool    saveToFile      = false;
    bool    speakerDiarize  = false;
    bool    timestampInclude= true;
};

// ─── 원형 신뢰도 게이지 (커스텀 위젯) ────────────────────────────────────
class ConfidenceGauge : public QWidget {
    Q_OBJECT
public:
    explicit ConfidenceGauge(QWidget* parent = nullptr);
    void setValue(int pct);
    QSize sizeHint() const override { return QSize(88, 88); }
protected:
    void paintEvent(QPaintEvent*) override;
private:
    int value_ = 0;
};

// ─── 오디오 파형 위젯 ─────────────────────────────────────────────────────
class AudioWaveform : public QWidget {
    Q_OBJECT
public:
    explicit AudioWaveform(QWidget* parent = nullptr);
    void setPosition(double pos, double duration);
    QSize sizeHint() const override { return QSize(200, 40); }
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
signals:
    void seekRequested(double sec);
private:
    QVector<float> samples_;
    double pos_      = 0.0;
    double duration_ = 0.0;
    QTimer* animTimer_ = nullptr;
};

// ─── WhisperWidget 메인 ───────────────────────────────────────────────────
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
    void subtitleGenerated(const QString& text, double startSec, double endSec, int confidence);
    void statusChanged(const QString& msg);
    void activeChanged(bool on);
    void seekToSubtitle(double sec);

private slots:
    void onProcessOutput();
    void onProcessError();
    void onProcessFinished(int code);
    void onLangChanged(int idx);
    void onModelChanged(int idx);
    void onTranslateToggled(bool on);
    void onSaveToggled(bool on);
    void onSpeakerToggled(bool on);
    void checkWhisperAvailable();
    void onSearchChanged(const QString& text);
    void onFilterChanged(int filter);   // 0=all, 1=high, 2=low
    void exportSRT();
    void copyAll();
    void translateSRT();  // 자동 번역 API 호출

private:
    void buildUI();
    void buildSettingsTab(QWidget* parent);
    void buildHistoryTab(QWidget* parent);
    void startProcess();
    void stopProcess();
    void addSubtitleEntry(const SubtitleEntry& e);
    void refreshHistoryList();
    QString detectGpu() const;
    QString formatTime(double sec) const;
    QString confidenceLabel(int pct) const;
    QColor  confidenceColor(int pct) const;

    // ── 탭 구조 ──────────────────────────────────────────
    QStackedWidget* stack_          = nullptr;
    QPushButton*    tabSettings_    = nullptr;
    QPushButton*    tabHistory_     = nullptr;

    // ── 설정 탭 ──────────────────────────────────────────
    QLabel*         lblGpuName_     = nullptr;
    QComboBox*      cmbLang_        = nullptr;
    QComboBox*      cmbModel_       = nullptr;
    QCheckBox*      chkTranslate_   = nullptr;
    QCheckBox*      chkSave_        = nullptr;
    QCheckBox*      chkSpeaker_     = nullptr;
    QCheckBox*      chkTimestamp_   = nullptr;
    ConfidenceGauge* gauge_         = nullptr;
    QLabel*         lblConfLabel_   = nullptr;
    QProgressBar*   barProgress_    = nullptr;
    QLabel*         lblElapsed_     = nullptr;
    QLabel*         lblRemaining_   = nullptr;
    AudioWaveform*  waveform_       = nullptr;
    QLabel*         lblPreview_     = nullptr;   // 실시간 자막 미리보기
    QLabel*         lblPreviewConf_ = nullptr;
    QPushButton*    btnToggle_      = nullptr;
    QLabel*         lblStatus_      = nullptr;

    // ── 히스토리 탭 ──────────────────────────────────────
    QLineEdit*      editSearch_     = nullptr;
    QPushButton*    btnFilterAll_   = nullptr;
    QPushButton*    btnFilterHigh_  = nullptr;
    QPushButton*    btnFilterLow_   = nullptr;
    QListWidget*    lstHistory_     = nullptr;

    // ── 상태 ─────────────────────────────────────────────
    WhisperConfig   cfg_;
    QProcess*       proc_           = nullptr;
    QString         mediaPath_;
    bool            active_         = false;
    bool            whisperReady_   = false;
    QVector<SubtitleEntry> entries_;
    int             filterMode_     = 0;
    QString         searchText_;
    double          currentPos_     = 0.0;
    double          mediaDuration_  = 0.0;
    QTimer*         elapsedTimer_   = nullptr;
    int             elapsedSec_     = 0;
    // 자동 번역
    QNetworkAccessManager* translateNam_ = nullptr;
    QPushButton*    btnTranslate_   = nullptr;
    QComboBox*      cmbTranslateTo_ = nullptr;
    QVector<SubtitleEntry> translatedEntries_;
    QString         translateApiUrl_;  // LibreTranslate API URL
    QString         translateApiKey_;  // 선택적 API 키
    void            applyTranslation(const QJsonArray& translations);
    QString         toSrtTime(double sec) const;
};
