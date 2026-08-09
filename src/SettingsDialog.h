#pragma once
#include <QDialog>
#include <QTabWidget>
#include <QComboBox>
#include <QCheckBox>
#include <QSlider>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QSettings>
#include "MpvCore.h"

// 전방 선언
class ScrobbleManager;

/**
 * SettingsDialog - 전문가용 설정 다이얼로그
 *
 * 탭 구성:
 *  1. 오디오 - WASAPI 장치, Exclusive Mode, 패스스루 코덱
 *  2. 비디오 - 하드웨어 디코딩, 업스케일링, HDR 톤매핑
 *  3. 자막   - 폰트, 크기, 색상, 외부 자막
 *  4. 일반   - 언어, 재생 기록, 파일 연결
 *  5. Last.fm - 스크로블링 활성화, 인증, 이력 (v6.18.0 신규)
 */
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(MpvCore* mpv, QWidget* parent = nullptr);
    void setScrobbleManager(ScrobbleManager* mgr) { scrobbleMgr_ = mgr; }

signals:
    void settingsApplied();

private slots:
    void onApply();
    void onOk();
    void refreshAudioDevices();
    // Last.fm
    void onLastfmAuth();
    void onLastfmLogout();

private:
    void setupAudioTab(QTabWidget* tabs);
    void setupVideoTab(QTabWidget* tabs);
    void setupSubtitleTab(QTabWidget* tabs);
    void setupGeneralTab(QTabWidget* tabs);
    void setupLastfmTab(QTabWidget* tabs);   // v6.18.0 신규
    void loadSettings();
    void applyToMpv();
    void updateLastfmStatus();

    MpvCore*        mpv_;
    QSettings       settings_;
    ScrobbleManager* scrobbleMgr_ = nullptr;

    // 오디오 탭
    QComboBox*  audioDeviceCombo_   = nullptr;
    QCheckBox*  exclusiveModeCheck_  = nullptr;
    QLabel*     exclusiveHintLabel_  = nullptr;
    QLabel*     sharedHintLabel_     = nullptr;
    QCheckBox*  passthroughCheck_   = nullptr;
    QCheckBox*  ptAC3_   = nullptr;
    QCheckBox*  ptEAC3_  = nullptr;
    QCheckBox*  ptDTS_   = nullptr;
    QCheckBox*  ptDTSHD_ = nullptr;
    QCheckBox*  ptTrueHD_= nullptr;
    QSlider*    volumeSlider_       = nullptr;
    QLabel*     volumeLabel_        = nullptr;

    // 비디오 탭
    QComboBox*  hwdecCombo_         = nullptr;
    QComboBox*  voCombo_            = nullptr;
    QComboBox*  scalingCombo_       = nullptr;
    QComboBox*  hdrCombo_           = nullptr;
    QCheckBox*  debandCheck_        = nullptr;
    QComboBox*  debandStrengthCombo_= nullptr;
    QCheckBox*  motionSmoothingCheck_ = nullptr;
    QComboBox*  renderProfileCombo_    = nullptr;

    // 자막 탭
    QLineEdit*  subApiKeyEdit_          = nullptr;
    QLineEdit*  deeplApiKeyEdit_        = nullptr;
    QLineEdit*  papagoClientIdEdit_     = nullptr;
    QLineEdit*  papagoClientSecretEdit_ = nullptr;
    QComboBox*  subFontCombo_       = nullptr;
    QSlider*    subSizeSlider_      = nullptr;
    QLabel*     subSizeLabel_       = nullptr;
    QCheckBox*  subBoldCheck_       = nullptr;
    QComboBox*  subColorCombo_      = nullptr;
    QCheckBox*  subShadowCheck_     = nullptr;

    // 일반 탭
    QCheckBox*  rememberPosCheck_   = nullptr;
    QCheckBox*  autoLoadSubCheck_   = nullptr;
    QCheckBox*  resumeCheck_        = nullptr;
    QComboBox*  langCombo_          = nullptr;
    QLineEdit*  screenshotDirEdit_  = nullptr;
    QComboBox*  screenshotFmtCombo_ = nullptr;
    QComboBox*  dsdModeCombo_       = nullptr;
    QCheckBox*  remoteEnabledCheck_ = nullptr;
    QLineEdit*  originalsApiUrlEdit_ = nullptr;

    // Last.fm 탭 (v6.18.0 신규)
    QCheckBox*  lastfmEnabledCheck_ = nullptr;
    QLabel*     lastfmStatusLabel_  = nullptr;
    QPushButton* btnLastfmAuth_     = nullptr;
    QPushButton* btnLastfmLogout_   = nullptr;
    QListWidget* lastfmHistoryList_ = nullptr;
    QLineEdit*  lastfmApiKeyEdit_   = nullptr;
};
