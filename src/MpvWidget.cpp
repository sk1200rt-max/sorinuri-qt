#include "MpvWidget.h"
#include <QShowEvent>
#include <QResizeEvent>
#include <QPaintEvent>
#include <QDebug>

MpvWidget::MpvWidget(QWidget* parent) : QWidget(parent) {
    // MPV가 이 위젯의 HWND에 직접 렌더링
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_NoSystemBackground);
    setAutoFillBackground(false);
    // 배경색을 검은색으로 강제 설정
    QPalette pal = palette();
    pal.setColor(QPalette::Window, Qt::black);
    setPalette(pal);

    core_ = new MpvCore(this);
}

MpvWidget::~MpvWidget() = default;

void MpvWidget::paintEvent(QPaintEvent*) {
    // MPV가 렌더링하기 전 검은 배경 그리기
    if (!initialized_) {
        QPainter p(this);
        p.fillRect(rect(), Qt::black);
    }
}

void MpvWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (!initialized_) initMpv();
}

void MpvWidget::initMpv() {
    WId wid = winId();
    qInfo() << "[MpvWidget] WID:" << wid;
    if (core_->initialize(wid)) {
        initialized_ = true;
        qInfo() << "[MpvWidget] 초기화 완료";
    } else {
        qCritical() << "[MpvWidget] 초기화 실패";
    }
}

void MpvWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
}

void MpvWidget::loadFile(const QString& path) {
    if (!initialized_) show();
    core_->loadFile(path, false);
}

void MpvWidget::appendFile(const QString& path) {
    core_->loadFile(path, true);
}
