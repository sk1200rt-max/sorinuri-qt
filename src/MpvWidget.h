#pragma once
#include <QWidget>
#include <QLabel>
#include "MpvCore.h"

class MpvWidget : public QWidget {
    Q_OBJECT
public:
    explicit MpvWidget(QWidget* parent = nullptr);
    ~MpvWidget() override;

    MpvCore* core() const { return core_; }
    void loadFile(const QString& path);
    void appendFile(const QString& path);
    void showLogo(bool show);

protected:
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    QPaintEngine* paintEngine() const override;

private:
    MpvCore* core_         = nullptr;
    QLabel*  logoLabel_    = nullptr;
    bool     initialized_  = false;
    void     initMpv();
    void     updateLogoPos();
};
