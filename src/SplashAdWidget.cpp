#include "SplashAdWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPixmap>
#include <QDesktopServices>
#include <QUrl>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonObject>
#include <QUrlQuery>
#include <QApplication>
#include <QScreen>

SplashAdWidget::SplashAdWidget(QWidget* parent) : QWidget(parent) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(320, 90);

    nam_ = new QNetworkAccessManager(this);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(4);

    imageLabel_ = new QLabel(this);
    imageLabel_->setFixedSize(296, 60);
    imageLabel_->setAlignment(Qt::AlignCenter);
    imageLabel_->setStyleSheet("background: #111; border-radius: 4px;");
    layout->addWidget(imageLabel_);

    countLabel_ = new QLabel(this);
    countLabel_->setAlignment(Qt::AlignRight);
    countLabel_->setStyleSheet("color: #444; font-size: 11px; background: transparent;");
    layout->addWidget(countLabel_);

    closeTimer_ = new QTimer(this);
    closeTimer_->setSingleShot(true);
    connect(closeTimer_, &QTimer::timeout, this, &SplashAdWidget::onTimeout);

    countTimer_ = new QTimer(this);
    countTimer_->setInterval(1000);
    connect(countTimer_, &QTimer::timeout, this, [this]() {
        remainSec_--;
        countLabel_->setText(QString("광고 %1초 후 종료").arg(remainSec_));
        if (remainSec_ <= 0) countTimer_->stop();
    });
}

void SplashAdWidget::showAd(const QJsonObject& adObj) {
    if (adObj.isEmpty()) return;
    adObj_ = adObj;

    int dur = adObj["duration_sec"].toInt(3);
    remainSec_ = dur;
    countLabel_->setText(QString("광고 %1초 후 종료").arg(dur));

    QString type     = adObj["type"].toString();
    QString mediaUrl = adObj["media_url"].toString();

    if (!mediaUrl.isEmpty() && (type == "image" || type == "youtube")) {
        // 유튜브인 경우 썸네일 URL로 변환
        if (type == "youtube") {
            // https://www.youtube.com/watch?v=XXXX → https://img.youtube.com/vi/XXXX/mqdefault.jpg
            QUrl u(mediaUrl);
            QString vid = u.hasQuery() ? QUrlQuery(u).queryItemValue("v") : u.path().split('/').last();
            if (!vid.isEmpty())
                mediaUrl = QString("https://img.youtube.com/vi/%1/mqdefault.jpg").arg(vid);
        }

        const QUrl imgUrl(mediaUrl);
        QNetworkRequest req;
        req.setUrl(imgUrl);
        auto* reply = nam_->get(req);
        connect(reply, &QNetworkReply::finished, this, &SplashAdWidget::onImageLoaded);
    } else {
        imageLabel_->setText("광고");
    }

    // 부모 창 우측 하단에 위치
    if (parentWidget()) {
        QRect pr = parentWidget()->geometry();
        move(pr.right() - width() - 8, pr.bottom() - height() - 8);
    }

    show();
    raise();
    closeTimer_->start(dur * 1000);
    countTimer_->start();
}

void SplashAdWidget::onImageLoaded() {
    auto* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    reply->deleteLater();

    if (reply->error() == QNetworkReply::NoError) {
        QPixmap px;
        px.loadFromData(reply->readAll());
        if (!px.isNull()) {
            imageLabel_->setPixmap(px.scaled(imageLabel_->size(),
                Qt::KeepAspectRatio, Qt::SmoothTransformation));
            return;
        }
    }
    imageLabel_->setText("광고");
}

void SplashAdWidget::onTimeout() {
    countTimer_->stop();
    hide();
    emit closed();
}

void SplashAdWidget::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        QString url = adObj_["click_url"].toString();
        if (!url.isEmpty())
            QDesktopServices::openUrl(QUrl(url));
        int id = adObj_["id"].toInt();
        emit clicked(id, adObj_["slot"].toString());
        onTimeout();
    }
}

void SplashAdWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(QColor(10, 10, 10, 230));
    p.setPen(QColor(30, 30, 30));
    p.drawRoundedRect(rect().adjusted(1,1,-1,-1), 8, 8);
}
