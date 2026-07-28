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
#include <windows.h>
#include <wrl.h>
#include <wil/com.h>
#include "WebView2.h"
#endif

/**
 * OttWidget - Edge WebView2 기반 OTT 스트리밍 패널
 *
 * Microsoft Edge WebView2 (Evergreen Runtime)를 Qt 위젯에 임베드합니다.
 * WebView2는 시스템에 설치된 Edge 런타임을 사용하므로:
 *   - PlayReady DRM 완전 지원 → 넷플릭스 4K, Dolby Atmos
 *   - Widevine L1 (하드웨어 보안) → 고화질 스트리밍
 *   - 오디오는 Windows 오디오 서브시스템 직접 사용
 *
 * 지원 서비스:
 *   - 넷플릭스 (Netflix) - Dolby Atmos, 4K
 *   - 디즈니+ (Disney+) - Dolby Atmos
 *   - 아마존 프라임 (Amazon Prime Video)
 *   - 웨이브 (Wavve), 왓챠 (Watcha) 등 국내 OTT
 *   - 유튜브 (YouTube) - 5.1 E-AC3
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
    QWidget*     toolBar_    = nullptr;
    QLineEdit*   urlBar_     = nullptr;
    QPushButton* backBtn_    = nullptr;
    QPushButton* fwdBtn_     = nullptr;
    QPushButton* reloadBtn_  = nullptr;
    QPushButton* homeBtn_    = nullptr;
    QComboBox*   serviceBox_ = nullptr;
    QWidget*     webContainer_ = nullptr;
    QLabel*      statusLabel_  = nullptr;

    bool webView2Ready_ = false;
    bool initAttempted_ = false;

#ifdef Q_OS_WIN
    // WebView2 COM 인터페이스
    wil::com_ptr<ICoreWebView2Environment> webEnv_;
    wil::com_ptr<ICoreWebView2Controller>  webCtrl_;
    wil::com_ptr<ICoreWebView2>            webView_;
    HWND                                   webHwnd_ = nullptr;
#endif

    // OTT 서비스 목록
    static const QStringList SERVICE_NAMES;
    static const QStringList SERVICE_URLS;
};
