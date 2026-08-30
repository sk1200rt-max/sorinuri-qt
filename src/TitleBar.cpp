#include "TitleBar.h"
#include "UiTheme.h"
#include <QIcon>
#include <QPixmap>
#include <QWindow>

QPushButton* TitleBar::makeIconBtn(const QString& svgPath, const QString& tooltip,
                                    const QString& hoverBg, int w) {
    auto* btn = new QPushButton();
    btn->setToolTip(tooltip);
    btn->setFixedSize(w, 36);
    btn->setFlat(true);
    btn->setCursor(Qt::ArrowCursor);
    btn->setFocusPolicy(Qt::NoFocus);  // HiDPI: 버튼 클릭 후 포커스가 MainWindow에 유지되도록
    btn->setIcon(QIcon(svgPath));
    btn->setIconSize(QSize(16, 16));
    btn->setStyleSheet(QString(
        "QPushButton { background: transparent; border: none; border-radius: 0; }"
        "QPushButton:hover { background: %1; }").arg(hoverBg));
    return btn;
}

TitleBar::TitleBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(40);
    setStyleSheet(QString("background: %1; border-bottom: 1px solid %2;")
                  .arg(SorinuriUi::Surface, SorinuriUi::BorderSoft));

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 0, 0, 0);
    layout->setSpacing(0);

    // ── 소리누리 로고 아이콘 ──────────────────────────────────────
    auto* logoIcon = new QLabel(this);
    logoIcon->setFixedSize(26, 26);
    logoIcon->setStyleSheet("background: transparent; border: none;");
    QPixmap logoPixmap(":/sorinuri-app.png");
    if (!logoPixmap.isNull()) {
        logoIcon->setPixmap(logoPixmap.scaled(26, 26,
            Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    layout->addWidget(logoIcon);
    layout->addSpacing(4);

    // 오디오 포맷 배지
    badgeLabel_ = new QLabel(this);
    badgeLabel_->setFixedHeight(18);
    badgeLabel_->setStyleSheet(SorinuriUi::statusBadgeStyle(SorinuriUi::Mint));
    badgeLabel_->hide();
    layout->addWidget(badgeLabel_);
    layout->addSpacing(8);

    // 파일명
    titleLabel_ = new QLabel(this);
    titleLabel_->setStyleSheet(
        "color: #8C9A99; font-size: 12px; font-family: 'Segoe UI', sans-serif;"
        "background: transparent; border: none;");
    layout->addWidget(titleLabel_);
    layout->addStretch();

    // ── 항상 위에 고정 버튼 (핀) ─────────────────────────────────
    btnPin_ = new QPushButton(this);
    btnPin_->setToolTip("항상 위에 고정");
    btnPin_->setFixedSize(38, 36);
    btnPin_->setFlat(true);
    btnPin_->setCursor(Qt::ArrowCursor);
    btnPin_->setFocusPolicy(Qt::NoFocus);  // HiDPI: 버튼 클릭 후 포커스가 MainWindow에 유지되도록
    btnPin_->setCheckable(true);
    btnPin_->setIcon(QIcon(":/icons/pin_off.svg"));
    btnPin_->setIconSize(QSize(18, 18));
    btnPin_->setStyleSheet(
        "QPushButton {"
        "  background: transparent; border: none; border-radius: 0;"
        "  color: #555;"
        "}"
        "QPushButton:hover {"
        "  background: #1A2526;"
        "}"
        "QPushButton:checked {"
        "  background: #063B35;"
        "  border-bottom: 2px solid #00D4B4;"
        "}"
        "QPushButton:checked:hover {"
        "  background: #0B564B;"
        "}");

    // ── 창 버튼 ───────────────────────────────────────────────────
    btnMin_        = makeIconBtn(":/icons/minimize.svg",   "최소화",     "#1A2526");
    btnMax_        = makeIconBtn(":/icons/maximize.svg",   "화면 채우기", "#1A2526");
    btnFullscreen_ = makeIconBtn(":/icons/expand.svg",     "전체화면",   "#063B35");
    btnClose_      = makeIconBtn(":/icons/close.svg",      "닫기",       "#D94D45", 48);

    layout->addWidget(btnPin_);
    layout->addWidget(btnMin_);
    layout->addWidget(btnMax_);
    layout->addWidget(btnFullscreen_);
    layout->addWidget(btnClose_);

    // 핀 버튼 토글 시 아이콘 및 툴팁 변경
    connect(btnPin_, &QPushButton::toggled, this, [this](bool checked) {
        pinned_ = checked;
        btnPin_->setIcon(QIcon(checked ? ":/icons/pin.svg" : ":/icons/pin_off.svg"));
        btnPin_->setToolTip(checked ? "항상 위에 고정 해제" : "항상 위에 고정");
        emit alwaysOnTopToggled(checked);
    });

    connect(btnMin_,        &QPushButton::clicked, this, &TitleBar::minimizeClicked);
    connect(btnMax_,        &QPushButton::clicked, this, &TitleBar::maximizeClicked);
    connect(btnFullscreen_, &QPushButton::clicked, this, &TitleBar::fullscreenClicked);
    connect(btnClose_,      &QPushButton::clicked, this, &TitleBar::closeClicked);
}

void TitleBar::setAlwaysOnTop(bool pinned) {
    // 외부에서 상태 설정 (시그널 발생 없이)
    btnPin_->blockSignals(true);
    btnPin_->setChecked(pinned);
    btnPin_->blockSignals(false);
    pinned_ = pinned;
    btnPin_->setIcon(QIcon(pinned ? ":/icons/pin.svg" : ":/icons/pin_off.svg"));
    btnPin_->setToolTip(pinned ? "항상 위에 고정 해제" : "항상 위에 고정");
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
    btnFullscreen_->setIcon(QIcon(fs ? ":/icons/fullscreen_exit.svg" : ":/icons/expand.svg"));
    btnFullscreen_->setToolTip(fs ? "전체화면 종료" : "전체화면");
    btnMax_->setIcon(QIcon(fs ? ":/icons/restore.svg" : ":/icons/maximize.svg"));
}

void TitleBar::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        // 좌표를 직접 옮기면 Windows가 비클라이언트 드래그를 보지 못해
        // 화면 가장자리 스냅·상단 최대화가 동작하지 않는다. 지원 플랫폼에서는
        // 시스템 드래그로 넘기고, 지원하지 않는 경우에만 기존 좌표 이동을 사용한다.
        if (window() && window()->windowHandle() &&
            window()->windowHandle()->startSystemMove()) {
            e->accept();
            return;
        }
        dragging_  = true;
        dragStart_ = e->globalPosition().toPoint() - window()->pos();
        e->accept();
        return;
    }
    QWidget::mousePressEvent(e);
}

void TitleBar::mouseMoveEvent(QMouseEvent* e) {
    if (dragging_ && (e->buttons() & Qt::LeftButton)) {
        window()->move(e->globalPosition().toPoint() - dragStart_);
        e->accept();
        return;
    }
    QWidget::mouseMoveEvent(e);
}

void TitleBar::mouseReleaseEvent(QMouseEvent* e) {
    dragging_ = false;
    QWidget::mouseReleaseEvent(e);
}
void TitleBar::mouseDoubleClickEvent(QMouseEvent*) { emit maximizeClicked(); }
