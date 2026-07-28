#include "YtdlpManager.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QNetworkRequest>
#include <QUrl>

// yt-dlp 최신 릴리즈 다운로드 URL (Windows x64)
static const char* YTDLP_DOWNLOAD_URL =
    "https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp.exe";

YtdlpManager::YtdlpManager(QObject* parent) : QObject(parent) {
    nam_ = new QNetworkAccessManager(this);
    connect(nam_, &QNetworkAccessManager::finished,
            this, &YtdlpManager::onDownloadFinished);
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

    QNetworkRequest req(QUrl(YTDLP_DOWNLOAD_URL));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setRawHeader("User-Agent", "Sorinuri/1.0");

    QNetworkReply* reply = nam_->get(req);
    connect(reply, &QNetworkReply::downloadProgress,
            this, &YtdlpManager::onDownloadProgress);
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

    QByteArray data = reply->readAll();
    if (data.isEmpty()) {
        emit downloadFailed("빈 응답");
        return;
    }

    QString destPath = appDir() + "/yt-dlp.exe";
    // 임시 파일로 먼저 저장 후 교체 (실행 중 덮어쓰기 방지)
    QString tmpPath = destPath + ".tmp";
    QFile f(tmpPath);
    if (!f.open(QIODevice::WriteOnly)) {
        emit downloadFailed("파일 쓰기 실패: " + tmpPath);
        return;
    }
    f.write(data);
    f.close();

    // 기존 파일 교체
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
    // yt-dlp가 지원하는 주요 사이트
    static const QStringList supported = {
        "youtube.com", "youtu.be",
        "twitch.tv",
        "vimeo.com",
        "dailymotion.com",
        "niconico", "nicovideo.jp",
        "bilibili.com",
        "soundcloud.com",
        "bandcamp.com",
        // 직접 스트림 프로토콜
        "rtmp://", "rtsp://", "mms://",
        // HTTP 직접 미디어
        ".m3u8", ".mpd"
    };
    for (const QString& s : supported) {
        if (url.contains(s)) return true;
    }
    // http/https로 시작하는 모든 URL 허용
    return url.startsWith("http://") || url.startsWith("https://");
}

bool YtdlpManager::isYouTubeUrl(const QString& url) {
    return url.contains("youtube.com") || url.contains("youtu.be");
}
