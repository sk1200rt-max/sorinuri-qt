#include "MpvWidget.h"
#include <QShowEvent>
#include <QResizeEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPixmap>
#include <QDebug>
#include <QApplication>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

MpvWidget::MpvWidget(QWidget* parent) : QWidget(parent) {
    // MPV가 직접 렌더링하는 네이티브 윈도우
    // Qt가 이 위젯에 절대 그리지 않도록 설정
    setAttribute(Qt::WA_NativeWindow);           // 네이티브 HWND 생성
    setAttribute(Qt::WA_PaintOnScreen);          // Qt 더블버퍼링 비활성화
    setAttribute(Qt::WA_NoSystemBackground);     // 시스템 배경 그리기 비활성화
    setAttribute(Qt::WA_OpaquePaintEvent);       // 불투명 이벤트 (배경 지우기 안 함)
    setAutoFillBackground(false);

    core_ = new MpvCore(this);

    // ── 소리누리 로고 오버레이 ────────────────────────────────────
    // 별도 네이티브 위젯으로 만들어서 MPV 위에 올림
    logoLabel_ = new QLabel(this);
    logoLabel_->setAttribute(Qt::WA_TransparentForMouseEvents);
    logoLabel_->setAttribute(Qt::WA_NoSystemBackground);
    logoLabel_->setStyleSheet("background: transparent; border: none;");
    logoLabel_->setAlignment(Qt::AlignCenter);

    QPixmap logo(":/sorinuri-logo-center.png");
    if (!logo.isNull()) {
        QPixmap scaled = logo.scaledToWidth(340, Qt::SmoothTransformation);
        logoLabel_->setPixmap(scaled);
        logoLabel_->resize(scaled.size());
    } else {
        logoLabel_->setText("소리누리");
        logoLabel_->setStyleSheet(
            "background: transparent; color: rgba(255,255,255,80);"
            "font-size: 36px; font-weight: 700; font-family: 'Malgun Gothic';");
        logoLabel_->adjustSize();
    }
    logoLabel_->show();
    logoLabel_->raise();
}

MpvWidget::~MpvWidget() = default;

// WA_PaintOnScreen 설정 시 paintEngine을 nullptr 반환해야 함
QPaintEngine* MpvWidget::paintEngine() const {
    return nullptr;
}

// paintEvent는 아무것도 하지 않음 - MPV가 직접 렌더링
void MpvWidget::paintEvent(QPaintEvent*) {
    // 의도적으로 비워둠: MPV가 이 창에 직접 렌더링
}

void MpvWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (!initialized_) initMpv();
    updateLogoPos();
}

void MpvWidget::initMpv() {
    // 위젯이 완전히 표시된 후 HWND를 가져와야 함
    QApplication::processEvents();

    WId wid = winId();
    qInfo() << "[MpvWidget] WID:" << wid;

#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(wid);
    // 창 배경을 검은색으로 설정
    SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)GetStockObject(BLACK_BRUSH));
    // 창을 검은색으로 초기화
    HDC hdc = GetDC(hwnd);
    RECT rc;
    GetClientRect(hwnd, &rc);
    HBRUSH black = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(hdc, &rc, black);
    DeleteObject(black);
    ReleaseDC(hwnd, hdc);
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
    updateLogoPos();
}

void MpvWidget::updateLogoPos() {
    if (!logoLabel_ || !logoLabel_->isVisible()) return;
    int x = (width()  - logoLabel_->width())  / 2;
    int y = (height() - logoLabel_->height()) / 2;
    logoLabel_->move(x, y);
    logoLabel_->raise();
}

void MpvWidget::showLogo(bool show) {
    if (!logoLabel_) return;
    logoLabel_->setVisible(show);
    if (show) { logoLabel_->raise(); updateLogoPos(); }
}

void MpvWidget::loadFile(const QString& path) {
    if (!initialized_) show();
    showLogo(false);
    core_->loadFile(path, false);
}

void MpvWidget::appendFile(const QString& path) {
    core_->loadFile(path, true);
}
