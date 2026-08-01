#include "AdManager.h"
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QUrlQuery>

AdManager::AdManager(QObject* parent) : QObject(parent) {
    nam_ = new QNetworkAccessManager(this);
}

void AdManager::fetchAd(const QString& slot) {
    QUrl url(BASE_URL);
    QUrlQuery q;
    q.addQueryItem("slot", slot);
    q.addQueryItem("v", appVersion_);
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::User, slot);

    auto* reply = nam_->get(req);
    connect(reply, &QNetworkReply::finished, this, &AdManager::onFetchReply);
}

void AdManager::onFetchReply() {
    auto* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    reply->deleteLater();

    QString slot = reply->request().attribute(QNetworkRequest::User).toString();

    if (reply->error() != QNetworkReply::NoError) {
        emit adReady(slot, QJsonObject());
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (doc.isNull() || !doc.isObject()) {
        emit adReady(slot, QJsonObject());
        return;
    }

    QJsonObject root = doc.object();
    QJsonObject adObj;
    if (root.contains("ad") && root["ad"].isObject())
        adObj = root["ad"].toObject();

    emit adReady(slot, adObj);
}

void AdManager::reportClick(int adId, const QString& slot) {
    QUrl url(BASE_URL);
    QUrlQuery q;
    q.addQueryItem("action", "click");
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject body;
    body["ad_id"] = adId;
    body["slot"]  = slot;
    body["v"]     = appVersion_;

    auto* reply = nam_->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
}
