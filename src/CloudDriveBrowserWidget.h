#pragma once
#include <QWidget>
#include <QList>
#include "CloudDriveManager.h"

class QPushButton;
class QLabel;
class QListWidget;
class QTabWidget;
class QStackedWidget;
class QProgressBar;

/**
 * CloudDriveBrowserWidget - OneDrive / Google Drive 파일 탐색기 UI
 *
 * - OneDrive / Google Drive 탭 분리
 * - 인증 상태 표시 및 로그인/로그아웃 버튼
 * - 미디어 파일 목록 (아이콘 + 파일명 + 크기)
 * - 더블클릭으로 즉시 스트리밍 재생
 */
class CloudDriveBrowserWidget : public QWidget {
    Q_OBJECT
public:
    explicit CloudDriveBrowserWidget(CloudDriveManager* manager, QWidget* parent = nullptr);

signals:
    void fileRequested(const QString& url, const QString& name);

private slots:
    void onOneDriveAuth();
    void onGoogleAuth();
    void onOneDriveRefresh();
    void onGoogleRefresh();
    void onFilesListed(const QList<CloudFile>& files);
    void onAuthenticated(CloudDriveManager::Provider provider);
    void onDownloadUrlReady(const QString& url, const QString& name);
    void onError(const QString& msg);

private:
    void buildUI();
    void buildProviderTab(QWidget* page, CloudDriveManager::Provider provider);
    void updateAuthStatus();

    CloudDriveManager* manager_     = nullptr;
    QTabWidget*        tabs_        = nullptr;

    // OneDrive 탭
    QPushButton*  btnOneDriveAuth_    = nullptr;
    QPushButton*  btnOneDriveRefresh_ = nullptr;
    QLabel*       lblOneDriveStatus_  = nullptr;
    QListWidget*  oneDriveList_       = nullptr;

    // Google Drive 탭
    QPushButton*  btnGoogleAuth_      = nullptr;
    QPushButton*  btnGoogleRefresh_   = nullptr;
    QLabel*       lblGoogleStatus_    = nullptr;
    QListWidget*  googleList_         = nullptr;

    QProgressBar* progressBar_        = nullptr;
    QLabel*       statusLabel_        = nullptr;

    CloudDriveManager::Provider currentProvider_ = CloudDriveManager::Provider::OneDrive;
};
