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

/**
 * SettingsDialog - 전문가용 설정 다이얼로그
 *
 * 탭 구성:
 *  1. 오디오 - WASAPI 장치, Exclusive Mode, 패스스루 코덱
 *  2. 비디오 - 하드웨어 디코딩, 업스케일링, HDR 톤매핑
 *  3. 자막   - 폰트, 크기, 색상, 외부 자막
 *  4. 일반   - 언어, 재생 기록, 파일 연결
 */
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(MpvCore* mpv, QWidget* parent = nullptr);

signals:
    void settingsApplied();

private slots:
    void onApply();
    void onOk();
    void refreshAudioDevices();

private:
    void setupAudioTab(QTabWidget* tabs);
    void setupVideoTab(QTabWidget* tabs);
    void setupSubtitleTab(QTabWidget* tabs);
    void setupGeneralTab(QTabWidget* tabs);
    void loadSettings();
    void applyToMpv();

    MpvCore*   mpv_;
    QSettings  settings_;

    // 오디오 탭
    QComboBox*  audioDeviceCombo_   = nullptr;
    QCheckBox*  exclusiveModeCheck_  = nullptr;
    QLabel*     exclusiveHintLabel_  = nullptr;   // 독점 모드 활성화 시 안내 (노란)
    QLabel*     sharedHintLabel_     = nullptr;   // 공유 모드 안내 (녹색, 기본 표시)
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
    QCheckBox*  motionSmoothingCheck_ = nullptr;  // 모션 스무딩 (프레임 보간)
    QComboBox*  renderProfileCombo_    = nullptr;  // GPU 렌더링 프로파일 (Eco/Balanced/Quality/HiEnd)

    // 자막 탭
    QLineEdit*  subApiKeyEdit_          = nullptr;  // OpenSubtitles API 키
    QLineEdit*  deeplApiKeyEdit_        = nullptr;  // DeepL 번역 API 키
    QLineEdit*  papagoClientIdEdit_     = nullptr;  // 파파고 Client ID
    QLineEdit*  papagoClientSecretEdit_ = nullptr;  // 파파고 Client Secret
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
    // DSD 설정
    QComboBox*  dsdModeCombo_       = nullptr;
    // 스마트폰 리모컨
    QCheckBox*  remoteEnabledCheck_ = nullptr;
};
