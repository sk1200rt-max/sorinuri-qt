#pragma once

#include <QWidget>
#include "MpvCore.h"

/**
 * MpvWidget - MPV 비디오 렌더링 위젯
 *
 * Qt 위젯의 WinId(HWND)를 libmpv에 전달하여
 * MPV가 이 위젯 안에 직접 비디오를 렌더링합니다.
 */
class MpvWidget : public QWidget {
    Q_OBJECT

public:
    explicit MpvWidget(QWidget* parent = nullptr);
    ~MpvWidget() override;

    MpvCore* core() const { return core_; }

    // 파일 로드
    void loadFile(const QString& path);
    void appendFile(const QString& path);

protected:
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    QPaintEngine* paintEngine() const override { return nullptr; }

private:
    MpvCore* core_ = nullptr;
    bool     initialized_ = false;

    void initMpv();
};
