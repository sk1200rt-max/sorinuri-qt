#include "OttWidget.h"
#include <QDebug>
#include <QApplication>
#include <QScrollArea>
#include <QGridLayout>
#include <QFrame>
#include <QShortcut>
#include <QStyle>

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
    toolBar_->setFixedHeight(44);
    toolBar_->setStyleSheet("background: #101414;");
    auto* tbLayout = new QHBoxLayout(toolBar_);
    tbLayout->setContentsMargins(10, 6, 10, 6);
    tbLayout->setSpacing(4);

    const QString iconButtonStyle =
        "QPushButton { background: transparent; color: #a8b5b3; border: none; "
        "border-radius: 6px; padding: 4px; }"
        "QPushButton:hover { background: #202827; color: #f3fbfa; }"
        "QPushButton:disabled { color: #485452; }";

    // 서비스 전환은 앱 전체의 상단바에서만 수행한다. 이 행은 OTT 안에서 필요한
    // 뒤로·앞으로·새로고침·홈과 주소 입력만 제공해 중복 메뉴를 만들지 않는다.
    backBtn_   = new QPushButton(toolBar_);
    fwdBtn_    = new QPushButton(toolBar_);
    reloadBtn_ = new QPushButton(toolBar_);
    homeBtn_   = new QPushButton(toolBar_);
    backBtn_->setIcon(style()->standardIcon(QStyle::SP_ArrowBack));
    fwdBtn_->setIcon(style()->standardIcon(QStyle::SP_ArrowForward));
    reloadBtn_->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    homeBtn_->setIcon(style()->standardIcon(QStyle::SP_DirHomeIcon));
    for (QPushButton* button : {backBtn_, fwdBtn_, reloadBtn_, homeBtn_}) {
        button->setStyleSheet(iconButtonStyle);
        button->setFixedSize(30, 30);
        button->setIconSize(QSize(16, 16));
    }
    backBtn_->setFocusPolicy(Qt::NoFocus);
    fwdBtn_->setFocusPolicy(Qt::NoFocus);
    reloadBtn_->setFocusPolicy(Qt::NoFocus);
    homeBtn_->setFocusPolicy(Qt::NoFocus);
    backBtn_->setToolTip("웹페이지 뒤로가기");
    fwdBtn_->setToolTip("웹페이지 앞으로가기");
    reloadBtn_->setToolTip("웹페이지 새로고침");
    homeBtn_->setToolTip("OTT 서비스 선택으로 돌아가기");

    tbLayout->addWidget(backBtn_);
    tbLayout->addWidget(fwdBtn_);
    tbLayout->addWidget(reloadBtn_);
    tbLayout->addWidget(homeBtn_);

    serviceBox_ = new QComboBox(toolBar_);
    serviceBox_->addItems(SERVICE_NAMES);
    serviceBox_->setFixedWidth(130);
    serviceBox_->setStyleSheet(
        "QComboBox { background: #1a2120; color: #dbe6e4; border: none; "
        "border-radius: 6px; padding: 4px 8px; font-size: 12px; }"
        "QComboBox:hover { background: #222c2b; }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView { background: #171d1c; color: #dbe6e4; "
        "selection-background-color: #263331; border: none; }");
    tbLayout->addWidget(serviceBox_);

    urlBar_ = new QLineEdit(toolBar_);
    urlBar_->setPlaceholderText("URL 입력 또는 서비스 선택...");
    urlBar_->setStyleSheet(
        "QLineEdit { background: #0c1010; color: #e8f1ef; border: none; "
        "border-radius: 6px; padding: 5px 10px; font-size: 12px; }"
        "QLineEdit:focus { background: #111918; outline: none; }");
    tbLayout->addWidget(urlBar_, 1);

    auto* goBtn = new QPushButton("이동", toolBar_);
    goBtn->setStyleSheet(
        "QPushButton { background: #00a98f; color: #08110f; border: none; "
        "border-radius: 6px; padding: 5px 13px; font-size: 12px; font-weight: 700; }"
        "QPushButton:hover { background: #00c4a7; }");
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

    goBtn->setFocusPolicy(Qt::NoFocus);
    connect(goBtn,         &QPushButton::clicked,    this, &OttWidget::onNavigateClicked);
    connect(urlBar_,     &QLineEdit::returnPressed, this, &OttWidget::onNavigateClicked);
    connect(backBtn_,   &QPushButton::clicked,    this, &OttWidget::goBack);
    connect(fwdBtn_,    &QPushButton::clicked,    this, &OttWidget::goForward);
    connect(reloadBtn_, &QPushButton::clicked,    this, &OttWidget::reload);
    connect(homeBtn_,   &QPushButton::clicked,    this, &OttWidget::goHome);
    connect(serviceBox_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &OttWidget::onServiceSelected);

    // 네이티브 WebView2에 포커스가 있어도 창 단위에서 동작하도록 앱 단축키로 등록한다.
    auto* returnShortcut = new QShortcut(QKeySequence("Ctrl+Shift+P"), this);
    returnShortcut->setContext(Qt::ApplicationShortcut);
    connect(returnShortcut, &QShortcut::activated, this, &OttWidget::returnToPlayerRequested);
}

// ── 홈 그리드 빌더 ─────────────────────────────────────────────────────────
QWidget* OttWidget::buildHomeGrid() {
    // OTT는 단순 링크 모음이 아니라, 감상할 서비스를 고르는 독립 탐색 화면이다.
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(
        "QScrollArea{background:#0A1011;border:none;}"
        "QScrollBar:vertical{background:transparent;width:7px;}"
        "QScrollBar::handle:vertical{background:#2A403D;border-radius:3px;}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}");

    auto* container = new QWidget(scroll);
    container->setStyleSheet("background:#0A1011;");
    auto* pageLayout = new QVBoxLayout(container);
    pageLayout->setContentsMargins(42, 34, 42, 44);
    pageLayout->setSpacing(24);

    auto* heading = new QWidget(container);
    auto* headingLayout = new QVBoxLayout(heading);
    headingLayout->setContentsMargins(0, 0, 0, 0);
    headingLayout->setSpacing(5);
    auto* eyebrow = new QLabel(QStringLiteral("OTT & STREAMING  ·  CINEMA AT HOME"), heading);
    eyebrow->setStyleSheet("color:#00D4B4;font-size:10px;font-weight:800;letter-spacing:1.5px;background:transparent;");
    auto* title = new QLabel(QStringLiteral("오늘은 무엇을 볼까요?"), heading);
    title->setStyleSheet("color:#F1F7F5;font-size:28px;font-weight:750;letter-spacing:-0.4px;background:transparent;");
    auto* description = new QLabel(QStringLiteral("즐겨 찾는 스트리밍 서비스를 소리누리 안에서 바로 이어서 감상하세요."), heading);
    description->setStyleSheet("color:#8BA09C;font-size:12px;background:transparent;");
    headingLayout->addWidget(eyebrow);
    headingLayout->addWidget(title);
    headingLayout->addWidget(description);
    pageLayout->addWidget(heading);

    auto* feature = new QFrame(container);
    feature->setObjectName("ottFeatureStage");
    feature->setFixedHeight(164);
    feature->setStyleSheet(
        "QFrame#ottFeatureStage{background:qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #163330,stop:.55 #10201F,stop:1 #0F1718);border:1px solid #284440;border-radius:14px;}"
        "QLabel{background:transparent;}");
    auto* featureLayout = new QHBoxLayout(feature);
    featureLayout->setContentsMargins(28, 24, 28, 24);
    auto* featureText = new QVBoxLayout();
    featureText->setSpacing(5);
    auto* featureEyebrow = new QLabel(QStringLiteral("빠른 시작"), feature);
    featureEyebrow->setStyleSheet("color:#79D6C6;font-size:10px;font-weight:800;letter-spacing:1.2px;");
    auto* featureTitle = new QLabel(QStringLiteral("YouTube에서 이어서 감상하기"), feature);
    featureTitle->setStyleSheet("color:#F4FBF9;font-size:21px;font-weight:750;");
    auto* featureBody = new QLabel(QStringLiteral("음악, 라이브, 채널 콘텐츠를 소리누리의 재생 환경에서 바로 엽니다."), feature);
    featureBody->setStyleSheet("color:#B4C8C4;font-size:12px;");
    auto* featureButton = new QPushButton(QStringLiteral("YouTube 열기"), feature);
    featureButton->setFocusPolicy(Qt::NoFocus);
    featureButton->setCursor(Qt::PointingHandCursor);
    featureButton->setFixedSize(112, 34);
    featureButton->setStyleSheet(
        "QPushButton{background:#00C7AA;color:#05231E;border:1px solid #68E3D2;border-radius:8px;font-size:11px;font-weight:800;}"
        "QPushButton:hover{background:#45E0C8;}");
    connect(featureButton, &QPushButton::clicked, this, [this]() { navigate(QStringLiteral("https://www.youtube.com")); });
    featureText->addWidget(featureEyebrow);
    featureText->addWidget(featureTitle);
    featureText->addWidget(featureBody);
    featureText->addStretch(1);
    featureText->addWidget(featureButton, 0, Qt::AlignLeft);
    featureLayout->addLayout(featureText, 1);
    auto* featureMark = new QLabel(QStringLiteral("OTT"), feature);
    featureMark->setAlignment(Qt::AlignCenter);
    featureMark->setFixedSize(88, 88);
    featureMark->setStyleSheet("color:#9DE9DB;background:#0B1919;border:1px solid #38645D;border-radius:44px;font-size:18px;font-weight:800;letter-spacing:1px;");
    featureLayout->addWidget(featureMark);
    pageLayout->addWidget(feature);

    auto* serviceHeader = new QHBoxLayout();
    auto* serviceTitle = new QLabel(QStringLiteral("서비스 선택"), container);
    serviceTitle->setStyleSheet("color:#EAF3F1;font-size:16px;font-weight:750;background:transparent;");
    auto* serviceHint = new QLabel(QStringLiteral("서비스를 선택하면 웹 플레이어가 열립니다"), container);
    serviceHint->setStyleSheet("color:#718681;font-size:11px;background:transparent;");
    serviceHeader->addWidget(serviceTitle);
    serviceHeader->addStretch(1);
    serviceHeader->addWidget(serviceHint);
    pageLayout->addLayout(serviceHeader);

    auto* gridHost = new QWidget(container);
    gridHost->setStyleSheet("background:transparent;");
    auto* gridLayout = new QGridLayout(gridHost);
    gridLayout->setContentsMargins(0, 0, 0, 0);
    gridLayout->setHorizontalSpacing(12);
    gridLayout->setVerticalSpacing(12);
    for (int column = 0; column < 3; ++column) gridLayout->setColumnStretch(column, 1);
    for (int i = 0; i < SERVICES.size(); ++i) gridLayout->addWidget(buildServiceCard(SERVICES[i]), i / 3, i % 3);
    pageLayout->addWidget(gridHost);
    pageLayout->addStretch(1);

    scroll->setWidget(container);
    return scroll;
}

// ── 서비스 카드 빌더 ───────────────────────────────────────────────────────
QWidget* OttWidget::buildServiceCard(const ServiceInfo& svc) {
    // 카드 외곽선을 없애고, 배경 명도와 여백만으로 클릭 영역을 구분한다.
    auto* card = new QFrame();
    card->setFixedHeight(82);
    card->setCursor(Qt::PointingHandCursor);
    card->setStyleSheet(
        "QFrame { background: #151b1a; border: none; border-radius: 8px; }"
        "QFrame:hover { background: #1d2826; }");

    auto* cardLayout = new QHBoxLayout(card);
    cardLayout->setContentsMargins(16, 0, 14, 0);
    cardLayout->setSpacing(12);

    // 서비스별 색은 작은 식별자에만 사용해 화면 전체가 과하게 산만해지지 않게 한다.
    auto* logoBox = new QLabel(card);
    logoBox->setFixedSize(42, 42);
    logoBox->setAlignment(Qt::AlignCenter);
    logoBox->setStyleSheet(QString(
        "background: %1; border: none; border-radius: 8px; color: %2; "
        "font-size: 17px; font-weight: 800; font-family: 'Segoe UI', sans-serif;")
        .arg(svc.logoBg, svc.logoTextColor));
    logoBox->setText(svc.logoText);
    cardLayout->addWidget(logoBox);

    auto* nameLabel = new QLabel(svc.name, card);
    nameLabel->setStyleSheet(
        "color: #edf4f2; font-size: 15px; font-weight: 600; "
        "font-family: 'Malgun Gothic', 'Segoe UI', sans-serif; background: transparent;");
    cardLayout->addWidget(nameLabel, 1);

    auto* openHint = new QLabel("열기", card);
    openHint->setStyleSheet(
        "color: #00b89c; font-size: 12px; font-weight: 600; background: transparent;");
    cardLayout->addWidget(openHint);

    // 클릭 이벤트: 마우스 릴리즈로 감지
    card->installEventFilter(this);
    card->setProperty("serviceUrl", svc.url);
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
