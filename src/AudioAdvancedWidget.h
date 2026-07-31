#pragma once
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>
#include <QListWidget>
#include <QSlider>
#include <QSettings>
#include "MpvCore.h"

/**
 * AudioAdvancedWidget - 하이엔드 오디오 고급 기능 패널
 *
 * 탭 구성:
 *  1. 컨볼루션 룸 코렉션 - REW IR 파일(.wav) 로드 → 실내 음향 보정
 *  2. VST/VST3 플러그인  - 상용 오디오 플러그인 체인 적용 (경로 등록 방식)
 *
 * 기존 HiFiEngine/MpvCore와 독립적으로 동작.
 * MpvCore::setProperty("af", ...)를 통해 MPV 오디오 필터 체인에 삽입.
 *
 * 주의: VST 플러그인 실제 호스팅은 Windows에서 별도 프로세스(sorinuri-vst-host.exe)가
 *       필요하므로, 이 버전에서는 플러그인 경로 관리 + 활성화 UI만 제공하고
 *       실제 오디오 처리는 MPV lavfi=aconvolve 필터 체인을 통해 구현.
 */
class AudioAdvancedWidget : public QWidget {
    Q_OBJECT
public:
    explicit AudioAdvancedWidget(MpvCore* core, QWidget* parent = nullptr);

    // 현재 활성화된 컨볼루션 IR 파일 경로
    QString activeIrPath() const { return activeIrPath_; }
    bool isConvolutionEnabled() const;

public slots:
    void applyConvolution();     // 현재 IR 파일로 컨볼루션 적용
    void disableConvolution();   // 컨볼루션 해제 (af 초기화)
    void loadSettings();
    void saveSettings();

signals:
    void convolutionChanged(bool enabled, const QString& irPath);

private slots:
    void onBrowseIrFile();
    void onConvolutionToggled(bool on);
    void onGainChanged(int val);

private:
    void setupUI();
    void buildConvolutionTab(QWidget* parent);
    void buildVstTab(QWidget* parent);
    void updateAfChain();

    MpvCore*  core_          = nullptr;
    QSettings settings_;

    // 컨볼루션 탭 UI
    QCheckBox*   convEnableCheck_  = nullptr;
    QLabel*      irPathLabel_      = nullptr;
    QPushButton* browseIrBtn_      = nullptr;
    QSlider*     gainSlider_       = nullptr;
    QLabel*      gainLabel_        = nullptr;
    QLabel*      statusLabel_      = nullptr;

    // VST 탭 UI
    QListWidget* vstListWidget_    = nullptr;
    QPushButton* addVstBtn_        = nullptr;
    QPushButton* removeVstBtn_     = nullptr;
    QLabel*      vstStatusLabel_   = nullptr;

    QString activeIrPath_;
    int     gainDb_        = 0;   // -12 ~ +12 dB
};
