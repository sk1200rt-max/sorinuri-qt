#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QString>

// 자동 업데이트 체커
// 앱 시작 시 https://sorinuri.com/downloads/version.json 을 확인하여
// 새 버전이 있으면 updateAvailable 시그널을 발생시킵니다.
class UpdateChecker : public QObject {
    Q_OBJECT
public:
    explicit UpdateChecker(QObject* parent = nullptr);

    // 현재 앱 버전 (빌드 시 자동 업데이트)
    static QString currentVersion() { return "4.2.7"; }

    // 버전 체크 시작 (비동기, 앱 시작 후 3초 지연)
    void checkForUpdates();

signals:
    // 새 버전이 있을 때 발생
    void updateAvailable(const QString& newVersion,
                         const QString& releaseNotes,
                         const QString& installerUrl);

private slots:
    void onReplyFinished(QNetworkReply* reply);

private:
    QNetworkAccessManager* nam_;

    static const QString VERSION_URL;
};
