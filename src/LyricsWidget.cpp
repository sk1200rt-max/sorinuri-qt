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

// ─── 생성자 ─────────────────────────────────────────────────────────────────
LyricsWidget::LyricsWidget(QWidget* parent) : QWidget(parent) {
    setAutoFillBackground(false);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    // 마우스 이동 이벤트를 MainWindow까지 전달하여 커서 숨김 방지
    setMouseTracking(true);

    nam_ = new QNetworkAccessManager(this);
    connect(nam_, &QNetworkAccessManager::finished,
            this, &LyricsWidget::onNetworkReply);

    // 가사 싱크 스크롤 타이머 (~60fps)
    scrollTimer_ = new QTimer(this);
    scrollTimer_->setInterval(16);
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

    // AI 아이콘 점멸 타이머 (500ms 간격)
    aiBlinkTimer_ = new QTimer(this);
    aiBlinkTimer_->setInterval(500);
    connect(aiBlinkTimer_, &QTimer::timeout, this, &LyricsWidget::onAiBlinkTick);

    statusText_ = "파일을 열어주세요";
}

// ─── AI 아이콘 점멸 ──────────────────────────────────────────────────────────
void LyricsWidget::onAiBlinkTick() {
    aiBlinkOn_ = !aiBlinkOn_;
    aiIconAlpha_ = aiBlinkOn_ ? 1.0f : 0.4f;
    update();
}

void LyricsWidget::setSearchState(LyricsSearchState state) {
    searchState_ = state;
    switch (state) {
    case LyricsSearchState::Searching:
        aiBlinkOn_    = true;
        aiIconAlpha_  = 1.0f;
        aiBlinkTimer_->start();
        break;
    case LyricsSearchState::Found:
    case LyricsSearchState::LocalFile:
        aiBlinkTimer_->stop();
        aiIconAlpha_ = 1.0f;
        break;
    case LyricsSearchState::NotFound:
    case LyricsSearchState::Idle:
        aiBlinkTimer_->stop();
        aiIconAlpha_ = 0.0f;
        break;
    }
    update();
}

// ─── 가사 로드 진입점 ────────────────────────────────────────────────────────
void LyricsWidget::loadForTrack(const QString& title, const QString& artist,
                                 const QString& filePath,
                                 double durationSecs,
                                 const QString& album) {
    clear();
    currentTitle_    = title;
    currentArtist_   = artist;
    currentAlbum_    = album;
    currentDuration_ = durationSecs;
    searchStep_      = 0;

    // ── 1순위: 같은 폴더의 .lrc 파일 ──────────────────────────────────────
    if (!filePath.isEmpty()) {
        QFileInfo fi(filePath);
        QString lrcPath = fi.dir().filePath(fi.completeBaseName() + ".lrc");
        if (QFile::exists(lrcPath)) {
            QFile f(lrcPath);
            if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                parseLrc(QString::fromUtf8(f.readAll()));
                if (hasLyrics_) {
                    statusText_ = "LRC 파일";
                    setSearchState(LyricsSearchState::LocalFile);
                    return;
                }
            }
        }
    }

    if (title.isEmpty()) {
        statusText_ = "가사를 찾을 수 없습니다";
        setSearchState(LyricsSearchState::NotFound);
        update();
        return;
    }

    // ── 2순위: LRCLIB /api/get (duration 기반 정확 매칭) ──────────────────
    statusText_ = "AI 가사 검색 중...";
    setSearchState(LyricsSearchState::Searching);
    update();

    if (durationSecs > 0) {
        searchStep_ = 1;
        searchBySignature(title, artist, album, durationSecs);
    } else {
        // duration 없으면 바로 검색 API로
        searchStep_ = 2;
        searchOnline(title, artist);
    }
}

// ─── LRCLIB /api/get (정확 매칭) ────────────────────────────────────────────
void LyricsWidget::searchBySignature(const QString& title, const QString& artist,
                                      const QString& album, double durationSecs) {
    QUrl url("https://lrclib.net/api/get");
    QUrlQuery query;
    query.addQueryItem("track_name", title);
    if (!artist.isEmpty())
        query.addQueryItem("artist_name", artist);
    if (!album.isEmpty())
        query.addQueryItem("album_name", album);
    query.addQueryItem("duration", QString::number(qRound(durationSecs)));
    url.setQuery(query);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  "Sorinuri/6.4.0 (https://sorinuri.com)");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    nam_->get(req);
}

// ─── LRCLIB /api/search (폴백) ───────────────────────────────────────────────
void LyricsWidget::searchOnline(const QString& title, const QString& artist) {
    QUrl url("https://lrclib.net/api/search");
    QUrlQuery query;
    query.addQueryItem("track_name", title);
    if (!artist.isEmpty())
        query.addQueryItem("artist_name", artist);
    url.setQuery(query);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  "Sorinuri/6.4.0 (https://sorinuri.com)");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    nam_->get(req);
}

// ─── 네트워크 응답 처리 ──────────────────────────────────────────────────────
void LyricsWidget::onNetworkReply(QNetworkReply* reply) {
    reply->deleteLater();

    // 네트워크 오류
    if (reply->error() != QNetworkReply::NoError) {
        if (searchStep_ == 1) {
            // /api/get 실패 → /api/search 폴백
            searchStep_ = 2;
            searchOnline(currentTitle_, currentArtist_);
        } else {
            statusText_ = "가사를 찾을 수 없습니다";
            setSearchState(LyricsSearchState::NotFound);
            update();
        }
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);

    // ── /api/get 응답: 단일 객체 ──────────────────────────────────────────
    if (searchStep_ == 1 && doc.isObject()) {
        QJsonObject obj = doc.object();

        // 404 에러 객체 처리
        if (obj.contains("code") && obj["code"].toInt() == 404) {
            // /api/search 폴백
            searchStep_ = 2;
            searchOnline(currentTitle_, currentArtist_);
            return;
        }

        // instrumental 트랙
        if (obj["instrumental"].toBool()) {
            statusText_ = "연주곡 (가사 없음)";
            setSearchState(LyricsSearchState::NotFound);
            update();
            return;
        }

        QString syncedLyrics = obj["syncedLyrics"].toString();
        if (!syncedLyrics.isEmpty()) {
            parseLrc(syncedLyrics);
            if (hasLyrics_) {
                statusText_ = "AI 가사 ✦";
                setSearchState(LyricsSearchState::Found);
                update();
                return;
            }
        }

        QString plainLyrics = obj["plainLyrics"].toString();
        if (!plainLyrics.isEmpty()) {
            lines_.clear();
            double t = 0;
            for (const QString& line : plainLyrics.split('\n')) {
                if (!line.trimmed().isEmpty())
                    lines_.append({t, line.trimmed()});
                t += 3000;
            }
            hasLyrics_ = !lines_.isEmpty();
            if (hasLyrics_) {
                statusText_ = "AI 가사 (비동기화)";
                setSearchState(LyricsSearchState::Found);
                update();
                return;
            }
        }

        // 가사 없음 → /api/search 폴백
        searchStep_ = 2;
        searchOnline(currentTitle_, currentArtist_);
        return;
    }

    // ── /api/search 응답: 배열 ────────────────────────────────────────────
    if (doc.isArray()) {
        QJsonArray arr = doc.array();
        for (const QJsonValue& v : arr) {
            QJsonObject obj = v.toObject();

            if (obj["instrumental"].toBool()) continue;

            QString syncedLyrics = obj["syncedLyrics"].toString();
            if (!syncedLyrics.isEmpty()) {
                parseLrc(syncedLyrics);
                if (hasLyrics_) {
                    statusText_ = "AI 가사 ✦";
                    setSearchState(LyricsSearchState::Found);
                    update();
                    return;
                }
            }

            QString plainLyrics = obj["plainLyrics"].toString();
            if (!plainLyrics.isEmpty()) {
                lines_.clear();
                double t = 0;
                for (const QString& line : plainLyrics.split('\n')) {
                    if (!line.trimmed().isEmpty())
                        lines_.append({t, line.trimmed()});
                    t += 3000;
                }
                hasLyrics_ = !lines_.isEmpty();
                if (hasLyrics_) {
                    statusText_ = "AI 가사 (비동기화)";
                    setSearchState(LyricsSearchState::Found);
                    update();
                    return;
                }
            }
        }
    }

    statusText_ = "가사를 찾을 수 없습니다";
    setSearchState(LyricsSearchState::NotFound);
    update();
}

// ─── LRC 파싱 ────────────────────────────────────────────────────────────────
void LyricsWidget::parseLrc(const QString& lrcText) {
    lines_.clear();
    hasLyrics_ = false;

    static QRegularExpression re(R"(\[(\d+):(\d+)[.:](\d+)\])");

    QStringList rawLines = lrcText.split('\n');
    for (const QString& raw : rawLines) {
        QString line = raw.trimmed();
        if (line.isEmpty()) continue;

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
            if (m.captured(3).length() == 2) ms *= 10;
            double timeMs = (min * 60 + sec) * 1000.0 + ms;
            if (!text.isEmpty())
                lines_.append({timeMs, text});
        }
    }

    std::sort(lines_.begin(), lines_.end(),
              [](const LrcLine& a, const LrcLine& b) { return a.timeMs < b.timeMs; });

    hasLyrics_ = !lines_.isEmpty();
}

// ─── 재생 위치 업데이트 ──────────────────────────────────────────────────────
void LyricsWidget::setPosition(double posSecs) {
    posMs_ = posSecs * 1000.0;
    int newIdx = findCurrentLine(posMs_);
    if (newIdx != currentIdx_) {
        currentIdx_ = newIdx;
        if (currentIdx_ >= 0) {
            // targetOffset_: 현재 라인의 절대 Y 위치 (lineH 단위)
            // paintEvent에서 startY = centerY - scrollOffset_ 로 계산하면
            // 현재 라인이 contentRect 중앙에 정확히 위치함
            const int lineH = 36;
            targetOffset_ = currentIdx_ * lineH + lineH / 2;
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

// ─── 가사 초기화 ─────────────────────────────────────────────────────────────
void LyricsWidget::clear() {
    lines_.clear();
    currentIdx_   = -1;
    hasLyrics_    = false;
    scrollOffset_ = 0;
    targetOffset_ = 0;
    searchStep_   = 0;
    setSearchState(LyricsSearchState::Idle);
    statusText_   = "파일을 열어주세요";
    update();
}

// ─── 렌더링 ──────────────────────────────────────────────────────────────────
void LyricsWidget::paintEvent(QPaintEvent* /*e*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    // 배경
    p.fillRect(rect(), QColor(13, 13, 13, 220));

    // 테두리
    p.setPen(QColor(50, 50, 50));
    p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 8, 8);

    // 헤더 (항상 먼저 그림)
    drawHeader(p);

    if (!hasLyrics_) {
        // 상태 메시지
        p.setPen(QColor(80, 80, 80));
        QFont f = p.font();
        f.setPixelSize(14);
        p.setFont(f);
        p.drawText(rect().adjusted(0, 44, 0, 0), Qt::AlignCenter, statusText_);
        return;
    }

    // 가사 렌더링 영역 (헤더 44px 아래)
    QRect contentRect = rect().adjusted(0, 44, 0, 0);
    p.setClipRect(contentRect);

    const int lineH   = 36;
    // centerY: contentRect 중앙 Y 좌표
    // scrollOffset_은 현재 라인의 중앙점 Y 위치 (lineH 단위)
    // startY = centerY - scrollOffset_: 이렇게 하면 라인 0의 Y = centerY - scrollOffset_
    // 현재 라인 i의 Y = startY + i * lineH
    // 현재 라인(currentIdx_)의 Y = startY + currentIdx_ * lineH
    //   = centerY - scrollOffset_ + currentIdx_ * lineH
    //   = centerY - (currentIdx_ * lineH + lineH/2) + currentIdx_ * lineH
    //   = centerY - lineH/2  (중앙에 정확히 위치)
    const int centerY = contentRect.top() + contentRect.height() / 2;
    const int startY  = centerY - static_cast<int>(scrollOffset_);

    for (int i = 0; i < lines_.size(); ++i) {
        int y = startY + i * lineH;
        if (y + lineH < contentRect.top() - lineH) continue;
        if (y > contentRect.bottom() + lineH) break;

        int  dist      = std::abs(i - currentIdx_);
        bool isCurrent = (i == currentIdx_);

        float alpha;
        int   fontSize;
        if (isCurrent) {
            alpha = 1.0f; fontSize = 18;
        } else if (dist == 1) {
            alpha = 0.70f; fontSize = 15;
        } else if (dist == 2) {
            alpha = 0.50f; fontSize = 13;
        } else if (dist == 3) {
            alpha = 0.30f; fontSize = 12;
        } else {
            alpha = 0.18f; fontSize = 11;
        }

        // 현재 줄 하이라이트
        if (isCurrent) {
            QRect hlRect(contentRect.left() + 8, y,
                         contentRect.width() - 16, lineH);
            p.fillRect(hlRect, QColor(0, 200, 180, 22));
        }

        QFont f = p.font();
        f.setPixelSize(fontSize);
        f.setBold(isCurrent);
        p.setFont(f);
        p.setPen(QColor(255, 255, 255, static_cast<int>(alpha * 255)));

        QRect lineRect(contentRect.left() + 16, y,
                       contentRect.width() - 32, lineH);
        p.drawText(lineRect, Qt::AlignVCenter | Qt::AlignHCenter,
                   lines_[i].text);
    }

    p.setClipping(false);

    // 상단/하단 페이드 그라데이션
    QLinearGradient fadeTop(0, contentRect.top(), 0, contentRect.top() + 56);
    fadeTop.setColorAt(0, QColor(13, 13, 13, 220));
    fadeTop.setColorAt(1, QColor(13, 13, 13, 0));
    p.fillRect(QRect(0, contentRect.top(), width(), 56), fadeTop);

    QLinearGradient fadeBottom(0, contentRect.bottom() - 56, 0, contentRect.bottom());
    fadeBottom.setColorAt(0, QColor(13, 13, 13, 0));
    fadeBottom.setColorAt(1, QColor(13, 13, 13, 220));
    p.fillRect(QRect(0, contentRect.bottom() - 56, width(), 56), fadeBottom);
}

void LyricsWidget::drawHeader(QPainter& p) {
    // "LYRICS" 레이블
    p.setPen(QColor(170, 170, 170));
    QFont f = p.font();
    f.setPixelSize(12);
    f.setBold(true);
    p.setFont(f);
    p.drawText(QRect(16, 12, 60, 20), Qt::AlignVCenter, "LYRICS");

    // AI 아이콘 및 상태 배지
    if (searchState_ == LyricsSearchState::Searching) {
        // AI 검색 중: 점멸하는 청록색 배지
        int alpha = static_cast<int>(aiIconAlpha_ * 255);
        QRect badgeRect(84, 10, 130, 20);
        p.fillRect(badgeRect, QColor(0, 180, 160, static_cast<int>(aiIconAlpha_ * 60)));

        // AI 아이콘 (별 모양 점)
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 200, 180, alpha));
        p.drawEllipse(QPoint(92, 20), 4, 4);

        p.setPen(QColor(0, 200, 180, alpha));
        QFont bf = p.font();
        bf.setPixelSize(10);
        bf.setBold(false);
        p.setFont(bf);
        p.drawText(QRect(100, 10, 110, 20), Qt::AlignVCenter, "AI 가사 검색 중...");

    } else if (searchState_ == LyricsSearchState::Found) {
        // 가사 찾음: 초록색 AI 배지
        QRect badgeRect(84, 10, 90, 20);
        p.fillRect(badgeRect, QColor(0, 180, 100, 40));

        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 220, 120));
        p.drawEllipse(QPoint(92, 20), 3, 3);

        p.setPen(QColor(0, 220, 120));
        QFont bf = p.font();
        bf.setPixelSize(10);
        bf.setBold(false);
        p.setFont(bf);
        p.drawText(QRect(100, 10, 80, 20), Qt::AlignVCenter, statusText_);

    } else if (searchState_ == LyricsSearchState::LocalFile) {
        // 로컬 LRC 파일
        p.setPen(QColor(100, 160, 255));
        QFont bf = p.font();
        bf.setPixelSize(10);
        bf.setBold(false);
        p.setFont(bf);
        p.drawText(QRect(84, 10, 100, 20), Qt::AlignVCenter, "📄 " + statusText_);

    } else if (searchState_ == LyricsSearchState::NotFound) {
        // 가사 없음
        p.setPen(QColor(80, 80, 80));
        QFont bf = p.font();
        bf.setPixelSize(10);
        bf.setBold(false);
        p.setFont(bf);
        p.drawText(QRect(84, 10, 160, 20), Qt::AlignVCenter, statusText_);
    }
}

void LyricsWidget::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    update();
}
