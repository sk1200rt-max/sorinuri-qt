#include "MpvWidget.h"
#include <QShowEvent>
#include <QResizeEvent>
#include <QDebug>
#include <QPixmap>

MpvWidget::MpvWidget(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setStyleSheet("background: #000000;");

    core_ = new MpvCore(this);

    // ── 소리누리 로고 오버레이 ────────────────────────────────────
    logoOverlay_ = new QLabel(this);
    logoOverlay_->setAlignment(Qt::AlignCenter);
    logoOverlay_->setAttribute(Qt::WA_TransparentForMouseEvents);
    logoOverlay_->setStyleSheet("background: transparent;");

    // 로고 이미지 로드 (2560x1440 원본에서 중앙 로고 부분만 사용)
    QPixmap logo(":/sorinuri-logo.png");
    if (!logo.isNull()) {
        // 원본이 2560x1440이고 로고가 왼쪽에 있음 - 로고 영역만 크롭
        // 로고는 약 x:100~650, y:280~540 위치
        QPixmap cropped = logo.copy(80, 260, 580, 280);
        // 적당한 크기로 리사이즈 (너비 320px)
        QPixmap scaled = cropped.scaled(320, 155, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        logoOverlay_->setPixmap(scaled);
    } else {
        // 로고 없으면 텍스트로 대체
        logoOverlay_->setText("소리누리\nSORINURI");
        logoOverlay_->setStyleSheet(
            "background: transparent; color: rgba(255,255,255,120);"
            "font-size: 32px; font-weight: 700;"
            "font-family: 'Malgun Gothic', 'Segoe UI', sans-serif;");
    }

    logoOverlay_->show();
    logoOverlay_->raise();
}

MpvWidget::~MpvWidget() = default;

void MpvWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (!initialized_) initMpv();
    updateLogoPosition();
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
    updateLogoPosition();
}

void MpvWidget::updateLogoPosition() {
    if (!logoOverlay_) return;
    // 중앙에 배치
    int x = (width()  - logoOverlay_->width())  / 2;
    int y = (height() - logoOverlay_->height()) / 2;
    logoOverlay_->move(x, y);
    logoOverlay_->raise();
}

void MpvWidget::showLogo(bool show) {
    if (logoOverlay_) {
        logoOverlay_->setVisible(show);
        if (show) { logoOverlay_->raise(); updateLogoPosition(); }
    }
}

void MpvWidget::loadFile(const QString& path) {
    if (!initialized_) show();
    showLogo(false);  // 재생 시작하면 로고 숨김
    core_->loadFile(path, false);
}

void MpvWidget::appendFile(const QString& path) {
    core_->loadFile(path, true);
}
