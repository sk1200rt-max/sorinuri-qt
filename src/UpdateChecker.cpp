#include "UpdateChecker.h"
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QDebug>

const QString UpdateChecker::VERSION_URL =
    "https://sorinuri.com/downloads/version.json";

UpdateChecker::UpdateChecker(QObject* parent)
    : QObject(parent)
    , nam_(new QNetworkAccessManager(this))
{
    connect(nam_, &QNetworkAccessManager::finished,
            this, &UpdateChecker::onReplyFinished);
}

void UpdateChecker::checkForUpdates() {
    // 앱 시작 후 3초 뒤에 체크 (UI 로딩 완료 후)
    QTimer::singleShot(3000, this, [this]() {
        QNetworkRequest req(QUrl(VERSION_URL));
        req.setHeader(QNetworkRequest::UserAgentHeader,
                      QString("Sorinuri/%1").arg(currentVersion()));
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
        nam_->get(req);
        qDebug() << "[UpdateChecker] 버전 확인 중...";
    });
}

// 버전 비교: "4.2.1" > "4.2.0" → true
static bool isNewerVersion(const QString& remote, const QString& current) {
    auto split = [](const QString& v) {
        QList<int> parts;
        for (const QString& p : v.split('.'))
            parts << p.toInt();
        while (parts.size() < 3) parts << 0;
        return parts;
    };
    auto r = split(remote);
    auto c = split(current);
    for (int i = 0; i < 3; ++i) {
        if (r[i] > c[i]) return true;
        if (r[i] < c[i]) return false;
    }
    return false;
}

void UpdateChecker::onReplyFinished(QNetworkReply* reply) {
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "[UpdateChecker] 네트워크 오류:" << reply->errorString();
        return;
    }

    QByteArray data = reply->readAll();
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        qDebug() << "[UpdateChecker] JSON 파싱 오류:" << err.errorString();
        return;
    }

    QJsonObject obj = doc.object();
    QString remoteVersion = obj.value("version").toString();
    QString releaseNotes  = obj.value("notes").toString();
    QString installerUrl  = obj.value("installer_url").toString();

    if (remoteVersion.isEmpty()) return;

    qDebug() << "[UpdateChecker] 서버 버전:" << remoteVersion
             << "/ 현재 버전:" << currentVersion();

    if (isNewerVersion(remoteVersion, currentVersion())) {
        emit updateAvailable(remoteVersion, releaseNotes, installerUrl);
    }
}
