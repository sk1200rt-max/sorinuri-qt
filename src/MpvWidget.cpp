#include "MpvWidget.h"
#include <QShowEvent>
#include <QResizeEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPixmap>
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

MpvWidget::MpvWidget(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_NativeWindow);
    setAutoFillBackground(true);
    QPalette pal;
    pal.setColor(QPalette::Window, Qt::black);
    setPalette(pal);

    core_ = new MpvCore(this);

    // ── 소리누리 로고 오버레이 ────────────────────────────────────
    logoLabel_ = new QLabel(this);
    logoLabel_->setAttribute(Qt::WA_TransparentForMouseEvents);
    logoLabel_->setStyleSheet("background: transparent; border: none;");
    logoLabel_->setAlignment(Qt::AlignCenter);

    QPixmap logo(":/sorinuri-logo-center.png");
    if (!logo.isNull()) {
        // 화면 너비의 약 30% 크기로 표시
        QPixmap scaled = logo.scaledToWidth(340, Qt::SmoothTransformation);
        logoLabel_->setPixmap(scaled);
        logoLabel_->resize(scaled.size());
    } else {
        // 폴백: 텍스트
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

void MpvWidget::paintEvent(QPaintEvent* e) {
    QPainter p(this);
    p.fillRect(e->rect(), Qt::black);
}

QPaintEngine* MpvWidget::paintEngine() const {
    return QWidget::paintEngine();
}

void MpvWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (!initialized_) initMpv();
    updateLogoPos();
}

void MpvWidget::initMpv() {
    WId wid = winId();
    qInfo() << "[MpvWidget] WID:" << wid;

#ifdef Q_OS_WIN
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
