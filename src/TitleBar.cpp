#include "TitleBar.h"
#include <QApplication>

TitleBar::TitleBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(40);
    setStyleSheet("QWidget { background: #0f0f0f; border-bottom: 1px solid #1c1c1c; }");

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 0, 0, 0);
    layout->setSpacing(0);

    // ── 로고 ──────────────────────────────────────────────────────
    logoLabel_ = new QLabel("소리누리", this);
    logoLabel_->setStyleSheet(
        "color:#fff; font-size:13px; font-weight:600;"
        "font-family:'Malgun Gothic','Segoe UI',sans-serif;"
        "letter-spacing:1px; background:transparent; border:none; padding-right:10px;");
    layout->addWidget(logoLabel_);

    // ── 오디오 포맷 배지 ──────────────────────────────────────────
    badgeLabel_ = new QLabel(this);
    badgeLabel_->setFixedHeight(20);
    badgeLabel_->setStyleSheet(
        "background:#1a3a5c; color:#4fc3f7; font-size:10px; font-weight:700;"
        "font-family:'Consolas','Courier New',monospace;"
        "padding:2px 7px; border-radius:3px; border:none;");
    badgeLabel_->hide();
    layout->addWidget(badgeLabel_);
    layout->addSpacing(8);

    // ── 파일명 ────────────────────────────────────────────────────
    titleLabel_ = new QLabel(this);
    titleLabel_->setStyleSheet(
        "color:#777; font-size:12px;"
        "font-family:'Segoe UI','Malgun Gothic',sans-serif;"
        "background:transparent; border:none;");
    layout->addWidget(titleLabel_);
    layout->addStretch();

    // ── 창 컨트롤 버튼 공통 스타일 ───────────────────────────────
    auto makeBtn = [&](const QString& text, const QString& tooltip,
                       const QString& hoverBg) -> QPushButton* {
        auto* btn = new QPushButton(text, this);
        btn->setToolTip(tooltip);
        btn->setFixedSize(46, 40);
        btn->setFlat(true);
        btn->setCursor(Qt::ArrowCursor);
        btn->setStyleSheet(QString(
            "QPushButton{background:transparent;color:#999;font-size:13px;border:none;border-radius:0;}"
            "QPushButton:hover{background:%1;color:#fff;}").arg(hoverBg));
        return btn;
    };

    btnMin_        = makeBtn("─",  "최소화",     "#2a2a2a");
    btnMax_        = makeBtn("□",  "화면 채우기", "#2a2a2a");
    btnFullscreen_ = makeBtn("⛶", "전체화면",   "#1e3a5f");
    btnClose_      = new QPushButton("✕", this);
    btnClose_->setToolTip("닫기");
    btnClose_->setFixedSize(48, 40);
    btnClose_->setFlat(true);
    btnClose_->setCursor(Qt::ArrowCursor);
    btnClose_->setStyleSheet(
        "QPushButton{background:transparent;color:#999;font-size:13px;border:none;border-radius:0;}"
        "QPushButton:hover{background:#c42b1c;color:#fff;}");

    layout->addWidget(btnMin_);
    layout->addWidget(btnMax_);
    layout->addWidget(btnFullscreen_);
    layout->addWidget(btnClose_);

    connect(btnMin_,        &QPushButton::clicked, this, &TitleBar::minimizeClicked);
    connect(btnMax_,        &QPushButton::clicked, this, &TitleBar::maximizeClicked);
    connect(btnFullscreen_, &QPushButton::clicked, this, &TitleBar::fullscreenClicked);
    connect(btnClose_,      &QPushButton::clicked, this, &TitleBar::closeClicked);
}

void TitleBar::setTitle(const QString& title) {
    int sep = title.lastIndexOf(" — ");
    titleLabel_->setText(sep >= 0 ? title.mid(sep + 3) : QString());
}

void TitleBar::setAudioBadge(const QString& codec) {
    if (codec.isEmpty()) { badgeLabel_->hide(); return; }
    QString d = codec.toUpper();
    if      (d.contains("TRUEHD") && d.contains("ATMOS")) d = "TrueHD Atmos";
    else if (d.contains("TRUEHD"))                         d = "TrueHD";
    else if (d.contains("EAC3")   && d.contains("ATMOS")) d = "DD+ Atmos";
    else if (d.contains("EAC3"))                           d = "DD+";
    else if (d.contains("AC3"))                            d = "DD";
    else if (d.contains("DTS-HD"))                         d = "DTS-HD MA";
    else if (d.contains("DTS"))                            d = "DTS";
    badgeLabel_->setText(d);
    badgeLabel_->show();
}

void TitleBar::setFullscreenMode(bool fs) {
    btnFullscreen_->setText(fs ? "⊡" : "⛶");
    btnFullscreen_->setToolTip(fs ? "전체화면 종료" : "전체화면");
}

void TitleBar::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        dragging_ = true;
        dragStart_ = e->globalPosition().toPoint() - window()->pos();
    }
}

void TitleBar::mouseMoveEvent(QMouseEvent* e) {
    if (dragging_ && (e->buttons() & Qt::LeftButton))
        window()->move(e->globalPosition().toPoint() - dragStart_);
}

void TitleBar::mouseReleaseEvent(QMouseEvent*) { dragging_ = false; }

void TitleBar::mouseDoubleClickEvent(QMouseEvent*) { emit maximizeClicked(); }
