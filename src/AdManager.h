#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>
#include <QString>

/**
 * AdManager — 소리누리 광고 관리자
 *
 * 서버(sorinuri.com/api/ad-api.php)에서 광고 정보를 가져오고
 * 노출/클릭 카운터를 전송합니다.
 *
 * 슬롯:
 *   "splash"  — 앱 시작 화면
 *   "ott"     — OTT 모드 진입 시
 */
class AdManager : public QObject {
    Q_OBJECT
public:
    explicit AdManager(QObject* parent = nullptr);

    // 광고 요청 (비동기 — adReady 시그널로 결과 전달)
    void fetchAd(const QString& slot);

    // 클릭 카운터 전송
    void reportClick(int adId, const QString& slot);

    // 현재 앱 버전 설정
    void setAppVersion(const QString& v) { appVersion_ = v; }

signals:
    // 광고 데이터 수신 완료
    // adObj: { id, slot, type, media_url, click_url, duration_sec }
    // adObj가 비어있으면 광고 없음
    void adReady(const QString& slot, const QJsonObject& adObj);

private slots:
    void onFetchReply();

private:
    QNetworkAccessManager* nam_;
    QString appVersion_ = "6.2.0";

    static constexpr const char* BASE_URL = "https://sorinuri.com/api/ad-api.php";
};
