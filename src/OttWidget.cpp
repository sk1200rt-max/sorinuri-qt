#include "OttWidget.h"
#include <QDebug>
#include <QApplication>
#include <QScrollArea>
#include <QGridLayout>
#include <QFrame>

// ── 서비스 데이터 ──────────────────────────────────────────────────────────
const QStringList OttWidget::SERVICE_NAMES = {
    "서비스 선택...",
    "Netflix",
    "Disney+",
    "Amazon Prime",
    "YouTube",
    "Wavve (웨이브)",
    "Watcha (왓챠)",
    "Tving (티빙)",
    "Coupang Play",
    "Apple TV+",
};

const QStringList OttWidget::SERVICE_URLS = {
    "",
    "https://www.netflix.com",
    "https://www.disneyplus.com",
    "https://www.primevideo.com",
    "https://www.youtube.com",
    "https://www.wavve.com",
    "https://www.watcha.com",
    "https://www.tving.com",
    "https://www.coupangplay.com",
    "https://tv.apple.com",
};

// ServiceInfo는 OttWidget.h에 정의됨

static const QVector<ServiceInfo> SERVICES = {
    // name           url                              audio      audioBg    logo  logoColor  logoBg
    { "Netflix",      "https://www.netflix.com",      "Dolby Atmos", "#1565c0", "N",  "#e50914", "#1a0000" },
    { "Disney+",      "https://www.disneyplus.com",   "Dolby Atmos", "#1565c0", "D+", "#0063e5", "#00001a" },
    { "Prime Video",  "https://www.primevideo.com",   "Dolby Atmos", "#1565c0", "▶",  "#00a8e0", "#00101a" },
    { "YouTube",      "https://www.youtube.com",      "5.1 E-AC3",   "#1b5e20", "▶",  "#ff0000", "#1a0000" },
    { "Wavve 웨이브", "https://www.wavve.com",         "5.1",         "#1b5e20", "W",  "#00b4b4", "#001a1a" },
    { "Watcha 왓챠",  "https://www.watcha.com",        "5.1",         "#1b5e20", "W",  "#e53935", "#1a0000" },
    { "Tving 티빙",   "https://www.tving.com",         "5.1",         "#1b5e20", "T",  "#e53935", "#1a0000" },
    { "Coupang Play", "https://www.coupangplay.com",  "5.1",         "#1b5e20", "▶",  "#1565c0", "#00001a" },
    { "Apple TV+",    "https://tv.apple.com",         "Dolby Atmos", "#1565c0", "",   "#f5f5f5", "#111111" },
};

// ── 생성자 ─────────────────────────────────────────────────────────────────
OttWidget::OttWidget(QWidget* parent) : QWidget(parent) {
    setupUI();
}

OttWidget::~OttWidget() {
#ifdef Q_OS_WIN
#ifndef WEBVIEW2_NOT_AVAILABLE
    if (webCtrl_) { webCtrl_->Close(); webCtrl_->Release(); webCtrl_ = nullptr; }
    if (webView_) { webView_->Release(); webView_ = nullptr; }
    if (webEnv_)  { webEnv_->Release();  webEnv_  = nullptr; }
#endif
#endif
}

// ── UI 구성 ────────────────────────────────────────────────────────────────
void OttWidget::setupUI() {
    setStyleSheet("QWidget { background: #0e0e0e; color: #ccc; }");

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ── 툴바 ──────────────────────────────────────────────────────
    toolBar_ = new QWidget(this);
    toolBar_->setFixedHeight(40);
    toolBar_->setStyleSheet("background: #141414; border-bottom: 1px solid #222;");
    auto* tbLayout = new QHBoxLayout(toolBar_);
    tbLayout->setContentsMargins(8, 4, 8, 4);
    tbLayout->setSpacing(4);

    QString btnStyle =
        "QPushButton { background: #222; color: #ccc; border: 1px solid #333; "
        "border-radius: 4px; padding: 3px 10px; font-size: 13px; min-width: 30px; }"
        "QPushButton:hover { background: #2a2a2a; border-color: #4fc3f7; color: #fff; }"
        "QPushButton:disabled { color: #444; border-color: #222; }";

    backBtn_   = new QPushButton("◀", toolBar_);
    fwdBtn_    = new QPushButton("▶", toolBar_);
    reloadBtn_ = new QPushButton("↻", toolBar_);
    homeBtn_   = new QPushButton("⌂", toolBar_);
    backBtn_->setStyleSheet(btnStyle);
    fwdBtn_->setStyleSheet(btnStyle);
    reloadBtn_->setStyleSheet(btnStyle);
    homeBtn_->setStyleSheet(btnStyle);
    backBtn_->setToolTip("뒤로");
    fwdBtn_->setToolTip("앞으로");
    reloadBtn_->setToolTip("새로고침");
    homeBtn_->setToolTip("홈 (서비스 선택)");

    tbLayout->addWidget(backBtn_);
    tbLayout->addWidget(fwdBtn_);
    tbLayout->addWidget(reloadBtn_);
    tbLayout->addWidget(homeBtn_);

    serviceBox_ = new QComboBox(toolBar_);
    serviceBox_->addItems(SERVICE_NAMES);
    serviceBox_->setFixedWidth(130);
    serviceBox_->setStyleSheet(
        "QComboBox { background: #1a1a1a; color: #ddd; border: 1px solid #333; "
        "border-radius: 4px; padding: 3px 8px; font-size: 12px; }"
        "QComboBox:hover { border-color: #4fc3f7; }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView { background: #1a1a1a; color: #ddd; "
        "selection-background-color: #2a2a2a; border: 1px solid #333; }");
    tbLayout->addWidget(serviceBox_);

    urlBar_ = new QLineEdit(toolBar_);
    urlBar_->setPlaceholderText("URL 입력 또는 서비스 선택...");
    urlBar_->setStyleSheet(
        "QLineEdit { background: #111; color: #eee; border: 1px solid #333; "
        "border-radius: 4px; padding: 4px 10px; font-size: 12px; }"
        "QLineEdit:focus { border-color: #4fc3f7; }");
    tbLayout->addWidget(urlBar_, 1);

    auto* goBtn = new QPushButton("이동", toolBar_);
    goBtn->setStyleSheet(
        "QPushButton { background: #1565c0; color: #fff; border: none; "
        "border-radius: 4px; padding: 4px 14px; font-size: 12px; }"
        "QPushButton:hover { background: #1976d2; }");
    tbLayout->addWidget(goBtn);

    mainLayout->addWidget(toolBar_);

    // ── 콘텐츠 스택 ───────────────────────────────────────────────
    contentStack_ = new QStackedWidget(this);
    contentStack_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    homeGrid_ = buildHomeGrid();
    contentStack_->addWidget(homeGrid_);   // index 0

    webContainer_ = new QWidget(this);
    webContainer_->setStyleSheet("background: #000;");
    webContainer_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    statusLabel_ = new QLabel(webContainer_);
    statusLabel_->setAlignment(Qt::AlignCenter);
    statusLabel_->setWordWrap(true);
    statusLabel_->setStyleSheet(
        "QLabel { color: #888; font-size: 14px; background: transparent; padding: 40px; }");
#ifdef WEBVIEW2_NOT_AVAILABLE
    statusLabel_->setText(
        "이 빌드에는 WebView2 SDK가 포함되지 않았습니다.\n\n"
        "OTT 기능을 사용하려면 WebView2 SDK가 포함된 빌드가 필요합니다.");
#else
    statusLabel_->setText(
        "Edge WebView2 초기화 중...\n\n"
        "Dolby Atmos / 5.1 서라운드 완전 지원");
#endif
    contentStack_->addWidget(webContainer_); // index 1

    mainLayout->addWidget(contentStack_, 1);

    // ── 하단 상태 바 ─────────────────────────────────────────────
    auto* statusBar = new QWidget(this);
    statusBar->setFixedHeight(24);
    statusBar->setStyleSheet("background: #0a0a0a; border-top: 1px solid #1a1a1a;");
    auto* sbLayout = new QHBoxLayout(statusBar);
    sbLayout->setContentsMargins(12, 0, 12, 0);
    auto* statusText = new QLabel("Edge WebView2 준비 완료  ·  PlayReady DRM 활성", statusBar);
    statusText->setStyleSheet("color: #444; font-size: 10px; background: transparent;");
    sbLayout->addWidget(statusText);
    sbLayout->addStretch();
    mainLayout->addWidget(statusBar);

    connect(goBtn,      &QPushButton::clicked,    this, &OttWidget::onNavigateClicked);
    connect(urlBar_,    &QLineEdit::returnPressed, this, &OttWidget::onNavigateClicked);
    connect(backBtn_,   &QPushButton::clicked,    this, &OttWidget::goBack);
    connect(fwdBtn_,    &QPushButton::clicked,    this, &OttWidget::goForward);
    connect(reloadBtn_, &QPushButton::clicked,    this, &OttWidget::reload);
    connect(homeBtn_,   &QPushButton::clicked,    this, &OttWidget::goHome);
    connect(serviceBox_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &OttWidget::onServiceSelected);
}

// ── 홈 그리드 빌더 ─────────────────────────────────────────────────────────
QWidget* OttWidget::buildHomeGrid() {
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(
        "QScrollArea { background: #0e0e0e; border: none; }"
        "QScrollBar:vertical { background: #111; width: 6px; border-radius: 3px; }"
        "QScrollBar::handle:vertical { background: #333; border-radius: 3px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }");

    auto* container = new QWidget();
    container->setStyleSheet("background: #0e0e0e;");
    auto* gridLayout = new QGridLayout(container);
    gridLayout->setContentsMargins(28, 24, 28, 24);
    gridLayout->setSpacing(12);
    // 3열 균등 분배
    gridLayout->setColumnStretch(0, 1);
    gridLayout->setColumnStretch(1, 1);
    gridLayout->setColumnStretch(2, 1);

    for (int i = 0; i < SERVICES.size(); ++i) {
        auto* card = buildServiceCard(SERVICES[i]);
        gridLayout->addWidget(card, i / 3, i % 3);
    }

    scroll->setWidget(container);
    return scroll;
}

// ── 서비스 카드 빌더 ───────────────────────────────────────────────────────
QWidget* OttWidget::buildServiceCard(const ServiceInfo& svc) {
    // 카드 전체: QFrame (QPushButton 대신 클릭 감지용 이벤트 필터 사용)
    auto* card = new QFrame();
    card->setFixedHeight(90);
    card->setCursor(Qt::PointingHandCursor);
    card->setStyleSheet(
        "QFrame {"
        "  background: #1a1a1a;"
        "  border: 1px solid #2a2a2a;"
        "  border-radius: 10px;"
        "}"
        "QFrame:hover {"
        "  background: #222222;"
        "  border-color: #3d3d3d;"
        "}");

    auto* cardLayout = new QHBoxLayout(card);
    cardLayout->setContentsMargins(16, 0, 16, 0);
    cardLayout->setSpacing(16);

    // ── 로고 박스 (52×52 라운드 사각형) ─────────────────────────
    auto* logoBox = new QLabel();
    logoBox->setFixedSize(52, 52);
    logoBox->setAlignment(Qt::AlignCenter);
    logoBox->setStyleSheet(QString(
        "background: %1;"
        "border-radius: 10px;"
        "color: %2;"
        "font-size: 22px;"
        "font-weight: 900;"
        "font-family: 'Arial Black', 'Segoe UI Black', sans-serif;")
        .arg(svc.logoBg, svc.logoTextColor));
    logoBox->setText(svc.logoText);
    cardLayout->addWidget(logoBox);

    // ── 텍스트 영역 ───────────────────────────────────────────────
    auto* textBox  = new QWidget();
    auto* textLayout = new QVBoxLayout(textBox);
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(6);

    auto* nameLabel = new QLabel(svc.name);
    nameLabel->setStyleSheet(
        "color: #e0e0e0;"
        "font-size: 15px;"
        "font-weight: 600;"
        "font-family: 'Malgun Gothic', 'Segoe UI', sans-serif;"
        "background: transparent;");

    // 오디오 배지
    auto* badgeLabel = new QLabel(svc.audioLabel);
    badgeLabel->setFixedHeight(20);
    badgeLabel->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    badgeLabel->setStyleSheet(QString(
        "background: %1;"
        "color: #fff;"
        "font-size: 11px;"
        "font-weight: 700;"
        "font-family: 'Consolas', 'Courier New', monospace;"
        "padding: 1px 8px;"
        "border-radius: 4px;")
        .arg(svc.audioBg));

    textLayout->addStretch();
    textLayout->addWidget(nameLabel);
    textLayout->addWidget(badgeLabel);
    textLayout->addStretch();
    cardLayout->addWidget(textBox, 1);

    // 클릭 이벤트: 마우스 릴리즈로 감지
    QString url = svc.url;
    card->installEventFilter(this);
    card->setProperty("serviceUrl", url);

    return card;
}

// ── 이벤트 필터 (카드 클릭 감지) ──────────────────────────────────────────
bool OttWidget::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::MouseButtonRelease) {
        QString url = obj->property("serviceUrl").toString();
        if (!url.isEmpty()) {
            navigate(url);
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

// ── showEvent ─────────────────────────────────────────────────────────────
void OttWidget::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (!initAttempted_) {
        initAttempted_ = true;
        QTimer::singleShot(200, this, &OttWidget::initWebView2);
    }
}

// ── WebView2 초기화 ────────────────────────────────────────────────────────
void OttWidget::initWebView2() {
#ifdef Q_OS_WIN
#ifndef WEBVIEW2_NOT_AVAILABLE
    HWND parentHwnd = reinterpret_cast<HWND>(webContainer_->winId());

    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, nullptr, nullptr,
        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this, parentHwnd](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                if (FAILED(result) || !env) {
                    QMetaObject::invokeMethod(this, [this]() {
                        statusLabel_->setText(
                            "Microsoft Edge WebView2를 찾을 수 없습니다.\n\n"
                            "Edge 브라우저가 설치되어 있는지 확인해 주세요.");
                        emit webView2Unavailable();
                    }, Qt::QueuedConnection);
                    return S_OK;
                }
                webEnv_ = env;
                webEnv_->AddRef();
                env->CreateCoreWebView2Controller(
                    parentHwnd,
                    Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [this](HRESULT result, ICoreWebView2Controller* ctrl) -> HRESULT {
                            if (FAILED(result) || !ctrl) return S_OK;
                            webCtrl_ = ctrl;
                            webCtrl_->AddRef();
                            ctrl->get_CoreWebView2(&webView_);
                            if (webView_) {
                                ICoreWebView2Settings* settings = nullptr;
                                webView_->get_Settings(&settings);
                                if (settings) {
                                    settings->put_IsStatusBarEnabled(FALSE);
                                    settings->put_AreDefaultContextMenusEnabled(TRUE);
                                    settings->put_IsZoomControlEnabled(TRUE);
                                    settings->Release();
                                }
                                EventRegistrationToken token;
                                webView_->add_DocumentTitleChanged(
                                    Microsoft::WRL::Callback<ICoreWebView2DocumentTitleChangedEventHandler>(
                                        [this](ICoreWebView2* sender, IUnknown*) -> HRESULT {
                                            LPWSTR title = nullptr;
                                            sender->get_DocumentTitle(&title);
                                            if (title) {
                                                QString t = QString::fromWCharArray(title);
                                                CoTaskMemFree(title);
                                                QMetaObject::invokeMethod(this, [this, t]() {
                                                    emit titleChanged(t);
                                                }, Qt::QueuedConnection);
                                            }
                                            return S_OK;
                                        }).Get(), &token);
                                webView_->add_SourceChanged(
                                    Microsoft::WRL::Callback<ICoreWebView2SourceChangedEventHandler>(
                                        [this](ICoreWebView2* sender, ICoreWebView2SourceChangedEventArgs*) -> HRESULT {
                                            LPWSTR uri = nullptr;
                                            sender->get_Source(&uri);
                                            if (uri) {
                                                QString u = QString::fromWCharArray(uri);
                                                CoTaskMemFree(uri);
                                                QMetaObject::invokeMethod(this, [this, u]() {
                                                    urlBar_->setText(u);
                                                    emit urlChanged(u);
                                                }, Qt::QueuedConnection);
                                            }
                                            return S_OK;
                                        }).Get(), &token);
                            }
                            QMetaObject::invokeMethod(this, [this]() {
                                webView2Ready_ = true;
                                updateWebViewBounds();
                                qInfo() << "[WebView2] 초기화 완료 - PlayReady DRM 활성";
                            }, Qt::QueuedConnection);
                            return S_OK;
                        }).Get());
                return S_OK;
            }).Get());
    if (FAILED(hr)) qWarning() << "[WebView2] 초기화 실패:" << hr;
#endif
#else
    statusLabel_->setText("WebView2는 Windows에서만 지원됩니다.");
#endif
}

void OttWidget::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    if (statusLabel_) statusLabel_->resize(webContainer_->size());
    updateWebViewBounds();
}

void OttWidget::updateWebViewBounds() {
#ifdef Q_OS_WIN
#ifndef WEBVIEW2_NOT_AVAILABLE
    if (!webCtrl_ || !webContainer_) return;
    RECT bounds = {0, 0, webContainer_->width(), webContainer_->height()};
    webCtrl_->put_Bounds(bounds);
    webCtrl_->put_IsVisible(contentStack_->currentIndex() == 1 ? TRUE : FALSE);
#endif
#endif
}

void OttWidget::navigate(const QString& url) {
    if (url.isEmpty()) return;
    urlBar_->setText(url);
    if (contentStack_->currentIndex() == 0) {
        contentStack_->setCurrentIndex(1);
        if (statusLabel_) statusLabel_->show();
    }
#ifdef Q_OS_WIN
#ifndef WEBVIEW2_NOT_AVAILABLE
    if (webView_) {
        updateWebViewBounds();
        std::wstring wurl = url.toStdWString();
        webView_->Navigate(wurl.c_str());
        if (statusLabel_) statusLabel_->hide();
    } else {
        QTimer::singleShot(500, this, [this, url]() { navigate(url); });
    }
#endif
#endif
}

void OttWidget::onNavigateClicked() {
    QString url = urlBar_->text().trimmed();
    if (url.isEmpty()) return;
    if (!url.startsWith("http://") && !url.startsWith("https://"))
        url = "https://" + url;
    navigate(url);
}

void OttWidget::onServiceSelected(int index) {
    if (index <= 0 || index >= SERVICE_URLS.size()) return;
    navigate(SERVICE_URLS[index]);
    serviceBox_->setCurrentIndex(0);
}

void OttWidget::goBack() {
#ifdef Q_OS_WIN
#ifndef WEBVIEW2_NOT_AVAILABLE
    if (webView_) { BOOL can = FALSE; webView_->get_CanGoBack(&can); if (can) webView_->GoBack(); }
#endif
#endif
}

void OttWidget::goForward() {
#ifdef Q_OS_WIN
#ifndef WEBVIEW2_NOT_AVAILABLE
    if (webView_) { BOOL can = FALSE; webView_->get_CanGoForward(&can); if (can) webView_->GoForward(); }
#endif
#endif
}

void OttWidget::reload() {
#ifdef Q_OS_WIN
#ifndef WEBVIEW2_NOT_AVAILABLE
    if (webView_) webView_->Reload();
#endif
#endif
}

void OttWidget::goHome() {
    contentStack_->setCurrentIndex(0);
    urlBar_->clear();
    urlBar_->setPlaceholderText("URL 입력 또는 서비스 선택...");
#ifdef Q_OS_WIN
#ifndef WEBVIEW2_NOT_AVAILABLE
    if (webCtrl_) webCtrl_->put_IsVisible(FALSE);
#endif
#endif
}

bool OttWidget::isWebView2Available() const { return webView2Ready_; }
