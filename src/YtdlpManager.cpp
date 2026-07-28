#include "YtdlpManager.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>

// yt-dlp 최신 릴리즈 다운로드 URL (Windows x64)
static const char* YTDLP_DOWNLOAD_URL =
    "https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp.exe";

YtdlpManager::YtdlpManager(QObject* parent) : QObject(parent) {
    nam_ = new QNetworkAccessManager(this);
}

QString YtdlpManager::appDir() const {
    return QCoreApplication::applicationDirPath();
}

QString YtdlpManager::ytdlpPath() const {
    QString path = appDir() + "/yt-dlp.exe";
    if (QFile::exists(path)) return path;
    return {};
}

bool YtdlpManager::isAvailable() const {
    return !ytdlpPath().isEmpty();
}

void YtdlpManager::downloadOrUpdate() {
    if (downloading_) return;
    downloading_ = true;

    qInfo() << "[yt-dlp] 다운로드 시작:" << YTDLP_DOWNLOAD_URL;
    emit downloadProgress(0);

    // Qt6 방식: QNetworkRequest에 리다이렉트 정책 설정
    QNetworkRequest req;
    req.setUrl(QUrl(QString::fromUtf8(YTDLP_DOWNLOAD_URL)));
    req.setHeader(QNetworkRequest::UserAgentHeader, "Sorinuri/1.1");
    // Qt6에서는 setTransferTimeout 사용
    req.setTransferTimeout(120000);  // 120초

    QNetworkReply* reply = nam_->get(req);
    if (!reply) {
        downloading_ = false;
        emit downloadFailed("네트워크 요청 실패");
        return;
    }

    // 리다이렉트 자동 처리 (Qt6에서는 기본 활성화)
    connect(reply, &QNetworkReply::downloadProgress,
            this, &YtdlpManager::onDownloadProgress);
    connect(reply, &QNetworkReply::finished,
            this, [this, reply]() { onDownloadFinished(reply); });
}

void YtdlpManager::onDownloadProgress(qint64 received, qint64 total) {
    if (total > 0) {
        int pct = static_cast<int>(received * 100 / total);
        emit downloadProgress(pct);
    }
}

void YtdlpManager::onDownloadFinished(QNetworkReply* reply) {
    downloading_ = false;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        QString err = reply->errorString();
        qWarning() << "[yt-dlp] 다운로드 실패:" << err;
        emit downloadFailed(err);
        return;
    }

    // 리다이렉트 처리
    QVariant redirect = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
    if (redirect.isValid()) {
        // 리다이렉트가 있으면 다시 요청
        QNetworkRequest req;
        req.setUrl(redirect.toUrl());
        req.setHeader(QNetworkRequest::UserAgentHeader, "Sorinuri/1.1");
        req.setTransferTimeout(120000);
        QNetworkReply* newReply = nam_->get(req);
        connect(newReply, &QNetworkReply::downloadProgress,
                this, &YtdlpManager::onDownloadProgress);
        connect(newReply, &QNetworkReply::finished,
                this, [this, newReply]() { onDownloadFinished(newReply); });
        downloading_ = true;
        return;
    }

    QByteArray data = reply->readAll();
    if (data.isEmpty()) {
        emit downloadFailed("빈 응답");
        return;
    }

    QString destPath = appDir() + "/yt-dlp.exe";
    QString tmpPath  = destPath + ".tmp";

    QFile f(tmpPath);
    if (!f.open(QIODevice::WriteOnly)) {
        emit downloadFailed("파일 쓰기 실패: " + tmpPath);
        return;
    }
    f.write(data);
    f.close();

    QFile::remove(destPath);
    if (!QFile::rename(tmpPath, destPath)) {
        emit downloadFailed("파일 교체 실패");
        return;
    }

    qInfo() << "[yt-dlp] 다운로드 완료:" << destPath
            << "(" << data.size() / 1024 / 1024 << "MB)";
    emit downloadProgress(100);
    emit ytdlpReady(destPath);
}

bool YtdlpManager::isSupportedUrl(const QString& url) {
    static const QStringList supported = {
        "youtube.com", "youtu.be",
        "twitch.tv",
        "vimeo.com",
        "dailymotion.com",
        "nicovideo.jp",
        "bilibili.com",
        "soundcloud.com",
        "bandcamp.com",
        "rtmp://", "rtsp://", "mms://",
        ".m3u8", ".mpd"
    };
    for (const QString& s : supported) {
        if (url.contains(s)) return true;
    }
    return url.startsWith("http://") || url.startsWith("https://");
}

bool YtdlpManager::isYouTubeUrl(const QString& url) {
    return url.contains("youtube.com") || url.contains("youtu.be");
}
