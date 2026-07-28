#pragma once
#include <QObject>
#include <QString>
#include <QProcess>
#include <QNetworkAccessManager>
#include <QNetworkReply>

/**
 * YtdlpManager - yt-dlp 자동 관리
 *
 * yt-dlp.exe를 앱 폴더에 자동 다운로드/업데이트하고,
 * MPV가 yt-dlp를 통해 유튜브 등 스트리밍 사이트를 재생할 수 있도록 합니다.
 *
 * 유튜브 5.1 서라운드 지원:
 *   - 유튜브는 Dolby Digital+ (E-AC3) 5.1 스트림을 제공
 *   - yt-dlp가 최고 품질 오디오 스트림 선택
 *   - MPV WASAPI 패스스루로 AV 앰프에 전달
 */
class YtdlpManager : public QObject {
    Q_OBJECT
public:
    explicit YtdlpManager(QObject* parent = nullptr);

    // yt-dlp 경로 반환 (없으면 빈 문자열)
    QString ytdlpPath() const;

    // yt-dlp 존재 여부
    bool isAvailable() const;

    // 비동기 다운로드/업데이트 (완료 시 ytdlpReady 시그널)
    void downloadOrUpdate();

    // URL이 지원되는 스트리밍 사이트인지 확인
    static bool isSupportedUrl(const QString& url);

    // URL이 유튜브인지 확인
    static bool isYouTubeUrl(const QString& url);

signals:
    void ytdlpReady(const QString& path);
    void downloadProgress(int percent);
    void downloadFailed(const QString& error);

private slots:
    void onDownloadFinished(QNetworkReply* reply);
    void onDownloadProgress(qint64 received, qint64 total);

private:
    QString appDir() const;
    QNetworkAccessManager* nam_ = nullptr;
    bool downloading_ = false;
};
