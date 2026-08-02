#include "LyricsWidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>

LyricsWidget::LyricsWidget(QWidget* parent) : QWidget(parent) {
    setAutoFillBackground(false);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    // 마우스 이동 이벤트를 MainWindow까지 전달하여 커서 숨김 방지
    setMouseTracking(true);

    nam_ = new QNetworkAccessManager(this);
    connect(nam_, &QNetworkAccessManager::finished,
            this, &LyricsWidget::onNetworkReply);

    scrollTimer_ = new QTimer(this);
    scrollTimer_->setInterval(16); // ~60fps
    connect(scrollTimer_, &QTimer::timeout, this, [this]() {
        double diff = targetOffset_ - scrollOffset_;
        if (std::abs(diff) < 0.5) {
            scrollOffset_ = targetOffset_;
            scrollTimer_->stop();
        } else {
            scrollOffset_ += diff * 0.15;
        }
        update();
    });

    statusText_ = "파일을 열어주세요";
}

void LyricsWidget::loadForTrack(const QString& title, const QString& artist,
                                 const QString& filePath) {
    clear();
    currentTitle_  = title;
    currentArtist_ = artist;
    statusText_    = "가사 검색 중...";
    update();

    // 1순위: 같은 폴더의 .lrc 파일
    if (!filePath.isEmpty()) {
        QFileInfo fi(filePath);
        QString lrcPath = fi.dir().filePath(fi.completeBaseName() + ".lrc");
        if (QFile::exists(lrcPath)) {
            QFile f(lrcPath);
            if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                parseLrc(QString::fromUtf8(f.readAll()));
                if (hasLyrics_) {
                    statusText_ = "[LRC] 동기화";
                    update();
                    return;
                }
            }
        }
    }

    // 2순위: 인터넷 검색 (LRCLIB)
    if (!title.isEmpty()) {
        searchOnline(title, artist);
    } else {
        statusText_ = "가사를 찾을 수 없습니다";
        update();
    }
}

void LyricsWidget::searchOnline(const QString& title, const QString& artist) {
    // LRCLIB API: https://lrclib.net/api/search
    QUrl url("https://lrclib.net/api/search");
    QUrlQuery query;
    query.addQueryItem("track_name", title);
    if (!artist.isEmpty())
        query.addQueryItem("artist_name", artist);
    url.setQuery(query);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  "Sorinuri/3.0 (https://sorinuri.com)");
    nam_->get(req);
}

void LyricsWidget::onNetworkReply(QNetworkReply* reply) {
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        statusText_ = "가사를 찾을 수 없습니다";
        update();
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);

    // LRCLIB 검색 결과 배열
    if (doc.isArray()) {
        QJsonArray arr = doc.array();
        for (const QJsonValue& v : arr) {
            QJsonObject obj = v.toObject();
            QString syncedLyrics = obj["syncedLyrics"].toString();
            if (!syncedLyrics.isEmpty()) {
                parseLrc(syncedLyrics);
                if (hasLyrics_) {
                    statusText_ = "[LRCLIB] 동기화";
                    update();
                    return;
                }
            }
            // 동기화 가사 없으면 일반 가사
            QString plainLyrics = obj["plainLyrics"].toString();
            if (!plainLyrics.isEmpty()) {
                // 타임스탬프 없는 가사
                lines_.clear();
                double t = 0;
                for (const QString& line : plainLyrics.split('\n')) {
                    lines_.append({t, line});
                    t += 3000; // 3초 간격 (동기화 없음)
                }
                hasLyrics_ = !lines_.isEmpty();
                statusText_ = "[LRCLIB] 가사";
                update();
                return;
            }
        }
    }

    statusText_ = "가사를 찾을 수 없습니다";
    update();
}

void LyricsWidget::parseLrc(const QString& lrcText) {
    lines_.clear();
    hasLyrics_ = false;

    // LRC 타임스탬프 정규식: [mm:ss.xx] 또는 [mm:ss:xx]
    static QRegularExpression re(R"(\[(\d+):(\d+)[.:](\d+)\])");

    QStringList rawLines = lrcText.split('\n');
    for (const QString& raw : rawLines) {
        QString line = raw.trimmed();
        if (line.isEmpty()) continue;

        // 메타 태그 건너뜀 ([ti:], [ar:] 등)
        if (line.startsWith("[ti:") || line.startsWith("[ar:") ||
            line.startsWith("[al:") || line.startsWith("[by:") ||
            line.startsWith("[offset:") || line.startsWith("[length:"))
            continue;

        QRegularExpressionMatchIterator it = re.globalMatch(line);
        QString text = re.match(line).hasMatch()
            ? line.mid(line.lastIndexOf(']') + 1).trimmed()
            : line;

        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            int min  = m.captured(1).toInt();
            int sec  = m.captured(2).toInt();
            int ms   = m.captured(3).toInt();
            // 밀리초 자릿수 정규화 (xx → 10ms 단위, xxx → 1ms 단위)
            if (m.captured(3).length() == 2) ms *= 10;
            double timeMs = (min * 60 + sec) * 1000.0 + ms;
            if (!text.isEmpty())
                lines_.append({timeMs, text});
        }
    }

    // 시간순 정렬
    std::sort(lines_.begin(), lines_.end(),
              [](const LrcLine& a, const LrcLine& b) { return a.timeMs < b.timeMs; });

    hasLyrics_ = !lines_.isEmpty();
}

void LyricsWidget::setPosition(double posSecs) {
    posMs_ = posSecs * 1000.0;
    int newIdx = findCurrentLine(posMs_);
    if (newIdx != currentIdx_) {
        currentIdx_ = newIdx;
        // 스크롤 목표 업데이트
        if (currentIdx_ >= 0) {
            // 현재 줄을 중앙에 위치시킴
            const int lineH = 32;
            targetOffset_ = currentIdx_ * lineH - height() / 2 + lineH / 2;
            if (!scrollTimer_->isActive())
                scrollTimer_->start();
        }
        update();
    }
}

int LyricsWidget::findCurrentLine(double posMs) const {
    if (lines_.isEmpty()) return -1;
    int idx = -1;
    for (int i = 0; i < lines_.size(); ++i) {
        if (lines_[i].timeMs <= posMs) idx = i;
        else break;
    }
    return idx;
}

void LyricsWidget::clear() {
    lines_.clear();
    currentIdx_ = -1;
    hasLyrics_  = false;
    scrollOffset_ = 0;
    targetOffset_ = 0;
    update();
}

void LyricsWidget::paintEvent(QPaintEvent* /*e*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    // 배경
    p.fillRect(rect(), QColor(13, 13, 13, 220));

    // 테두리
    p.setPen(QColor(50, 50, 50));
    p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 8, 8);

    if (!hasLyrics_) {
        // 상태 메시지 표시
        p.setPen(QColor(80, 80, 80));
        QFont f = p.font();
        f.setPixelSize(14);
        p.setFont(f);
        p.drawText(rect(), Qt::AlignCenter, statusText_);

        // 헤더
        drawHeader(p);
        return;
    }

    drawHeader(p);

    // 가사 렌더링 영역 (헤더 아래)
    QRect contentRect = rect().adjusted(0, 40, 0, 0);
    p.setClipRect(contentRect);

    const int lineH    = 32;
    const int centerY  = contentRect.top() + contentRect.height() / 2;
    const int startY   = centerY - static_cast<int>(scrollOffset_)
                         - currentIdx_ * lineH;

    for (int i = 0; i < lines_.size(); ++i) {
        int y = startY + i * lineH;
        if (y + lineH < contentRect.top() - lineH) continue;
        if (y > contentRect.bottom() + lineH) break;

        int dist = std::abs(i - currentIdx_);
        bool isCurrent = (i == currentIdx_);

        // 투명도 및 크기 계산
        float alpha;
        int   fontSize;
        if (isCurrent) {
            alpha    = 1.0f;
            fontSize = 17;
        } else if (dist == 1) {
            alpha    = 0.75f;
            fontSize = 14;
        } else if (dist == 2) {
            alpha    = 0.55f;
            fontSize = 13;
        } else if (dist == 3) {
            alpha    = 0.35f;
            fontSize = 12;
        } else {
            alpha    = 0.20f;
            fontSize = 11;
        }

        // 현재 줄 하이라이트 배경
        if (isCurrent) {
            QRect hlRect(contentRect.left() + 8, y - 2,
                         contentRect.width() - 16, lineH);
            p.fillRect(hlRect, QColor(79, 195, 247, 20));
        }

        QFont f = p.font();
        f.setPixelSize(fontSize);
        f.setBold(isCurrent);
        p.setFont(f);

        QColor textColor(255, 255, 255, static_cast<int>(alpha * 255));
        p.setPen(textColor);

        QRect lineRect(contentRect.left() + 16, y,
                       contentRect.width() - 32, lineH);
        p.drawText(lineRect, Qt::AlignVCenter | Qt::AlignHCenter,
                   lines_[i].text);
    }

    p.setClipping(false);

    // 상단/하단 페이드 그라데이션
    QLinearGradient fadeTop(0, contentRect.top(), 0, contentRect.top() + 60);
    fadeTop.setColorAt(0, QColor(13, 13, 13, 220));
    fadeTop.setColorAt(1, QColor(13, 13, 13, 0));
    p.fillRect(QRect(0, contentRect.top(), width(), 60), fadeTop);

    QLinearGradient fadeBottom(0, contentRect.bottom() - 60, 0, contentRect.bottom());
    fadeBottom.setColorAt(0, QColor(13, 13, 13, 0));
    fadeBottom.setColorAt(1, QColor(13, 13, 13, 220));
    p.fillRect(QRect(0, contentRect.bottom() - 60, width(), 60), fadeBottom);
}

void LyricsWidget::drawHeader(QPainter& p) {
    // "LYRICS" 레이블
    p.setPen(QColor(170, 170, 170));
    QFont f = p.font();
    f.setPixelSize(12);
    f.setBold(true);
    p.setFont(f);
    p.drawText(QRect(16, 10, 80, 20), Qt::AlignVCenter, "LYRICS");

    // 상태 배지
    if (!statusText_.isEmpty()) {
        QRect badgeRect(100, 12, 100, 16);
        p.fillRect(badgeRect, QColor(40, 40, 40));
        p.setPen(QColor(100, 100, 100));
        QFont bf = p.font();
        bf.setPixelSize(10);
        bf.setBold(false);
        p.setFont(bf);
        p.drawText(badgeRect, Qt::AlignCenter, statusText_);
    }
}

void LyricsWidget::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    update();
}
