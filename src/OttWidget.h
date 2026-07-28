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

#ifdef Q_OS_WIN
#ifndef WEBVIEW2_NOT_AVAILABLE
#include <windows.h>
#include <wrl.h>
#include "WebView2.h"
#endif
#endif

/**
 * OttWidget - Edge WebView2 기반 OTT 스트리밍 패널
 *
 * Microsoft Edge WebView2 (Evergreen Runtime)를 Qt 위젯에 임베드합니다.
 * WebView2 Evergreen 런타임은 PlayReady DRM을 완전 지원하므로:
 *   - 넷플릭스 Dolby Atmos / 4K 재생 가능
 *   - 디즈니+ Dolby Atmos 재생 가능
 *   - 국내 OTT (웨이브, 왓챠, 티빙 등) 재생 가능
 *   - 유튜브 5.1 E-AC3 재생 가능
 *
 * WebView2 SDK가 없으면 WEBVIEW2_NOT_AVAILABLE이 정의되어
 * 안내 메시지만 표시하고 빌드는 정상 완료됩니다.
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
    void webView2Unavailable();

protected:
    void resizeEvent(QResizeEvent* e) override;
    void showEvent(QShowEvent* e) override;

private slots:
    void onNavigateClicked();
    void onServiceSelected(int index);
    void initWebView2();

private:
    void setupUI();
    void updateWebViewBounds();

    // UI 위젯
    QWidget*     toolBar_      = nullptr;
    QLineEdit*   urlBar_       = nullptr;
    QPushButton* backBtn_      = nullptr;
    QPushButton* fwdBtn_       = nullptr;
    QPushButton* reloadBtn_    = nullptr;
    QPushButton* homeBtn_      = nullptr;
    QComboBox*   serviceBox_   = nullptr;
    QWidget*     webContainer_ = nullptr;
    QLabel*      statusLabel_  = nullptr;

    bool webView2Ready_  = false;
    bool initAttempted_  = false;

#ifdef Q_OS_WIN
#ifndef WEBVIEW2_NOT_AVAILABLE
    // WebView2 COM 인터페이스 (WIL 없이 순수 COM 포인터)
    ICoreWebView2Environment* webEnv_  = nullptr;
    ICoreWebView2Controller*  webCtrl_ = nullptr;
    ICoreWebView2*            webView_ = nullptr;
#endif
#endif

    static const QStringList SERVICE_NAMES;
    static const QStringList SERVICE_URLS;
};
