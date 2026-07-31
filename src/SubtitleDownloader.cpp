#include "SubtitleDownloader.h"
#include <QFile>
#include <QRegularExpression>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QSettings>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QUrl>
#include <QDataStream>

// ─── OpenSubtitles REST API v1 베이스 URL ────────────────────────────────
static const QString API_BASE = "https://api.opensubtitles.com/api/v1";
static const QString USER_AGENT = "Sorinuri v4.5";

SubtitleDownloader::SubtitleDownloader(QObject* parent)
    : QObject(parent)
    , nam_(new QNetworkAccessManager(this))
{
    QSettings s;
    apiKey_ = s.value("subtitle/opensubtitles_apikey").toString();
}

// ─── OpenSubtitles 해시 알고리즘 ─────────────────────────────────────────
quint64 SubtitleDownloader::computeHash(const QString& filePath, qint64* outSize)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) return 0;

    qint64 fileSize = f.size();
    if (outSize) *outSize = fileSize;

    // 파일이 너무 작으면 해시 계산 불가
    if (fileSize < 131072) return 0;

    quint64 hash = static_cast<quint64>(fileSize);

    // 앞 64KB
    QByteArray headData = f.read(65536);
    QDataStream headStream(headData);
    headStream.setByteOrder(QDataStream::LittleEndian);
    for (int i = 0; i < 8192; ++i) {
        quint64 val = 0;
        headStream >> val;
        hash += val;
    }

    // 뒤 64KB
    f.seek(fileSize - 65536);
    QByteArray tailData = f.read(65536);
    QDataStream tailStream(tailData);
    tailStream.setByteOrder(QDataStream::LittleEndian);
    for (int i = 0; i < 8192; ++i) {
        quint64 val = 0;
        tailStream >> val;
        hash += val;
    }

    f.close();
    return hash;
}

// ─── 자막 검색 진입점 ────────────────────────────────────────────────────
void SubtitleDownloader::search(const QString& filePath, const QStringList& languages)
{
    if (apiKey_.isEmpty()) {
        emit errorOccurred("OpenSubtitles API 키가 설정되지 않았습니다.\n"
                           "설정 → 자막 탭에서 API 키를 입력해 주세요.");
        return;
    }

    pendingFilePath_ = filePath;
    pendingLangs_    = languages;
    hashSearchDone_  = false;

    searchByHash(filePath, languages);
}

void SubtitleDownloader::searchByHash(const QString& filePath, const QStringList& langs)
{
    qint64 fileSize = 0;
    quint64 hash = computeHash(filePath, &fileSize);

    QUrl url(API_BASE + "/subtitles");
    QUrlQuery query;
    if (hash > 0) {
        query.addQueryItem("moviehash", QString("%1").arg(hash, 16, 16, QChar('0')));
        query.addQueryItem("moviebytesize", QString::number(fileSize));
    }
    query.addQueryItem("languages", langs.join(","));
    query.addQueryItem("order_by", "download_count");
    url.setQuery(query);

    QNetworkRequest req(url);
    req.setRawHeader("Api-Key", apiKey_.toUtf8());
    req.setRawHeader("User-Agent", USER_AGENT.toUtf8());
    req.setRawHeader("Content-Type", "application/json");

    auto* reply = nam_->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onSearchReply(reply);
    });
}

void SubtitleDownloader::searchByName(const QString& query, const QStringList& langs)
{
    // 파일명에서 연도, 해상도 등 제거하여 검색어 정제
    QString cleanQuery = query;
    cleanQuery.remove(QRegularExpression("\\.(mkv|mp4|avi|mov|wmv|flac|mp3)$",
                                          QRegularExpression::CaseInsensitiveOption));
    cleanQuery.replace(QRegularExpression("[._\\-]"), " ");
    cleanQuery.remove(QRegularExpression("\\b(1080p|720p|4k|bluray|bdrip|webrip|hdtv|x264|x265|hevc|aac|dts)\\b",
                                          QRegularExpression::CaseInsensitiveOption));
    cleanQuery = cleanQuery.trimmed();

    QUrl url(API_BASE + "/subtitles");
    QUrlQuery q;
    q.addQueryItem("query", cleanQuery);
    q.addQueryItem("languages", langs.join(","));
    q.addQueryItem("order_by", "download_count");
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setRawHeader("Api-Key", apiKey_.toUtf8());
    req.setRawHeader("User-Agent", USER_AGENT.toUtf8());

    auto* reply = nam_->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onSearchReply(reply);
    });
}

void SubtitleDownloader::onSearchReply(QNetworkReply* reply)
{
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        if (!hashSearchDone_) {
            // 해시 검색 실패 → 파일명으로 폴백
            hashSearchDone_ = true;
            searchByName(QFileInfo(pendingFilePath_).fileName(), pendingLangs_);
        } else {
            emit errorOccurred("자막 검색 실패: " + reply->errorString());
        }
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonArray data = doc.object()["data"].toArray();

    if (data.isEmpty() && !hashSearchDone_) {
        // 해시 검색 결과 없음 → 파일명 폴백
        hashSearchDone_ = true;
        searchByName(QFileInfo(pendingFilePath_).fileName(), pendingLangs_);
        return;
    }

    parseSearchResults(data);
}

void SubtitleDownloader::parseSearchResults(const QJsonArray& data)
{
    QList<SubtitleResult> results;
    for (const QJsonValue& v : data) {
        QJsonObject obj = v.toObject();
        QJsonObject attrs = obj["attributes"].toObject();
        QJsonArray files = attrs["files"].toArray();
        if (files.isEmpty()) continue;

        SubtitleResult r;
        r.id              = files[0].toObject()["file_id"].toVariant().toString();
        r.fileName        = files[0].toObject()["file_name"].toString();
        r.language        = attrs["language"].toString();
        r.downloadCount   = attrs["download_count"].toInt();
        r.rating          = attrs["ratings"].toDouble();
        r.hearingImpaired = attrs["hearing_impaired"].toBool();
        if (!r.id.isEmpty()) results.append(r);
    }
    emit searchFinished(results);
}

// ─── 자막 다운로드 ────────────────────────────────────────────────────────
void SubtitleDownloader::download(const QString& fileId, const QString& destDir)
{
    QUrl url(API_BASE + "/download");
    QNetworkRequest req(url);
    req.setRawHeader("Api-Key", apiKey_.toUtf8());
    req.setRawHeader("User-Agent", USER_AGENT.toUtf8());
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject body;
    body["file_id"] = fileId.toInt();
    QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);

    auto* reply = nam_->post(req, payload);
    connect(reply, &QNetworkReply::finished, this, [this, reply, destDir]() {
        onDownloadLinkReply(reply, destDir);
    });
}

void SubtitleDownloader::onDownloadLinkReply(QNetworkReply* reply, const QString& destDir)
{
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        emit errorOccurred("자막 다운로드 링크 획득 실패: " + reply->errorString());
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QString downloadUrl = doc.object()["link"].toString();
    QString fileName    = doc.object()["file_name"].toString();

    if (downloadUrl.isEmpty()) {
        emit errorOccurred("자막 다운로드 URL을 받지 못했습니다.");
        return;
    }

    // 저장 경로 결정
    QDir dir(destDir);
    if (!dir.exists()) dir.mkpath(".");
    QString destPath = dir.filePath(fileName.isEmpty() ? "subtitle.srt" : fileName);

    QNetworkRequest fileReq(downloadUrl);
    fileReq.setRawHeader("User-Agent", USER_AGENT.toUtf8());
    auto* fileReply = nam_->get(fileReq);
    connect(fileReply, &QNetworkReply::finished, this, [this, fileReply, destPath]() {
        onFileDownloadReply(fileReply, destPath);
    });
}

void SubtitleDownloader::onFileDownloadReply(QNetworkReply* reply, const QString& destPath)
{
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        emit errorOccurred("자막 파일 다운로드 실패: " + reply->errorString());
        return;
    }

    QFile f(destPath);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(reply->readAll());
        f.close();
        emit downloadFinished(destPath);
    } else {
        emit errorOccurred("자막 파일 저장 실패: " + destPath);
    }
}
