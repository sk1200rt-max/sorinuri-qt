#include "StoreUpdateManager.h"

#if defined(SORINURI_STORE_BUILD) && defined(Q_OS_WIN)

#include <windows.h>
#include <shobjidl_core.h>

#include <QDebug>
#include <QMetaObject>
#include <QPointer>
#include <QString>
#include <QWidget>

#include <winrt/Windows.Foundation.h>

namespace {

QString winrtMessage(const winrt::hresult_error& error)
{
    const auto message = error.message();
    return QString::fromWCharArray(message.c_str(), static_cast<int>(message.size()));
}

} // namespace

StoreUpdateManager::StoreUpdateManager(QObject* parent)
    : QObject(parent)
{
}

void StoreUpdateManager::checkForUpdates(QWidget* ownerWindow)
{
    if (updateOperationActive_ || !ownerWindow || !ownerWindow->isVisible()) {
        return;
    }

    // Store API는 UI thread에서 호출해야 하며, packaged desktop 앱은 Store가 띄우는
    // 다운로드/설치 권한 창의 owner HWND를 명시해야 한다.
    ownerWindow_ = ownerWindow;
    try {
        storeContext_ = winrt::Windows::Services::Store::StoreContext::GetDefault();
        configureOwnerWindow(ownerWindow);
        updateOperationActive_ = true;
        queryAvailableUpdates();
    } catch (const winrt::hresult_error& error) {
        qWarning() << "[StoreUpdate] Store update 확인을 시작하지 못함:" << winrtMessage(error);
    }
}

void StoreUpdateManager::configureOwnerWindow(QWidget* ownerWindow)
{
    auto initializeWithWindow = storeContext_.as<IInitializeWithWindow>();
    const HRESULT result = initializeWithWindow->Initialize(
        reinterpret_cast<HWND>(ownerWindow->winId()));
    if (FAILED(result)) {
        winrt::throw_hresult(result);
    }
}

winrt::fire_and_forget StoreUpdateManager::queryAvailableUpdates()
{
    const QPointer<StoreUpdateManager> self(this);
    const auto storeContext = storeContext_;
    try {
        const auto updates = co_await storeContext.GetAppAndOptionalStorePackageUpdatesAsync();
        if (!self || !self->ownerWindow_) {
            if (self) self->updateOperationActive_ = false;
            co_return;
        }

        if (updates.Size() == 0) {
            qInfo() << "[StoreUpdate] Microsoft Store 최신 버전 사용 중";
            self->updateOperationActive_ = false;
            co_return;
        }

        // 이 API는 Windows Store의 확인 창을 띄운다. 앱은 MSIX 파일을 직접 내려받거나
        // 현재 실행 중인 패키지를 교체하지 않으므로 Store 설치·서명·롤백 정책을 유지한다.
        // GetApp...의 coroutine 재개 스레드는 구현에 따라 달라질 수 있다. 설치 요청은
        // Microsoft 요구사항대로 Qt 메인(UI) 스레드에 명시적으로 되돌려 호출한다.
        QMetaObject::invokeMethod(self.data(), [self, updates]() {
            if (!self || !self->ownerWindow_) return;
            self->requestDownloadAndInstall(updates);
        }, Qt::QueuedConnection);
    } catch (const winrt::hresult_error& error) {
        qWarning() << "[StoreUpdate] Microsoft Store 업데이트 확인 실패:" << winrtMessage(error);
        if (self) self->updateOperationActive_ = false;
    }
}

winrt::fire_and_forget StoreUpdateManager::requestDownloadAndInstall(
    winrt::Windows::Foundation::Collections::IVectorView<
        winrt::Windows::Services::Store::StorePackageUpdate> updates)
{
    const QPointer<StoreUpdateManager> self(this);
    const auto storeContext = storeContext_;
    try {
        // Microsoft 문서상 UI thread 호출이 필수다. queryAvailableUpdates()가 Qt main
        // event loop로 되돌린 뒤 이 메서드를 시작하므로 Windows가 권한 UI를 소유한다.
        const auto result = co_await storeContext.RequestDownloadAndInstallStorePackageUpdatesAsync(updates);
        switch (result.OverallState()) {
        case winrt::Windows::Services::Store::StorePackageUpdateState::Completed:
            qInfo() << "[StoreUpdate] 업데이트 설치 완료; Windows가 앱을 재시작할 수 있음";
            break;
        case winrt::Windows::Services::Store::StorePackageUpdateState::Canceled:
            qInfo() << "[StoreUpdate] 사용자가 이번 업데이트를 나중에 적용하도록 선택";
            break;
        default:
            qWarning() << "[StoreUpdate] 업데이트 작업 상태:" << static_cast<int>(result.OverallState());
            break;
        }
    } catch (const winrt::hresult_error& error) {
        qWarning() << "[StoreUpdate] Microsoft Store 다운로드/설치 실패:" << winrtMessage(error);
    }

    if (self) self->updateOperationActive_ = false;
}

#endif
