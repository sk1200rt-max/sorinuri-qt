#include "OttWidget.h"
#include <QDebug>
#include <QMessageBox>
#include <QApplication>

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
        "border-radius: 4px; padding: 3px 8px; font-size: 12px; min-width: 28px; }"
        "QPushButton:hover { background: #2a2a2a; border-color: #4fc3f7; color: #fff; }"
        "QPushButton:disabled { color: #444; }";

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
    homeBtn_->setToolTip("홈");

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
        "border-radius: 4px; padding: 4px 12px; font-size: 12px; }"
        "QPushButton:hover { background: #1976d2; }");
    tbLayout->addWidget(goBtn);

    mainLayout->addWidget(toolBar_);

    // ── WebView2 컨테이너 ─────────────────────────────────────────
    webContainer_ = new QWidget(this);
    webContainer_->setStyleSheet("background: #000;");
    webContainer_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    mainLayout->addWidget(webContainer_, 1);

    // ── 상태 레이블 ───────────────────────────────────────────────
    statusLabel_ = new QLabel(webContainer_);
    statusLabel_->setAlignment(Qt::AlignCenter);
    statusLabel_->setWordWrap(true);
    statusLabel_->setStyleSheet(
        "QLabel { color: #888; font-size: 14px; background: transparent; "
        "padding: 40px; }");

#ifdef WEBVIEW2_NOT_AVAILABLE
    statusLabel_->setText(
        "이 빌드에는 WebView2 SDK가 포함되지 않았습니다.\n\n"
        "OTT 기능을 사용하려면 WebView2 SDK가 포함된 빌드가 필요합니다.");
#else
    statusLabel_->setText(
        "Edge WebView2 초기화 중...\n\n"
        "Windows 10/11에 Microsoft Edge가 설치되어 있으면\n"
        "자동으로 넷플릭스, 디즈니+ 등 OTT 서비스를 이용할 수 있습니다.\n\n"
        "Dolby Atmos / 5.1 서라운드 완전 지원");
#endif

    statusLabel_->resize(webContainer_->size());

    connect(goBtn,      &QPushButton::clicked,   this, &OttWidget::onNavigateClicked);
    connect(urlBar_,    &QLineEdit::returnPressed, this, &OttWidget::onNavigateClicked);
    connect(backBtn_,   &QPushButton::clicked,   this, &OttWidget::goBack);
    connect(fwdBtn_,    &QPushButton::clicked,   this, &OttWidget::goForward);
    connect(reloadBtn_, &QPushButton::clicked,   this, &OttWidget::reload);
    connect(homeBtn_,   &QPushButton::clicked,   this, &OttWidget::goHome);
    connect(serviceBox_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &OttWidget::onServiceSelected);
}

void OttWidget::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (!initAttempted_) {
        initAttempted_ = true;
        QTimer::singleShot(100, this, &OttWidget::initWebView2);
    }
}

void OttWidget::initWebView2() {
#ifdef Q_OS_WIN
#ifndef WEBVIEW2_NOT_AVAILABLE
    HWND parentHwnd = reinterpret_cast<HWND>(webContainer_->winId());

    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, nullptr, nullptr,
        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this, parentHwnd](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                if (FAILED(result) || !env) {
                    qWarning() << "[WebView2] 환경 생성 실패:" << result;
                    QMetaObject::invokeMethod(this, [this]() {
                        statusLabel_->setText(
                            "Microsoft Edge WebView2를 찾을 수 없습니다.\n\n"
                            "Edge 브라우저가 설치되어 있는지 확인해 주세요.\n"
                            "Windows 10/11에는 기본 설치되어 있습니다.");
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
                            if (FAILED(result) || !ctrl) {
                                qWarning() << "[WebView2] 컨트롤러 생성 실패:" << result;
                                return S_OK;
                            }

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

                                // 타이틀 변경 이벤트
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

                                // URL 변경 이벤트
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
                                statusLabel_->hide();
                                updateWebViewBounds();
                                navigate("https://www.netflix.com");
                                qInfo() << "[WebView2] 초기화 완료 - PlayReady DRM 활성";
                            }, Qt::QueuedConnection);

                            return S_OK;
                        }).Get());

                return S_OK;
            }).Get());

    if (FAILED(hr)) {
        qWarning() << "[WebView2] 초기화 실패:" << hr;
        statusLabel_->setText(
            "WebView2 초기화 실패.\n\n"
            "Microsoft Edge가 설치되어 있는지 확인해 주세요.");
    }
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
    webCtrl_->put_IsVisible(TRUE);
#endif
#endif
}

void OttWidget::navigate(const QString& url) {
    if (url.isEmpty()) return;
    urlBar_->setText(url);
#ifdef Q_OS_WIN
#ifndef WEBVIEW2_NOT_AVAILABLE
    if (webView_) {
        std::wstring wurl = url.toStdWString();
        webView_->Navigate(wurl.c_str());
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
    if (webView_) {
        BOOL canGoBack = FALSE;
        webView_->get_CanGoBack(&canGoBack);
        if (canGoBack) webView_->GoBack();
    }
#endif
#endif
}

void OttWidget::goForward() {
#ifdef Q_OS_WIN
#ifndef WEBVIEW2_NOT_AVAILABLE
    if (webView_) {
        BOOL canGoFwd = FALSE;
        webView_->get_CanGoForward(&canGoFwd);
        if (canGoFwd) webView_->GoForward();
    }
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
    navigate("https://www.netflix.com");
}

bool OttWidget::isWebView2Available() const {
    return webView2Ready_;
}
