#pragma once
#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QDateTime>

/**
 * ScrobbleManager - Last.fm 양방향 스크로블링
 *
 * - 재생 시작 시 updateNowPlaying 전송
 * - 재생 시간 50% 또는 4분 이상 재생 시 scrobble 전송
 * - Last.fm API 2.0 규격 준수
 * - API 키/시크릿은 QSettings에 저장
 */
class ScrobbleManager : public QObject {
    Q_OBJECT
public:
    explicit ScrobbleManager(QObject* parent = nullptr);

    bool isEnabled() const { return enabled_; }
    void setEnabled(bool on);

    // 재생 시작 시 호출
    void onTrackStarted(const QString& title, const QString& artist,
                        const QString& album, double duration);
    // 재생 중지/변경 시 호출
    void onTrackStopped();
    // 재생 위치 업데이트
    void onPositionChanged(double pos);

    // 설정
    void setApiKey(const QString& key)    { apiKey_    = key; }
    void setApiSecret(const QString& sec) { apiSecret_ = sec; }
    void setSessionKey(const QString& sk) { sessionKey_ = sk; }
    QString sessionKey() const { return sessionKey_; }

    // Last.fm 인증 URL 생성
    QString authUrl() const;
    // 토큰으로 세션 키 획득
    void fetchSessionKey(const QString& token);

signals:
    void scrobbled(const QString& title, const QString& artist);
    void nowPlayingUpdated(const QString& title, const QString& artist);
    void authRequired(const QString& authUrl);
    void sessionKeyFetched(const QString& sessionKey);
    void error(const QString& msg);

private slots:
    void onScrobbleTimer();

private:
    QString signRequest(const QMap<QString, QString>& params) const;
    void sendRequest(const QMap<QString, QString>& params,
                     std::function<void(const QByteArray&)> callback);
    void updateNowPlaying();
    void scrobble();

    QNetworkAccessManager* nam_         = nullptr;
    QTimer*                scrobbleTimer_ = nullptr;

    bool    enabled_     = false;
    QString apiKey_      = "b25b959554ed76058ac220b7b2e0a026";  // 소리누리 Last.fm 앱 키
    QString apiSecret_   = "425f55b1e2b2b8e7d9c2f3a4e5b6c7d8";  // 소리누리 Last.fm 앱 시크릿
    QString sessionKey_;

    // 현재 트랙 정보
    QString currentTitle_;
    QString currentArtist_;
    QString currentAlbum_;
    double  currentDuration_ = 0.0;
    double  currentPos_      = 0.0;
    qint64  startTime_       = 0;
    bool    scrobbled_       = false;

    static const QString API_URL;
};
