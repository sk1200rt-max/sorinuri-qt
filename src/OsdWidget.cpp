#include "OsdWidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPropertyAnimation>

OsdWidget::OsdWidget(QWidget* parent) : QWidget(parent) {
    // 반투명 프레임리스 위젯, 마우스 이벤트 무시
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowTransparentForInput |
                   Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFixedSize(220, 64);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 10, 16, 10);
    layout->setSpacing(4);

    auto* row = new QHBoxLayout();
    row->setSpacing(10);

    iconLabel_ = new QLabel(this);
    iconLabel_->setStyleSheet("color: #fff; font-size: 20px; background: transparent;");
    iconLabel_->setFixedWidth(28);
    iconLabel_->setAlignment(Qt::AlignCenter);

    textLabel_ = new QLabel(this);
    textLabel_->setStyleSheet(
        "color: #fff; font-size: 15px; font-weight: 600;"
        "font-family: 'Segoe UI', 'Malgun Gothic', sans-serif;"
        "background: transparent;");
    textLabel_->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

    row->addWidget(iconLabel_);
    row->addWidget(textLabel_, 1);
    layout->addLayout(row);

    hideTimer_ = new QTimer(this);
    hideTimer_->setSingleShot(true);
    connect(hideTimer_, &QTimer::timeout, this, [this]() {
        // 페이드 아웃
        if (!fadeAnim_) {
            fadeAnim_ = new QPropertyAnimation(this, "opacity", this);
            fadeAnim_->setDuration(300);
            connect(fadeAnim_, &QPropertyAnimation::finished, this, &QWidget::hide);
        }
        fadeAnim_->stop();
        fadeAnim_->setStartValue(windowOpacity());
        fadeAnim_->setEndValue(0.0);
        fadeAnim_->start();
    });

    hide();
}

void OsdWidget::showVolume(int vol, bool muted) {
    currentType_ = Volume;
    if (muted) {
        iconLabel_->setText("🔇");
        textLabel_->setText("음소거");
        barValue_ = 0;
    } else {
        iconLabel_->setText(vol > 60 ? "🔊" : vol > 20 ? "🔉" : "🔈");
        textLabel_->setText(QString("볼륨  %1%").arg(vol));
        barValue_ = qMin(vol, 100);
    }
    showBar_ = true;
    showOsd(1600);
}

void OsdWidget::showSeek(double pos, double dur) {
    currentType_ = Seek;
    iconLabel_->setText("⏩");
    int t = qRound(pos);
    int h = t / 3600, m = (t % 3600) / 60, s = t % 60;
    QString timeStr = h > 0
        ? QString("%1:%2:%3").arg(h).arg(m,2,10,QChar('0')).arg(s,2,10,QChar('0'))
        : QString("%1:%2").arg(m,2,10,QChar('0')).arg(s,2,10,QChar('0'));
    textLabel_->setText(timeStr);
    barValue_ = dur > 0 ? qRound(pos / dur * 100) : 0;
    showBar_ = (dur > 0);
    showOsd(1200);
}

void OsdWidget::showSpeed(double speed) {
    currentType_ = Speed;
    iconLabel_->setText(speed > 1.0 ? "⏩" : speed < 1.0 ? "⏪" : "▶");
    textLabel_->setText(QString("속도  x%1").arg(speed, 0, 'f', 2));
    showBar_ = false;
    showOsd(1600);
}

void OsdWidget::showInfo(const QString& text, int durationMs) {
    currentType_ = Info;
    iconLabel_->setText("ℹ");
    textLabel_->setText(text);
    showBar_ = false;
    showOsd(durationMs);
}

void OsdWidget::showOsd(int durationMs) {
    // 페이드 아웃 중이면 중단
    if (fadeAnim_) fadeAnim_->stop();
    setWindowOpacity(0.88);

    // 부모 위젯 중앙 하단 1/3 위치에 배치
    if (parentWidget()) {
        QRect pr = parentWidget()->rect();
        int x = (pr.width() - width()) / 2;
        int y = pr.height() * 2 / 3 - height() / 2;
        move(x, y);
    }

    show();
    raise();
    hideTimer_->start(durationMs);
}

void OsdWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // 반투명 다크 배경 (둥근 모서리)
    QPainterPath bg;
    bg.addRoundedRect(rect(), 12, 12);
    p.fillPath(bg, QColor(0, 0, 0, 170));

    // 프로그레스 바 (하단)
    if (showBar_ && barValue_ >= 0) {
        int barH = 3;
        int barY = height() - barH - 4;
        int barW = width() - 24;
        int barX = 12;

        // 배경 트랙
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(60, 60, 60));
        p.drawRoundedRect(barX, barY, barW, barH, 1, 1);

        // 진행 부분
        int fillW = barW * barValue_ / 100;
        if (fillW > 0) {
            p.setBrush(QColor(79, 195, 247));  // #4fc3f7
            p.drawRoundedRect(barX, barY, fillW, barH, 1, 1);
        }
    }
}
