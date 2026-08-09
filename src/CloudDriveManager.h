#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTcpServer>

/**
 * CloudDriveManager - OneDrive / Google Drive 연동
 *
 * - OAuth 2.0 인증 흐름 (로컬 콜백 서버 방식)
 * - Microsoft Graph API / Google Drive API로 파일 목록 조회
 * - 미디어 파일의 임시 다운로드 URL 추출 → MPV 스트리밍
 */
struct CloudFile {
    QString id;
    QString name;
    QString mimeType;
    QString downloadUrl;
    qint64  size = 0;
    bool    isFolder = false;
};

class CloudDriveManager : public QObject {
    Q_OBJECT
public:
    enum class Provider { OneDrive, GoogleDrive };

    explicit CloudDriveManager(QObject* parent = nullptr);
    ~CloudDriveManager() override;

    bool isAuthenticated(Provider p) const;
    void authenticate(Provider p);
    void logout(Provider p);
    void listFiles(Provider p, const QString& folderId = {});
    void getDownloadUrl(Provider p, const CloudFile& file);

signals:
    void authRequired(const QString& url);
    void authenticated(CloudDriveManager::Provider provider);
    void filesListed(const QList<CloudFile>& files);
    void downloadUrlReady(const QString& url, const QString& fileName);
    void error(const QString& msg);

private slots:
    void onCallbackReceived();

private:
    void startCallbackServer();
    void stopCallbackServer();
    void exchangeCodeForToken(Provider p, const QString& code);
    void listOneDriveFiles(const QString& folderId);
    void listGoogleDriveFiles(const QString& folderId);

    QNetworkAccessManager* nam_     = nullptr;
    QTcpServer*            server_  = nullptr;

    // OneDrive
    QString oneDriveToken_;
    QString oneDriveRefreshToken_;
    static const QString ONEDRIVE_CLIENT_ID;
    static const QString ONEDRIVE_SCOPE;

    // Google Drive
    QString googleToken_;
    QString googleRefreshToken_;
    static const QString GOOGLE_CLIENT_ID;
    static const QString GOOGLE_SCOPE;

    Provider pendingProvider_ = Provider::OneDrive;
    int      callbackPort_    = 9876;
};
