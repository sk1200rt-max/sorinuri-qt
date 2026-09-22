#pragma once

#include <QObject>
#include <QPointer>

#if defined(SORINURI_STORE_BUILD) && defined(Q_OS_WIN)
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Services.Store.h>

class QWidget;

// Microsoft Store 배포본 전용 업데이트 경로.
// Store 밖의 EXE/BIN updater를 실행하지 않고, Store가 제공하는 검증된 패키지로만
// 확인·다운로드·설치를 요청한다. 실제 설치 권한과 재시작은 Windows UI가 처리한다.
class StoreUpdateManager final : public QObject {
    Q_OBJECT

public:
    explicit StoreUpdateManager(QObject* parent = nullptr);

    // 반드시 표시된 Qt UI 스레드에서 호출한다. Store API의 확인 빈도 제한은
    // Microsoft가 관리하며, API가 최신 패키지 상태를 반환한다.
    void checkForUpdates(QWidget* ownerWindow);

private:
    void configureOwnerWindow(QWidget* ownerWindow);
    winrt::fire_and_forget queryAvailableUpdates();
    winrt::fire_and_forget requestDownloadAndInstall(
        winrt::Windows::Foundation::Collections::IVectorView<
            winrt::Windows::Services::Store::StorePackageUpdate> updates);

    QPointer<QWidget> ownerWindow_;
    winrt::Windows::Services::Store::StoreContext storeContext_{nullptr};
    bool updateOperationActive_ = false;
};
#endif
