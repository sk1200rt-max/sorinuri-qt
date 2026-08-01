#pragma once
#include <QWidget>
#include <QLabel>
#include <QTimer>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QMouseEvent>
#include <QPaintEvent>

/**
 * SplashAdWidget — 앱 시작 시 로고 옆에 표시되는 광고 위젯
 *
 * - 이미지/유튜브 썸네일을 표시
 * - duration_sec 초 후 자동으로 사라짐
 * - 클릭 시 브라우저로 click_url 열기
 */
class SplashAdWidget : public QWidget {
    Q_OBJECT
public:
    explicit SplashAdWidget(QWidget* parent = nullptr);

    // 광고 데이터 설정 후 표시 시작
    void showAd(const QJsonObject& adObj);

signals:
    void clicked(int adId, const QString& slot);
    void closed();

protected:
    void mousePressEvent(QMouseEvent* e) override;
    void paintEvent(QPaintEvent* e) override;

private slots:
    void onImageLoaded();
    void onTimeout();

private:
    QLabel*               imageLabel_  = nullptr;
    QLabel*               countLabel_  = nullptr;
    QTimer*               closeTimer_  = nullptr;
    QTimer*               countTimer_  = nullptr;
    QNetworkAccessManager* nam_        = nullptr;

    QJsonObject   adObj_;
    int           remainSec_   = 3;
};
