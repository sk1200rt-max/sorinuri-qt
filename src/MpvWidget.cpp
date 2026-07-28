#include "MpvWidget.h"
#include <QShowEvent>
#include <QResizeEvent>
#include <QDebug>

MpvWidget::MpvWidget(QWidget* parent)
    : QWidget(parent)
{
    // MPV가 직접 렌더링하므로 Qt 배경 그리기 비활성화
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setStyleSheet("background: black;");

    core_ = new MpvCore(this);
}

MpvWidget::~MpvWidget() = default;

void MpvWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (!initialized_) {
        initMpv();
    }
}

void MpvWidget::initMpv() {
    WId wid = winId();
    qInfo() << "[MpvWidget] WID:" << wid;

    if (core_->initialize(wid)) {
        initialized_ = true;
        qInfo() << "[MpvWidget] MPV 초기화 완료";
    } else {
        qCritical() << "[MpvWidget] MPV 초기화 실패";
    }
}

void MpvWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    // MPV가 자동으로 크기에 맞게 렌더링
}

void MpvWidget::loadFile(const QString& path) {
    if (!initialized_) {
        // 아직 초기화 안 됨 - 표시 후 자동 재생
        show();
    }
    core_->loadFile(path, false);
}

void MpvWidget::appendFile(const QString& path) {
    core_->loadFile(path, true);
}
