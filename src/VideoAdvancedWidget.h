#pragma once
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>
#include <QSettings>
#include "MpvCore.h"

/**
 * VideoAdvancedWidget - 하이엔드 비디오 고급 기능 패널
 *
 * 기능:
 *  1. 3D LUT 디스플레이 캘리브레이션 - .cube 파일 로드 → ICC 프로파일 수준 색상 정확도
 *  2. 색공간 프리셋 - BT.709 SDR / BT.2020 HDR / DCI-P3 / sRGB
 *
 * 기존 코드에 영향 없는 독립 모듈.
 * MpvCore::setProperty("icc-profile", ...) 및 "lut3d" 옵션으로 적용.
 */
class VideoAdvancedWidget : public QWidget {
    Q_OBJECT
public:
    explicit VideoAdvancedWidget(MpvCore* core, QWidget* parent = nullptr);

    bool isLutEnabled() const;
    QString activeLutPath() const { return activeLutPath_; }

public slots:
    void applyLut();
    void disableLut();
    void loadSettings();
    void saveSettings();

signals:
    void lutChanged(bool enabled, const QString& lutPath);

private slots:
    void onBrowseLut();
    void onLutToggled(bool on);
    void onColorspaceChanged(int idx);

private:
    void setupUI();

    MpvCore*  core_         = nullptr;
    QSettings settings_;

    QCheckBox*   lutEnableCheck_   = nullptr;
    QLabel*      lutPathLabel_     = nullptr;
    QPushButton* browseLutBtn_     = nullptr;
    QComboBox*   colorspaceCombo_  = nullptr;
    QLabel*      statusLabel_      = nullptr;

    QString activeLutPath_;
};
