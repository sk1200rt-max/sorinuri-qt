#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonArray>

/**
 * SubtitleDownloader - OpenSubtitles REST API v1 연동 자막 다운로더
 *
 * 동작 흐름:
 *  1. searchByHash(filePath, lang) : 파일 해시 기반 검색 (가장 정확)
 *  2. searchByName(title, lang)    : 파일명 기반 폴백 검색
 *  3. download(fileId)             : 선택된 자막 다운로드 URL 획득
 *  4. downloadFile(url, destPath)  : 실제 파일 다운로드
 *
 * 설정: QSettings "subtitle/opensubtitles_apikey" 에서 API 키 읽음
 */
struct SubtitleResult {
    QString id;           // file_id
    QString fileName;     // 자막 파일명
    QString language;     // 언어 코드 (ko, en ...)
    int     downloadCount = 0;
    double  rating        = 0.0;
    bool    hearingImpaired = false;
};

class SubtitleDownloader : public QObject {
    Q_OBJECT
public:
    explicit SubtitleDownloader(QObject* parent = nullptr);

    // 파일 해시 계산 (OpenSubtitles 방식: 파일 앞/뒤 64KB XOR + 파일크기)
    static quint64 computeHash(const QString& filePath, qint64* outSize = nullptr);

    // 자막 검색 (해시 우선, 실패 시 파일명 폴백)
    void search(const QString& filePath, const QStringList& languages = {"ko", "en"});

    // 자막 다운로드 (file_id → 임시 파일 경로 반환)
    void download(const QString& fileId, const QString& destDir);

    // 자막 번역 (DeepL 또는 파파고 API 사용)
    void translate(const QString& srtPath, const QString& targetLang,
                   const QString& deeplKey,
                   const QString& papagoClientId,
                   const QString& papagoClientSecret);

    void setApiKey(const QString& key) { apiKey_ = key; }
    QString apiKey() const { return apiKey_; }

signals:
    void searchFinished(const QList<SubtitleResult>& results);
    void downloadFinished(const QString& savedPath);      // 다운로드 성공
    void translateFinished(const QString& translatedPath); // 번역 성공
    void errorOccurred(const QString& message);

private slots:
    void onSearchReply(QNetworkReply* reply);
    void onDownloadLinkReply(QNetworkReply* reply, const QString& destDir);
    void onFileDownloadReply(QNetworkReply* reply, const QString& destPath);

private:
    void searchByHash(const QString& filePath, const QStringList& langs);
    void searchByName(const QString& query, const QStringList& langs);
    void parseSearchResults(const QJsonArray& data);

    QNetworkAccessManager* nam_    = nullptr;
    QString                apiKey_;
    QString                pendingFilePath_;
    QStringList            pendingLangs_;
    bool                   hashSearchDone_ = false;
};
