#include "TitleBar.h"
#include <QApplication>
#include <QScreen>

TitleBar::TitleBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(38);
    setStyleSheet(R"(
        QWidget {
            background: #0f0f0f;
            border-bottom: 1px solid #1e1e1e;
        }
        QPushButton {
            background: transparent;
            border: none;
            color: #888;
            font-size: 14px;
            padding: 0 12px;
            min-width: 40px;
            min-height: 38px;
        }
        QPushButton:hover { background: #2a2a2a; color: #fff; }
        QPushButton#btnClose:hover { background: #c42b1c; color: #fff; }
    )");

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 0, 0, 0);
    layout->setSpacing(4);

    // 로고
    logoLabel_ = new QLabel("소리누리", this);
    logoLabel_->setStyleSheet("color: #fff; font-weight: bold; font-size: 14px; padding: 0 4px;");
    layout->addWidget(logoLabel_);

    // 오디오 포맷 배지
    badgeLabel_ = new QLabel(this);
    badgeLabel_->setStyleSheet(R"(
        background: #1a3a5c;
        color: #4fc3f7;
        font-size: 10px;
        font-weight: bold;
        padding: 2px 6px;
        border-radius: 3px;
    )");
    badgeLabel_->hide();
    layout->addWidget(badgeLabel_);

    // 타이틀
    titleLabel_ = new QLabel(this);
    titleLabel_->setStyleSheet("color: #888; font-size: 12px;");
    titleLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    layout->addWidget(titleLabel_, 1);

    // 창 컨트롤 버튼
    btnMin_ = new QPushButton("─", this);
    btnMax_ = new QPushButton("□", this);
    btnClose_ = new QPushButton("✕", this);
    btnClose_->setObjectName("btnClose");

    layout->addWidget(btnMin_);
    layout->addWidget(btnMax_);
    layout->addWidget(btnClose_);

    connect(btnMin_,   &QPushButton::clicked, this, &TitleBar::minimizeClicked);
    connect(btnMax_,   &QPushButton::clicked, this, &TitleBar::maximizeClicked);
    connect(btnClose_, &QPushButton::clicked, this, &TitleBar::closeClicked);
}

void TitleBar::setTitle(const QString& title) {
    // 파일명만 표시
    int sep = title.lastIndexOf(" — ");
    if (sep >= 0)
        titleLabel_->setText(title.mid(sep + 3));
    else
        titleLabel_->setText({});
}

void TitleBar::setAudioBadge(const QString& codec) {
    if (codec.isEmpty()) {
        badgeLabel_->hide();
    } else {
        badgeLabel_->setText(codec);
        badgeLabel_->show();
    }
}

void TitleBar::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        dragging_ = true;
        dragStart_ = e->globalPosition().toPoint() - window()->pos();
    }
}

void TitleBar::mouseMoveEvent(QMouseEvent* e) {
    if (dragging_ && (e->buttons() & Qt::LeftButton)) {
        window()->move(e->globalPosition().toPoint() - dragStart_);
    }
}

void TitleBar::mouseReleaseEvent(QMouseEvent* e) {
    Q_UNUSED(e);
    dragging_ = false;
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent* e) {
    Q_UNUSED(e);
    emit maximizeClicked();
}
