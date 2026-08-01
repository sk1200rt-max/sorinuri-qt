#pragma once
#include <QWidget>

class QLabel;
class QPushButton;
class QSlider;
class MpvCore;

enum class UpscaleMode { Off, NvidiaRTX, AmdFSR, Lanczos, RealESRGAN, RIFE };

class UpscaleWidget : public QWidget {
    Q_OBJECT
public:
    explicit UpscaleWidget(MpvCore* core, QWidget* parent = nullptr);

    UpscaleMode currentMode() const { return mode_; }
    bool compareMode() const { return compareMode_; }

public slots:
    void setMode(UpscaleMode m);
    void setStrength(int level); // 1-4
    void toggleCompare(bool on);

signals:
    void modeChanged(UpscaleMode m);
    void compareToggled(bool on);

private:
    void buildUI();
    void applyToMpv();
    QString detectGpu() const;

    MpvCore*     core_        = nullptr;
    UpscaleMode  mode_        = UpscaleMode::Off;
    int          strength_    = 3;
    bool         compareMode_ = false;

    // UI
    QPushButton* btnOff_       = nullptr;
    QPushButton* btnNvidia_    = nullptr;
    QPushButton* btnAmd_       = nullptr;
    QPushButton* btnLanczos_   = nullptr;
    QPushButton* btnESRGAN_    = nullptr;
    QPushButton* btnRIFE_      = nullptr;
    QSlider*     sldStrength_ = nullptr;
    QLabel*      lblStrVal_   = nullptr;
    QPushButton* btnCompare_  = nullptr;
    QLabel*      lblStatus_   = nullptr;
};
