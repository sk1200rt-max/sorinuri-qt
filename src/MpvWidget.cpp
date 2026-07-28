#include "MpvWidget.h"
#include <QShowEvent>
#include <QResizeEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

MpvWidget::MpvWidget(QWidget* parent) : QWidget(parent) {
    // WA_NativeWindow만 설정 - MPV가 HWND에 렌더링하기 위해 필요
    // WA_PaintOnScreen / WA_NoSystemBackground 제거 → Qt가 배경을 검은색으로 그림
    setAttribute(Qt::WA_NativeWindow);
    setAutoFillBackground(true);

    QPalette pal;
    pal.setColor(QPalette::Window, Qt::black);
    setPalette(pal);

    core_ = new MpvCore(this);
}

MpvWidget::~MpvWidget() = default;

void MpvWidget::paintEvent(QPaintEvent* e) {
    // 항상 검은 배경 그리기 (MPV 렌더링 전/후 모두)
    QPainter p(this);
    p.fillRect(e->rect(), Qt::black);
}

QPaintEngine* MpvWidget::paintEngine() const {
    // WA_PaintOnScreen 없이도 paintEvent가 호출되도록
    return QWidget::paintEngine();
}

void MpvWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (!initialized_) initMpv();
}

void MpvWidget::initMpv() {
    WId wid = winId();
    qInfo() << "[MpvWidget] WID:" << wid;

#ifdef Q_OS_WIN
    // Windows: 창 배경을 검은색으로 강제 설정
    HWND hwnd = reinterpret_cast<HWND>(wid);
    SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)GetStockObject(BLACK_BRUSH));
    InvalidateRect(hwnd, nullptr, TRUE);
#endif

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
