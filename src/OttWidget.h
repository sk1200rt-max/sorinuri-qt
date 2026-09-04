#pragma once
#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QTimer>
#include <QResizeEvent>
#include <QStackedWidget>
#include <QEvent>

#ifdef Q_OS_WIN
#ifndef WEBVIEW2_NOT_AVAILABLE
#include <windows.h>
#include <wrl.h>
#include "WebView2.h"
#endif
#endif

// 서비스 정보 구조체 (OttWidget.cpp에서 정의)
struct ServiceInfo {
    QString name;
    QString url;
    QString audioLabel;
    QString audioBg;
    QString logoText;
    QString logoTextColor;
    QString logoBg;
};

/**
 * OttWidget - Edge WebView2 기반 OTT 스트리밍 패널
 *
 * 초기 화면: 서비스 선택 그리드 (목업 디자인 동일)
 * 서비스 카드 클릭 시: WebView2로 전환하여 해당 서비스 재생
 *
 * WebView2 Evergreen 런타임 → PlayReady DRM 완전 지원
 *   - Netflix Dolby Atmos / 4K
 *   - Disney+ Dolby Atmos
 *   - 국내 OTT (웨이브, 왓챠, 티빙 등)
 *   - YouTube 5.1 E-AC3
 */
class OttWidget : public QWidget {
    Q_OBJECT
public:
    explicit OttWidget(QWidget* parent = nullptr);
    ~OttWidget() override;

    void navigate(const QString& url);
    bool isWebView2Available() const;

public slots:
    void goBack();
    void goForward();
    void reload();
    void goHome();

signals:
    void titleChanged(const QString& title);
    void urlChanged(const QString& url);
    // 웹페이지 방문 기록과 무관하게 MainWindow의 일반 플레이어 화면으로 복귀한다.
    void returnToPlayerRequested();
    // OTT 화면에서도 기존 OriginalsWidget·통합 재생 대기열로 진입한다.
    void originalsRequested();
    void webView2Unavailable();

protected:
    void resizeEvent(QResizeEvent* e) override;
    void showEvent(QShowEvent* e) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onNavigateClicked();
    void onServiceSelected(int index);
    void initWebView2();

private:
    void setupUI();
    void updateWebViewBounds();

    // 홈 그리드 빌더
    QWidget* buildHomeGrid();
    QWidget* buildServiceCard(const ServiceInfo& svc);

    // ── UI 위젯 ──────────────────────────────────────────────────
    QWidget*        toolBar_       = nullptr;
    QLineEdit*      urlBar_        = nullptr;
    QPushButton*    backBtn_       = nullptr;
    QPushButton*    fwdBtn_        = nullptr;
    QPushButton*    reloadBtn_     = nullptr;
    QPushButton*    homeBtn_       = nullptr;
    QPushButton*    returnBtn_     = nullptr;  // 웹 탐색과 분리된 플레이어 복귀
    QPushButton*    originalsBtn_  = nullptr;  // 기존 OriginalsWidget 진입
    QComboBox*      serviceBox_    = nullptr;

    // 콘텐츠 스택: 0=홈 그리드, 1=WebView2
    QStackedWidget* contentStack_  = nullptr;
    QWidget*        homeGrid_      = nullptr;
    QWidget*        webContainer_  = nullptr;
    QLabel*         statusLabel_   = nullptr;

    bool webView2Ready_  = false;
    bool initAttempted_  = false;

#ifdef Q_OS_WIN
#ifndef WEBVIEW2_NOT_AVAILABLE
    ICoreWebView2Environment* webEnv_  = nullptr;
    ICoreWebView2Controller*  webCtrl_ = nullptr;
    ICoreWebView2*            webView_ = nullptr;
#endif
#endif

    static const QStringList SERVICE_NAMES;
    static const QStringList SERVICE_URLS;
};
