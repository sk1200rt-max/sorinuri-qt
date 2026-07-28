#pragma once

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
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
    QPaintEngine* paintEngine() const override { return nullptr; }

private:
    MpvCore* core_        = nullptr;
    bool     initialized_ = false;
    QLabel*  logoOverlay_ = nullptr;

    void initMpv();
    void updateLogoPosition();
};
