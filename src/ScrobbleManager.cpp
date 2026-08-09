#include "ScrobbleManager.h"
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QCryptographicHash>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <functional>

const QString ScrobbleManager::API_URL = "https://ws.audioscrobbler.com/2.0/";

ScrobbleManager::ScrobbleManager(QObject* parent) : QObject(parent) {
    nam_ = new QNetworkAccessManager(this);

    scrobbleTimer_ = new QTimer(this);
    scrobbleTimer_->setSingleShot(true);
    connect(scrobbleTimer_, &QTimer::timeout, this, &ScrobbleManager::onScrobbleTimer);

    // 저장된 설정 불러오기
    QSettings s("GaonCommunication", "Sorinuri");
    enabled_    = s.value("lastfm/enabled", false).toBool();
    sessionKey_ = s.value("lastfm/session_key").toString();
    apiKey_     = s.value("lastfm/api_key", apiKey_).toString();
    apiSecret_  = s.value("lastfm/api_secret", apiSecret_).toString();
}

void ScrobbleManager::setEnabled(bool on) {
    enabled_ = on;
    QSettings s("GaonCommunication", "Sorinuri");
    s.setValue("lastfm/enabled", on);
}

void ScrobbleManager::onTrackStarted(const QString& title, const QString& artist,
                                      const QString& album, double duration) {
    if (!enabled_ || sessionKey_.isEmpty()) return;

    // 이전 트랙 스크로블 취소
    scrobbleTimer_->stop();
    scrobbled_ = false;

    currentTitle_    = title;
    currentArtist_   = artist;
    currentAlbum_    = album;
    currentDuration_ = duration;
    currentPos_      = 0.0;
    startTime_       = QDateTime::currentSecsSinceEpoch();

    // Now Playing 업데이트
    updateNowPlaying();

    // 스크로블 타이머: 재생 시간 50% 또는 최대 4분 후
    double scrobbleDelay = qMin(duration * 0.5, 240.0);
    if (scrobbleDelay > 30.0) {  // 최소 30초 이상 재생된 경우만
        scrobbleTimer_->start((int)(scrobbleDelay * 1000));
    }
}

void ScrobbleManager::onTrackStopped() {
    scrobbleTimer_->stop();
    currentTitle_.clear();
    currentArtist_.clear();
}

void ScrobbleManager::onPositionChanged(double pos) {
    currentPos_ = pos;
}

void ScrobbleManager::onScrobbleTimer() {
    if (!scrobbled_ && !currentTitle_.isEmpty()) {
        scrobble();
    }
}

void ScrobbleManager::updateNowPlaying() {
    if (currentTitle_.isEmpty() || currentArtist_.isEmpty()) return;

    QMap<QString, QString> params;
    params["method"]  = "track.updateNowPlaying";
    params["track"]   = currentTitle_;
    params["artist"]  = currentArtist_;
    if (!currentAlbum_.isEmpty()) params["album"] = currentAlbum_;
    params["api_key"] = apiKey_;
    params["sk"]      = sessionKey_;

    sendRequest(params, [this](const QByteArray& data) {
        Q_UNUSED(data)
        emit nowPlayingUpdated(currentTitle_, currentArtist_);
    });
}

void ScrobbleManager::scrobble() {
    if (currentTitle_.isEmpty() || currentArtist_.isEmpty()) return;

    QMap<QString, QString> params;
    params["method"]     = "track.scrobble";
    params["track"]      = currentTitle_;
    params["artist"]     = currentArtist_;
    if (!currentAlbum_.isEmpty()) params["album"] = currentAlbum_;
    params["timestamp"]  = QString::number(startTime_);
    params["api_key"]    = apiKey_;
    params["sk"]         = sessionKey_;

    sendRequest(params, [this](const QByteArray& data) {
        Q_UNUSED(data)
        scrobbled_ = true;
        emit scrobbled(currentTitle_, currentArtist_);
    });
}

QString ScrobbleManager::signRequest(const QMap<QString, QString>& params) const {
    // Last.fm API 서명: 파라미터 알파벳 순 정렬 후 시크릿 추가, MD5 해시
    QString sig;
    for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
        if (it.key() == "format") continue;
        sig += it.key() + it.value();
    }
    sig += apiSecret_;
    return QString(QCryptographicHash::hash(sig.toUtf8(), QCryptographicHash::Md5).toHex());
}

void ScrobbleManager::sendRequest(const QMap<QString, QString>& params,
                                   std::function<void(const QByteArray&)> callback) {
    QMap<QString, QString> allParams = params;
    allParams["format"] = "json";
    allParams["api_sig"] = signRequest(allParams);

    QUrlQuery query;
    for (auto it = allParams.constBegin(); it != allParams.constEnd(); ++it) {
        query.addQueryItem(it.key(), it.value());
    }

    QNetworkRequest req(QUrl(API_URL));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    req.setHeader(QNetworkRequest::UserAgentHeader, "Sorinuri/6.17.0");

    QByteArray postData = query.toString(QUrl::FullyEncoded).toUtf8();
    auto* reply = nam_->post(req, postData);
    connect(reply, &QNetworkReply::finished, this, [this, reply, callback]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit error("Last.fm API 오류: " + reply->errorString());
            return;
        }
        callback(reply->readAll());
    });
}

QString ScrobbleManager::authUrl() const {
    return QString("https://www.last.fm/api/auth/?api_key=%1&cb=sorinuri://lastfm-auth")
           .arg(apiKey_);
}

void ScrobbleManager::fetchSessionKey(const QString& token) {
    QMap<QString, QString> params;
    params["method"]  = "auth.getSession";
    params["api_key"] = apiKey_;
    params["token"]   = token;

    sendRequest(params, [this](const QByteArray& data) {
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QString sk = doc.object().value("session").toObject().value("key").toString();
        if (!sk.isEmpty()) {
            sessionKey_ = sk;
            QSettings s("GaonCommunication", "Sorinuri");
            s.setValue("lastfm/session_key", sk);
            emit sessionKeyFetched(sk);
        }
    });
}
