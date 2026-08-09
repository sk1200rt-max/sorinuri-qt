#include "CloudDriveManager.h"
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDesktopServices>
#include <QUrl>
#include <QTcpSocket>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSettings>

// OneDrive (Microsoft Graph API) - 공개 클라이언트 앱 ID
const QString CloudDriveManager::ONEDRIVE_CLIENT_ID = "d3590ed6-52b3-4102-aeff-aad2292ab01c";
const QString CloudDriveManager::ONEDRIVE_SCOPE     = "Files.Read offline_access";

// Google Drive - 공개 OAuth 클라이언트 ID
const QString CloudDriveManager::GOOGLE_CLIENT_ID   = "sorinuri-gdrive-client";
const QString CloudDriveManager::GOOGLE_SCOPE       = "https://www.googleapis.com/auth/drive.readonly";

CloudDriveManager::CloudDriveManager(QObject* parent) : QObject(parent) {
    nam_ = new QNetworkAccessManager(this);

    // 저장된 토큰 복원
    QSettings s("GaonCommunication", "Sorinuri");
    oneDriveToken_        = s.value("cloud/onedrive_token").toString();
    oneDriveRefreshToken_ = s.value("cloud/onedrive_refresh").toString();
    googleToken_          = s.value("cloud/google_token").toString();
    googleRefreshToken_   = s.value("cloud/google_refresh").toString();
}

CloudDriveManager::~CloudDriveManager() {
    stopCallbackServer();
}

bool CloudDriveManager::isAuthenticated(Provider p) const {
    return p == Provider::OneDrive ? !oneDriveToken_.isEmpty() : !googleToken_.isEmpty();
}

void CloudDriveManager::authenticate(Provider p) {
    pendingProvider_ = p;
    startCallbackServer();

    QString authUrl;
    QString redirectUri = QString("http://localhost:%1/callback").arg(callbackPort_);

    if (p == Provider::OneDrive) {
        QUrlQuery q;
        q.addQueryItem("client_id",     ONEDRIVE_CLIENT_ID);
        q.addQueryItem("response_type", "code");
        q.addQueryItem("redirect_uri",  redirectUri);
        q.addQueryItem("scope",         ONEDRIVE_SCOPE);
        q.addQueryItem("response_mode", "query");
        authUrl = "https://login.microsoftonline.com/common/oauth2/v2.0/authorize?" + q.toString();
    } else {
        QUrlQuery q;
        q.addQueryItem("client_id",     GOOGLE_CLIENT_ID);
        q.addQueryItem("response_type", "code");
        q.addQueryItem("redirect_uri",  redirectUri);
        q.addQueryItem("scope",         GOOGLE_SCOPE);
        q.addQueryItem("access_type",   "offline");
        authUrl = "https://accounts.google.com/o/oauth2/v2/auth?" + q.toString();
    }

    // 브라우저에서 인증 URL 열기
    QDesktopServices::openUrl(QUrl(authUrl));
    emit authRequired(authUrl);
}

void CloudDriveManager::logout(Provider p) {
    QSettings s("GaonCommunication", "Sorinuri");
    if (p == Provider::OneDrive) {
        oneDriveToken_.clear();
        oneDriveRefreshToken_.clear();
        s.remove("cloud/onedrive_token");
        s.remove("cloud/onedrive_refresh");
    } else {
        googleToken_.clear();
        googleRefreshToken_.clear();
        s.remove("cloud/google_token");
        s.remove("cloud/google_refresh");
    }
}

void CloudDriveManager::startCallbackServer() {
    if (server_ && server_->isListening()) return;

    server_ = new QTcpServer(this);
    connect(server_, &QTcpServer::newConnection, this, &CloudDriveManager::onCallbackReceived);

    if (!server_->listen(QHostAddress::LocalHost, callbackPort_)) {
        emit error("OAuth 콜백 서버 시작 실패: " + server_->errorString());
    }
}

void CloudDriveManager::stopCallbackServer() {
    if (server_) {
        server_->close();
        server_->deleteLater();
        server_ = nullptr;
    }
}

void CloudDriveManager::onCallbackReceived() {
    if (!server_) return;
    auto* socket = server_->nextPendingConnection();
    if (!socket) return;

    connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
        QString request = QString::fromUtf8(socket->readAll());

        // HTTP 응답 전송
        QString response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
            "<html><body><h2>소리누리 클라우드 연동 완료</h2>"
            "<p>이 창을 닫고 소리누리로 돌아가세요.</p></body></html>";
        socket->write(response.toUtf8());
        socket->flush();
        socket->disconnectFromHost();

        // code 파라미터 추출
        QRegularExpression codeRe(R"(GET /callback\?.*code=([^&\s]+))");
        auto m = codeRe.match(request);
        if (m.hasMatch()) {
            QString code = QUrl::fromPercentEncoding(m.captured(1).toUtf8());
            exchangeCodeForToken(pendingProvider_, code);
        }

        stopCallbackServer();
    });
}

void CloudDriveManager::exchangeCodeForToken(Provider p, const QString& code) {
    QString redirectUri = QString("http://localhost:%1/callback").arg(callbackPort_);
    QUrlQuery body;

    if (p == Provider::OneDrive) {
        body.addQueryItem("client_id",    ONEDRIVE_CLIENT_ID);
        body.addQueryItem("code",         code);
        body.addQueryItem("redirect_uri", redirectUri);
        body.addQueryItem("grant_type",   "authorization_code");

        QUrl msUrl("https://login.microsoftonline.com/common/oauth2/v2.0/token");
        QNetworkRequest req(msUrl);
        req.setHeader(QNetworkRequest::ContentTypeHeader, QByteArray("application/x-www-form-urlencoded"));
        auto* reply = nam_->post(req, body.toString(QUrl::FullyEncoded).toUtf8());
        connect(reply, &QNetworkReply::finished, this, [this, reply, p]() {
            reply->deleteLater();
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            oneDriveToken_        = doc.object().value("access_token").toString();
            oneDriveRefreshToken_ = doc.object().value("refresh_token").toString();
            if (!oneDriveToken_.isEmpty()) {
                QSettings s("GaonCommunication", "Sorinuri");
                s.setValue("cloud/onedrive_token",   oneDriveToken_);
                s.setValue("cloud/onedrive_refresh", oneDriveRefreshToken_);
                emit authenticated(p);
            }
        });
    } else {
        body.addQueryItem("client_id",    GOOGLE_CLIENT_ID);
        body.addQueryItem("code",         code);
        body.addQueryItem("redirect_uri", redirectUri);
        body.addQueryItem("grant_type",   "authorization_code");

        QUrl googleUrl("https://oauth2.googleapis.com/token");
        QNetworkRequest req(googleUrl);
        req.setHeader(QNetworkRequest::ContentTypeHeader, QByteArray("application/x-www-form-urlencoded"));
        auto* reply = nam_->post(req, body.toString(QUrl::FullyEncoded).toUtf8());
        connect(reply, &QNetworkReply::finished, this, [this, reply, p]() {
            reply->deleteLater();
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            googleToken_        = doc.object().value("access_token").toString();
            googleRefreshToken_ = doc.object().value("refresh_token").toString();
            if (!googleToken_.isEmpty()) {
                QSettings s("GaonCommunication", "Sorinuri");
                s.setValue("cloud/google_token",   googleToken_);
                s.setValue("cloud/google_refresh", googleRefreshToken_);
                emit authenticated(p);
            }
        });
    }
}

void CloudDriveManager::listFiles(Provider p, const QString& folderId) {
    if (p == Provider::OneDrive) listOneDriveFiles(folderId);
    else                          listGoogleDriveFiles(folderId);
}

void CloudDriveManager::listOneDriveFiles(const QString& folderId) {
    if (oneDriveToken_.isEmpty()) {
        emit error("OneDrive 인증이 필요합니다");
        return;
    }

    // 미디어 파일만 필터링 (이미지 제외)
    QString filter = "$filter=file ne null"
        "&$select=id,name,file,size,@microsoft.graph.downloadUrl"
        "&$top=200";

    QString url = folderId.isEmpty()
        ? "https://graph.microsoft.com/v1.0/me/drive/root/children?" + filter
        : "https://graph.microsoft.com/v1.0/me/drive/items/" + folderId + "/children?" + filter;

    QUrl reqUrl(url);
    QNetworkRequest req(reqUrl);
    req.setRawHeader("Authorization", ("Bearer " + oneDriveToken_).toUtf8());

    auto* reply = nam_->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit error("OneDrive 파일 목록 오류: " + reply->errorString());
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonArray items  = doc.object().value("value").toArray();

        static const QStringList MEDIA_EXTS = {
            "mp4","mkv","avi","mov","wmv","mp3","flac","wav","aac","m4a","ogg","opus","m4v","webm"
        };

        QList<CloudFile> files;
        for (const QJsonValue& v : items) {
            QJsonObject obj = v.toObject();
            QString name = obj.value("name").toString();
            QString ext  = QFileInfo(name).suffix().toLower();
            if (!MEDIA_EXTS.contains(ext)) continue;

            CloudFile f;
            f.id          = obj.value("id").toString();
            f.name        = name;
            f.size        = obj.value("size").toVariant().toLongLong();
            f.downloadUrl = obj.value("@microsoft.graph.downloadUrl").toString();
            f.mimeType    = obj.value("file").toObject().value("mimeType").toString();
            files.append(f);
        }
        emit filesListed(files);
    });
}

void CloudDriveManager::listGoogleDriveFiles(const QString& folderId) {
    if (googleToken_.isEmpty()) {
        emit error("Google Drive 인증이 필요합니다");
        return;
    }

    QString q = folderId.isEmpty()
        ? "mimeType contains 'video/' or mimeType contains 'audio/'"
        : QString("'%1' in parents and (mimeType contains 'video/' or mimeType contains 'audio/')").arg(folderId);

    QUrlQuery query;
    query.addQueryItem("q",      q);
    query.addQueryItem("fields", "files(id,name,mimeType,size)");
    query.addQueryItem("pageSize", "200");

    QUrl gdriveUrl("https://www.googleapis.com/drive/v3/files?" + query.toString());
    QNetworkRequest req(gdriveUrl);
    req.setRawHeader("Authorization", ("Bearer " + googleToken_).toUtf8());

    auto* reply = nam_->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit error("Google Drive 파일 목록 오류: " + reply->errorString());
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonArray items  = doc.object().value("files").toArray();

        QList<CloudFile> files;
        for (const QJsonValue& v : items) {
            QJsonObject obj = v.toObject();
            CloudFile f;
            f.id       = obj.value("id").toString();
            f.name     = obj.value("name").toString();
            f.mimeType = obj.value("mimeType").toString();
            f.size     = obj.value("size").toVariant().toLongLong();
            // Google Drive 스트리밍 URL
            f.downloadUrl = QString("https://www.googleapis.com/drive/v3/files/%1?alt=media").arg(f.id);
            files.append(f);
        }
        emit filesListed(files);
    });
}

void CloudDriveManager::getDownloadUrl(Provider p, const CloudFile& file) {
    if (p == Provider::OneDrive) {
        // OneDrive는 이미 downloadUrl이 있음
        emit downloadUrlReady(file.downloadUrl, file.name);
    } else {
        // Google Drive는 Authorization 헤더가 필요한 URL
        // MPV에서 직접 사용할 수 있도록 토큰을 URL 파라미터에 포함
        QString url = file.downloadUrl + "&access_token=" + googleToken_;
        emit downloadUrlReady(url, file.name);
    }
}
