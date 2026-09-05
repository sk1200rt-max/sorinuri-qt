#include "TitleBar.h"
#include "UiTheme.h"
#include <QGridLayout>
#include <QIcon>
#include <QPixmap>
#include <QStyle>
#include <QWindow>

QPushButton* TitleBar::makeIconBtn(const QString& svgPath, const QString& tooltip,
                                    const QString& hoverBg, int w) {
    auto* btn = new QPushButton();
    btn->setToolTip(tooltip);
    btn->setFixedSize(w, 40);
    btn->setFlat(true);
    btn->setCursor(Qt::ArrowCursor);
    btn->setFocusPolicy(Qt::NoFocus);
    btn->setIcon(QIcon(svgPath));
    btn->setIconSize(QSize(16, 16));
    btn->setStyleSheet(QString(
        "QPushButton { background: transparent; border: 1px solid transparent; border-radius: 7px; }"
        "QPushButton:hover { background: %1; border-color: %2; }"
        "QPushButton:pressed { background: %3; }")
        .arg(hoverBg, SorinuriUi::Border, SorinuriUi::SurfacePress));
    return btn;
}

QPushButton* TitleBar::makeCommandBtn(const QString& text, const QString& tooltip) {
    auto* btn = new QPushButton(text);
    btn->setToolTip(tooltip);
    btn->setFixedHeight(32);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFocusPolicy(Qt::NoFocus);
    btn->setStyleSheet(QString(
        "QPushButton { color: %1; background: transparent; border: 1px solid transparent;"
        " border-radius: 7px; padding: 0 9px; font-size: 11px; font-weight: 650; }"
        "QPushButton:hover { color: %2; background: %3; border-color: %4; }"
        "QPushButton:pressed { background: %5; }")
        .arg(SorinuriUi::TextMuted, SorinuriUi::Text, SorinuriUi::SurfaceHover,
             SorinuriUi::Border, SorinuriUi::SurfacePress));
    return btn;
}

QPushButton* TitleBar::makeServiceBtn(const QString& text, const QString& tooltip) {
    auto* btn = new QPushButton(text);
    btn->setToolTip(tooltip);
    btn->setFixedHeight(50);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFocusPolicy(Qt::NoFocus);
    btn->setStyleSheet(QString(
        "QPushButton { color: %1; background: transparent; border: none; border-bottom: 2px solid transparent;"
        " padding: 0 18px; font-size: 12px; font-weight: 700; }"
        "QPushButton:hover { color: %2; background: %3; }"
        "QPushButton[active=true] { color: %2; border-bottom-color: %4; }")
        .arg(SorinuriUi::TextDim, SorinuriUi::Text, SorinuriUi::SurfaceHover, SorinuriUi::Mint));
    return btn;
}

TitleBar::TitleBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(52);
    setStyleSheet(QString("background: %1; border-bottom: 1px solid %2;")
                  .arg(SorinuriUi::Surface, SorinuriUi::BorderSoft));

    // 좌·중앙·우측을 동일한 신축 열로 배치해 서비스 전환이 창 폭의 정중앙을 유지한다.
    auto* grid = new QGridLayout(this);
    grid->setContentsMargins(14, 0, 8, 0);
    grid->setHorizontalSpacing(0);
    grid->setVerticalSpacing(0);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(2, 1);

    auto* identity = new QWidget(this);
    auto* identityLayout = new QHBoxLayout(identity);
    identityLayout->setContentsMargins(0, 0, 0, 0);
    identityLayout->setSpacing(7);

    auto* logoIcon = new QLabel(identity);
    logoIcon->setFixedSize(26, 26);
    logoIcon->setStyleSheet("background: transparent; border: none;");
    QPixmap logoPixmap(":/sorinuri-app.png");
    if (!logoPixmap.isNull()) {
        logoIcon->setPixmap(logoPixmap.scaled(26, 26,
            Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    identityLayout->addWidget(logoIcon);

    auto* wordmark = new QLabel(QStringLiteral("SORINURI"), identity);
    wordmark->setStyleSheet(
        "color: #F2F7F6; font-size: 13px; font-weight: 800; letter-spacing: 1.4px;"
        "font-family: 'Segoe UI', 'Malgun Gothic', sans-serif; background: transparent; border: none;");
    identityLayout->addWidget(wordmark);
    identityLayout->addSpacing(10);

    badgeLabel_ = new QLabel(identity);
    badgeLabel_->setFixedHeight(18);
    badgeLabel_->setStyleSheet(SorinuriUi::statusBadgeStyle(SorinuriUi::Mint));
    badgeLabel_->hide();
    identityLayout->addWidget(badgeLabel_);

    titleLabel_ = new QLabel(identity);
    titleLabel_->setMaximumWidth(260);
    titleLabel_->setStyleSheet(
        "color: #A3B1B0; font-size: 11px; font-weight: 600;"
        "font-family: 'Segoe UI', 'Malgun Gothic', sans-serif; background: transparent; border: none;");
    identityLayout->addWidget(titleLabel_);
    identityLayout->addStretch(1);

    auto* services = new QWidget(this);
    auto* serviceLayout = new QHBoxLayout(services);
    serviceLayout->setContentsMargins(0, 0, 0, 0);
    serviceLayout->setSpacing(2);
    btnPlayer_ = makeServiceBtn(QStringLiteral("플레이어"), QStringLiteral("현재 재생 화면"));
    btnOtt_ = makeServiceBtn(QStringLiteral("OTT"), QStringLiteral("OTT 및 웹 스트리밍"));
    btnOriginals_ = makeServiceBtn(QStringLiteral("오리지널"), QStringLiteral("소리누리 오리지널 음악"));
    serviceLayout->addWidget(btnPlayer_);
    serviceLayout->addWidget(btnOtt_);
    serviceLayout->addWidget(btnOriginals_);

    auto* commands = new QWidget(this);
    auto* commandLayout = new QHBoxLayout(commands);
    commandLayout->setContentsMargins(0, 0, 0, 0);
    commandLayout->setSpacing(2);
    btnOpen_ = makeCommandBtn(QStringLiteral("파일 열기"), QStringLiteral("파일 열기 (Ctrl+O)"));
    btnOpen_->setIcon(QIcon(":/icons/open.svg"));
    btnOpen_->setIconSize(QSize(15, 15));
    btnTools_ = makeCommandBtn(QStringLiteral("도구  ▾"), QStringLiteral("플레이어 도구 및 환경 설정"));
    btnPin_ = makeIconBtn(":/icons/pin_off.svg", "항상 위에 고정", "#1A2526", 36);
    btnPin_->setCheckable(true);
    // 표준 창 제어는 모두 같은 정사각형 클릭 영역을 사용한다. 닫기 버튼만
    // 더 좁거나 세로로 길게 보이지 않게 해 250% HiDPI에서도 균일하게 유지한다.
    constexpr int kWindowControlSide = 40;
    btnMin_ = makeIconBtn(":/icons/minimize.svg", "최소화", "#1A2526", kWindowControlSide);
    btnMax_ = makeIconBtn(":/icons/maximize.svg", "화면 채우기", "#1A2526", kWindowControlSide);
    btnFullscreen_ = makeIconBtn(":/icons/expand.svg", "전체화면", "#063B35", kWindowControlSide);
    btnClose_ = makeIconBtn(":/icons/close.svg", "닫기", SorinuriUi::Danger, kWindowControlSide);

    commandLayout->addWidget(btnOpen_);
    commandLayout->addWidget(btnTools_);
    commandLayout->addSpacing(5);
    commandLayout->addWidget(btnPin_);
    commandLayout->addWidget(btnMin_);
    commandLayout->addWidget(btnMax_);
    commandLayout->addWidget(btnFullscreen_);
    commandLayout->addWidget(btnClose_);

    grid->addWidget(identity, 0, 0, Qt::AlignLeft | Qt::AlignVCenter);
    grid->addWidget(services, 0, 1, Qt::AlignHCenter | Qt::AlignVCenter);
    grid->addWidget(commands, 0, 2, Qt::AlignRight | Qt::AlignVCenter);

    connect(btnPin_, &QPushButton::toggled, this, [this](bool checked) {
        pinned_ = checked;
        btnPin_->setIcon(QIcon(checked ? ":/icons/pin.svg" : ":/icons/pin_off.svg"));
        btnPin_->setToolTip(checked ? "항상 위에 고정 해제" : "항상 위에 고정");
        emit alwaysOnTopToggled(checked);
    });
    connect(btnPlayer_, &QPushButton::clicked, this, &TitleBar::playerServiceClicked);
    connect(btnOtt_, &QPushButton::clicked, this, &TitleBar::ottServiceClicked);
    connect(btnOriginals_, &QPushButton::clicked, this, &TitleBar::originalsServiceClicked);
    connect(btnOpen_, &QPushButton::clicked, this, &TitleBar::openFileClicked);
    connect(btnTools_, &QPushButton::clicked, this, &TitleBar::toolsClicked);
    connect(btnMin_, &QPushButton::clicked, this, &TitleBar::minimizeClicked);
    connect(btnMax_, &QPushButton::clicked, this, &TitleBar::maximizeClicked);
    connect(btnFullscreen_, &QPushButton::clicked, this, &TitleBar::fullscreenClicked);
    connect(btnClose_, &QPushButton::clicked, this, &TitleBar::closeClicked);

    refreshServiceButtons();
    updateResponsiveLayout();
}

void TitleBar::updateResponsiveLayout() {
    // 250% HiDPI의 낮은 논리 폭에서는 창 제어·서비스 전환을 보존하고,
    // 반복 정보(코덱 배지·파일명)와 항상 위 고정만 먼저 접어 잘림을 방지한다.
    const bool compact = width() < 900;
    compactLayout_ = compact;
    if (badgeLabel_) badgeLabel_->setVisible(!compact && !badgeLabel_->text().isEmpty());
    if (titleLabel_) titleLabel_->setVisible(!compact);
    if (btnPin_) btnPin_->setVisible(!compact);
    // 좁은 논리 폭에서는 창 최소화·최대화·전체화면을 도구 메뉴로 이동한다.
    // 닫기 버튼은 항상 남겨 기본 창 동작을 즉시 수행할 수 있게 한다.
    if (btnMin_) btnMin_->setVisible(!compact);
    if (btnMax_) btnMax_->setVisible(!compact);
    if (btnFullscreen_) btnFullscreen_->setVisible(!compact);
    if (btnClose_) btnClose_->show();
    // 파일·도구는 아이콘형으로 압축하고, 숨긴 창 제어는 도구 메뉴에서 제공한다.
    if (btnOpen_) {
        btnOpen_->setVisible(!compact);
        if (!compact) { btnOpen_->setMinimumWidth(0); btnOpen_->setMaximumWidth(QWIDGETSIZE_MAX); }
    }
    if (btnTools_) {
        btnTools_->setText(compact ? QStringLiteral("⋮") : QStringLiteral("도구  ▾"));
        if (compact) btnTools_->setFixedWidth(36);
        else { btnTools_->setMinimumWidth(0); btnTools_->setMaximumWidth(QWIDGETSIZE_MAX); }
    }
    if (btnClose_) {
        btnClose_->setText(compact ? QStringLiteral("×") : QString());
        btnClose_->setIcon(compact ? QIcon() : QIcon(":/icons/close.svg"));
        if (compact) {
            btnClose_->setFixedWidth(40);
            btnClose_->setStyleSheet("QPushButton { color: #F2F7F6; font-size: 20px; background: transparent; border: none; } QPushButton:hover { background: #C42B35; }");
        } else {
            btnClose_->setMinimumWidth(0);
            btnClose_->setMaximumWidth(QWIDGETSIZE_MAX);
            btnClose_->setStyleSheet("QPushButton { background: transparent; border: 1px solid transparent; border-radius: 7px; } QPushButton:hover { background: #C42B35; border-color: #263A3C; } QPushButton:pressed { background: #102425; }");
        }
    }

    const QString serviceStyle = compact
        ? "QPushButton { color: #A3B1B0; background: transparent; border: none; border-bottom: 2px solid transparent; padding: 0 11px; font-size: 12px; font-weight: 700; }"
          "QPushButton:hover { color: #F2F7F6; background: #172425; }"
          "QPushButton[active=true] { color: #F2F7F6; border-bottom-color: #00D4B4; }"
        : "QPushButton { color: #A3B1B0; background: transparent; border: none; border-bottom: 2px solid transparent; padding: 0 18px; font-size: 12px; font-weight: 700; }"
          "QPushButton:hover { color: #F2F7F6; background: #172425; }"
          "QPushButton[active=true] { color: #F2F7F6; border-bottom-color: #00D4B4; }";
    for (QPushButton* button : {btnPlayer_, btnOtt_, btnOriginals_}) {
        if (!button) continue;
        button->setStyleSheet(serviceStyle);
    }
}

void TitleBar::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    updateResponsiveLayout();
}

void TitleBar::refreshServiceButtons() {
    const QList<QPair<QPushButton*, Service>> buttons = {
        {btnPlayer_, Service::Player}, {btnOtt_, Service::Ott}, {btnOriginals_, Service::Originals}
    };
    for (const auto& item : buttons) {
        QPushButton* button = item.first;
        if (!button) continue;
        button->setProperty("active", item.second == activeService_);
        button->style()->unpolish(button);
        button->style()->polish(button);
    }
}

void TitleBar::setActiveService(Service service) {
    if (activeService_ == service) return;
    activeService_ = service;
    refreshServiceButtons();
}

bool TitleBar::isInteractiveControlAt(const QPoint& localPos) const {
    const QWidget* const controls[] = {
        btnPlayer_, btnOtt_, btnOriginals_, btnOpen_, btnTools_,
        btnPin_, btnMin_, btnMax_, btnFullscreen_, btnClose_
    };
    for (const QWidget* control : controls) {
        if (control && control->isVisible() && control->rect().contains(control->mapFrom(this, localPos)))
            return true;
    }
    return false;
}

void TitleBar::setAlwaysOnTop(bool pinned) {
    btnPin_->blockSignals(true);
    btnPin_->setChecked(pinned);
    btnPin_->blockSignals(false);
    pinned_ = pinned;
    btnPin_->setIcon(QIcon(pinned ? ":/icons/pin.svg" : ":/icons/pin_off.svg"));
    btnPin_->setToolTip(pinned ? "항상 위에 고정 해제" : "항상 위에 고정");
}

void TitleBar::setTitle(const QString& title) {
    // 창 제목 형식이 '소리누리 — 파일명'이 아니어도 현재 곡·파일명을 잃지 않는다.
    const int sep = title.lastIndexOf(" — ");
    const QString displayTitle = (sep >= 0 ? title.mid(sep + 3) : title).trimmed();
    const QFontMetrics metrics(titleLabel_->font());
    titleLabel_->setText(metrics.elidedText(displayTitle, Qt::ElideRight, 250));
    titleLabel_->setToolTip(displayTitle);
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
    if (!compactLayout_) badgeLabel_->show();
}

void TitleBar::setFullscreenMode(bool fs) {
    btnFullscreen_->setIcon(QIcon(fs ? ":/icons/fullscreen_exit.svg" : ":/icons/expand.svg"));
    btnFullscreen_->setToolTip(fs ? "전체화면 종료" : "전체화면");
    btnMax_->setIcon(QIcon(fs ? ":/icons/restore.svg" : ":/icons/maximize.svg"));
}

void TitleBar::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        if (window() && window()->windowHandle() && window()->windowHandle()->startSystemMove()) {
            e->accept();
            return;
        }
        dragging_ = true;
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
