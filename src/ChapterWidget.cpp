// ChapterWidget.cpp
// v4_chapter_ui.png 목업 정밀 일치 구현
//

#include "ChapterWidget.h"
#include "MpvCore.h"
#include <QVBoxLayout>
#include <QProcess>
#include <QRegularExpression>
#include <QFile>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QVariantList>
#include <QVariantMap>
#include <QFont>
#include <QFontMetrics>
#include <QToolTip>

// forward declaration
static QString formatTimeStatic(double sec);

// ═══════════════════════════════════════════════════════════════════════════
// ChapterTimeline
// ═══════════════════════════════════════════════════════════════════════════

ChapterTimeline::ChapterTimeline(QWidget* parent) : QWidget(parent) {
    setFixedHeight(60);
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
}

void ChapterTimeline::setChapters(const QVector<Chapter>& c, double dur) {
    chapters_ = c; duration_ = dur; update();
}

void ChapterTimeline::setPosition(double sec) {
    position_ = sec; update();
}

int ChapterTimeline::secToPos(double sec) const {
    if (duration_ <= 0) return 0;
    return (int)((sec / duration_) * (width() - 20)) + 10;
}

double ChapterTimeline::posToSec(int x) const {
    if (duration_ <= 0) return 0;
    return ((double)(x - 10) / (width() - 20)) * duration_;
}

void ChapterTimeline::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int W = width(), barY = 38, barH = 6;

    // ── 챕터 레이블 (타임라인 위) ──────────────────────────
    p.setFont(QFont("Malgun Gothic", 9));
    for (int i = 0; i < chapters_.size(); ++i) {
        const auto& ch = chapters_[i];
        int x = secToPos(ch.startSec);
        bool isCurrent = (i < chapters_.size()-1)
            ? (position_ >= ch.startSec && position_ < chapters_[i+1].startSec)
            : (position_ >= ch.startSec);

        // 챕터 제목
        QString title = ch.isBookmark ? formatTimeStatic(ch.startSec) : ch.title;
        p.setPen(isCurrent ? QColor(0x00,0xc8,0xb4) : QColor(0x88,0x88,0x88));
        QRect tr(x-40, 2, 80, 14);
        p.drawText(tr, Qt::AlignCenter, title);

        // 시간
        p.setPen(isCurrent ? QColor(0x00,0xc8,0xb4) : QColor(0x66,0x66,0x66));
        QRect timeR(x-30, 16, 60, 12);
        p.drawText(timeR, Qt::AlignCenter, formatTimeStatic(ch.startSec));
    }

    // ── 타임라인 바 (진행 부분) ────────────────────────────
    int playedX = secToPos(position_);

    // 전체 바 배경
    p.fillRect(10, barY, W-20, barH, QColor(0x2a,0x2a,0x2a));

    // 진행된 부분 (청록)
    p.fillRect(10, barY, playedX-10, barH, QColor(0x00,0xc8,0xb4));

    // 챕터 구분 마커
    for (int i = 0; i < chapters_.size(); ++i) {
        const auto& ch = chapters_[i];
        int x = secToPos(ch.startSec);
        bool isBm = ch.isBookmark;

        if (isBm) {
            // 북마크: 노란 다이아몬드
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0xf0,0xc0,0x40));
            QPolygon diamond;
            diamond << QPoint(x, barY-6) << QPoint(x+6, barY)
                    << QPoint(x, barY+6) << QPoint(x-6, barY);
            p.drawPolygon(diamond);
        } else {
            // 챕터: 흰색 원점
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0xcc,0xcc,0xcc));
            p.drawEllipse(QPoint(x, barY+barH/2), 5, 5);
        }
    }

    // 현재 위치 인디케이터 (청록 수직선 + 원)
    p.setPen(QPen(QColor(0x00,0xc8,0xb4), 2));
    p.drawLine(playedX, barY-4, playedX, barY+barH+4);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x00,0xc8,0xb4));
    p.drawEllipse(QPoint(playedX, barY+barH/2), 7, 7);

    // ── 호버 썸네일 팝업 ──────────────────────────────────
    if (hoverX_ >= 0 && hoverChapter_ >= 0 && hoverChapter_ < chapters_.size()) {
        const auto& ch = chapters_[hoverChapter_];
        int px = hoverX_;
        int popW = 140, popH = 90;
        int popX = qBound(10, px - popW/2, W - popX - 10);
        int popY = barY - popH - 14;

        // 팝업 배경
        QPainterPath path;
        path.addRoundedRect(popX, popY, popW, popH, 6, 6);
        p.fillPath(path, QColor(0x1a,0x1a,0x1a,230));
        p.setPen(QPen(QColor(0x44,0x44,0x44), 1));
        p.drawPath(path);

        // 썸네일
        if (!ch.thumbnail.isNull()) {
            p.drawPixmap(popX+4, popY+4, popW-8, 60, ch.thumbnail);
        } else {
            p.fillRect(popX+4, popY+4, popW-8, 60, QColor(0x2a,0x2a,0x2a));
        }

        // 시간 + 제목
        p.setFont(QFont("Malgun Gothic", 9));
        p.setPen(QColor(0xcc,0xcc,0xcc));
        p.drawText(popX+4, popY+66, popW-8, 20, Qt::AlignCenter,
                   formatTimeStatic(ch.startSec) + "  " + ch.title);

        // 화살표
        QPolygon arrow;
        arrow << QPoint(px-6, popY+popH) << QPoint(px+6, popY+popH) << QPoint(px, popY+popH+8);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0x1a,0x1a,0x1a,230));
        p.drawPolygon(arrow);
    }
}

void ChapterTimeline::mousePressEvent(QMouseEvent* e) {
    double sec = posToSec(e->pos().x());
    emit seekRequested(qBound(0.0, sec, duration_));
}

void ChapterTimeline::mouseMoveEvent(QMouseEvent* e) {
    hoverX_ = e->pos().x();
    double sec = posToSec(hoverX_);
    hoverChapter_ = -1;
    // 가장 가까운 챕터 찾기
    for (int i = chapters_.size()-1; i >= 0; --i) {
        if (sec >= chapters_[i].startSec) { hoverChapter_ = i; break; }
    }
    update();
}

void ChapterTimeline::leaveEvent(QEvent*) {
    hoverX_ = -1; hoverChapter_ = -1; update();
}

static QString formatTimeStatic(double sec) {
    int h=(int)sec/3600, m=((int)sec%3600)/60, s=(int)sec%60;
    if (h>0) return QString("%1:%2:%3").arg(h).arg(m,2,10,QChar('0')).arg(s,2,10,QChar('0'));
    return QString("%1:%2").arg(m,2,10,QChar('0')).arg(s,2,10,QChar('0'));
}

// ═══════════════════════════════════════════════════════════════════════════
// ThumbnailStrip
// ═══════════════════════════════════════════════════════════════════════════

ThumbnailStrip::ThumbnailStrip(QWidget* parent) : QWidget(parent) {
    setFixedHeight(THUMB_H + 28);
    setCursor(Qt::PointingHandCursor);
}

void ThumbnailStrip::setChapters(const QVector<Chapter>& c) {
    chapters_ = c; update();
}

void ThumbnailStrip::setCurrentChapter(int idx) {
    currentIdx_ = idx; update();
}

void ThumbnailStrip::paintEvent(QPaintEvent*) {
    if (chapters_.isEmpty()) return;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int n = chapters_.size();
    int spacing = 8;
    int totalW = n * THUMB_W + (n-1) * spacing;
    int startX = (width() - totalW) / 2;

    for (int i = 0; i < n; ++i) {
        const auto& ch = chapters_[i];
        int x = startX + i * (THUMB_W + spacing);
        bool isCur = (i == currentIdx_);

        // 썸네일 배경
        QRect thumbR(x, 0, THUMB_W, THUMB_H);
        if (!ch.thumbnail.isNull()) {
            p.drawPixmap(thumbR, ch.thumbnail);
        } else {
            p.fillRect(thumbR, QColor(0x22,0x22,0x22));
        }

        // 선택된 챕터: 청록 테두리
        if (isCur) {
            p.setPen(QPen(QColor(0x00,0xc8,0xb4), 2));
            p.setBrush(Qt::NoBrush);
            p.drawRect(thumbR.adjusted(1,1,-1,-1));
        }

        // 시간 + 제목 레이블
        p.setFont(QFont("Malgun Gothic", 9));
        bool isBm = ch.isBookmark;
        p.setPen(isCur ? QColor(0x00,0xc8,0xb4) : (isBm ? QColor(0xf0,0xc0,0x40) : QColor(0x88,0x88,0x88)));
        QString label = formatTimeStatic(ch.startSec) + "  " + ch.title;
        p.drawText(QRect(x, THUMB_H+4, THUMB_W, 20), Qt::AlignCenter, label);
    }
}

void ThumbnailStrip::mousePressEvent(QMouseEvent* e) {
    int n = chapters_.size();
    if (n == 0) return;
    int spacing = 8;
    int totalW = n * THUMB_W + (n-1) * spacing;
    int startX = (width() - totalW) / 2;
    for (int i = 0; i < n; ++i) {
        int x = startX + i * (THUMB_W + spacing);
        if (e->pos().x() >= x && e->pos().x() < x + THUMB_W) {
            emit chapterClicked(i); return;
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// ChapterWidget (메인)
// ═══════════════════════════════════════════════════════════════════════════

static const char* CW_STYLE = R"(
QWidget { background-color: #141414; color: #cccccc; font-family: 'Malgun Gothic'; }
QLabel#hdr { color: #00c8b4; font-size: 14px; font-weight: bold; }
QLabel#cnt { color: #555; font-size: 11px; background: #1e1e1e; border: 1px solid #333; border-radius: 10px; padding: 2px 8px; }
QListWidget {
    background: #0e0e0e; border: none;
    color: #cccccc; font-size: 12px; outline: none;
}
QListWidget::item { padding: 8px 12px; border-bottom: 1px solid #1a1a1a; min-height: 28px; }
QListWidget::item:selected { background: #00c8b4; color: #000; }
QListWidget::item:hover:!selected { background: #1a1a1a; }
QPushButton#btnAdd {
    background: transparent; border: 1px solid #00c8b4; border-radius: 4px;
    color: #00c8b4; font-size: 12px; padding: 6px 12px; min-height: 32px;
}
QPushButton#btnAdd:hover { background: #0a2a28; }
QPushButton#btnDel {
    background: transparent; border: 1px solid #cc3333; border-radius: 4px;
    color: #cc3333; font-size: 12px; padding: 6px 12px; min-height: 32px;
}
QPushButton#btnDel:hover { background: #2a0a0a; }
QPushButton#btnDel:disabled { border-color: #333; color: #444; }
QLabel#hint { color: #444; font-size: 10px; }
)";

ChapterWidget::ChapterWidget(MpvCore* core, QWidget* parent)
    : QWidget(parent), core_(core)
{
    setStyleSheet(CW_STYLE);
    buildUI();
}

void ChapterWidget::buildUI() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── 타임라인 영역 (영상 아래, 전체 너비) ─────────────
    timeline_ = new ChapterTimeline(this);
    root->addWidget(timeline_);

    // ── 썸네일 스트립 ─────────────────────────────────────
    thumbStrip_ = new ThumbnailStrip(this);
    root->addWidget(thumbStrip_);

    // ── 우측 패널 (챕터 목록 + 버튼) ─────────────────────
    // 실제 레이아웃에서 ChapterWidget은 우측 패널에 배치됨
    auto* panelRoot = new QVBoxLayout;
    panelRoot->setContentsMargins(16, 14, 16, 14);
    panelRoot->setSpacing(0);

    // 헤더
    auto* rowHdr = new QHBoxLayout;
    auto* lblHdr = new QLabel("📚  챕터 & 북마크");
    lblHdr->setObjectName("hdr");
    lblCount_ = new QLabel("챕터 0개");
    lblCount_->setObjectName("cnt");
    rowHdr->addWidget(lblHdr);
    rowHdr->addStretch();
    rowHdr->addWidget(lblCount_);
    panelRoot->addLayout(rowHdr);
    panelRoot->addSpacing(10);

    // 챕터 목록
    lstChapters_ = new QListWidget;
    lstChapters_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    panelRoot->addWidget(lstChapters_);
    panelRoot->addSpacing(10);

    // 버튼 행
    auto* rowBtns = new QHBoxLayout;
    rowBtns->setSpacing(8);
    btnAddBm_ = new QPushButton("⊕  현재 위치 북마크");
    btnAddBm_->setObjectName("btnAdd");
    btnRemoveBm_ = new QPushButton("✕  북마크 삭제");
    btnRemoveBm_->setObjectName("btnDel");
    btnRemoveBm_->setEnabled(false);
    rowBtns->addWidget(btnAddBm_);
    rowBtns->addWidget(btnRemoveBm_);
    panelRoot->addLayout(rowBtns);
    panelRoot->addSpacing(8);

    // AI 장면 감지 버튼
    btnDetect_ = new QPushButton("🤖  AI 장면 자동 감지");
    btnDetect_->setObjectName("btnDetect");
    btnDetect_->setToolTip("ffmpeg으로 영상의 장면 전환을 자동 감지하여 챕터를 생성합니다");
    panelRoot->addSpacing(4);
    panelRoot->addWidget(btnDetect_);
    panelRoot->addSpacing(8);

    // 단축키 힌트
    auto* lblHint = new QLabel("단축키: B 북마크 추가  |  [ ] 챕터 이동");
    lblHint->setObjectName("hint");
    panelRoot->addWidget(lblHint);
    panelRoot->addStretch();

    root->addLayout(panelRoot);

    // 시그널 연결
    connect(timeline_, &ChapterTimeline::seekRequested, this, &ChapterWidget::seekRequested);
    connect(thumbStrip_, &ThumbnailStrip::chapterClicked, this, &ChapterWidget::jumpToChapter);
    connect(lstChapters_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item){
        jumpToChapter(lstChapters_->row(item));
    });
    connect(lstChapters_, &QListWidget::currentRowChanged, this, [this](int row){
        btnRemoveBm_->setEnabled(row >= 0 && row < chapters_.size() && chapters_[row].isBookmark);
    });
    connect(btnAddBm_, &QPushButton::clicked, this, [this](){
        addBookmark(currentPos_);
    });
    connect(btnRemoveBm_, &QPushButton::clicked, this, [this](){
        int row = lstChapters_->currentRow();
        if (row >= 0) removeBookmark(row);
    });
    connect(btnDetect_, &QPushButton::clicked, this, [this](){
        if (!mediaPath_.isEmpty()) detectScenes(mediaPath_);
    });
}

void ChapterWidget::setDuration(double sec) {
    duration_ = sec;
    timeline_->setChapters(chapters_, duration_);
}

void ChapterWidget::loadChapters() {
    if (!core_) return;
    QVariant v = core_->getProperty("chapter-list");
    QVariantList list = v.toList();

    // 북마크 유지, 챕터만 교체
    chapters_.erase(std::remove_if(chapters_.begin(), chapters_.end(),
        [](const Chapter& c){ return !c.isBookmark; }), chapters_.end());

    for (const QVariant& item : list) {
        QVariantMap m = item.toMap();
        Chapter ch;
        ch.startSec = m.value("time", 0.0).toDouble();
        ch.title    = m.value("title", QString("챕터 %1").arg(chapters_.size()+1)).toString();
        chapters_.append(ch);
    }
    std::sort(chapters_.begin(), chapters_.end(),
        [](const Chapter& a, const Chapter& b){ return a.startSec < b.startSec; });

    refreshList();
    refreshTimeline();
    emit chaptersChanged();
}

void ChapterWidget::addBookmark(double sec) {
    Chapter bm;
    bm.startSec   = sec;
    bm.title      = "북마크 " + formatTimeStatic(sec);
    bm.isBookmark = true;
    chapters_.append(bm);
    std::sort(chapters_.begin(), chapters_.end(),
        [](const Chapter& a, const Chapter& b){ return a.startSec < b.startSec; });
    refreshList();
    refreshTimeline();
    emit chaptersChanged();
}

void ChapterWidget::removeBookmark(int idx) {
    if (idx < 0 || idx >= chapters_.size() || !chapters_[idx].isBookmark) return;
    chapters_.removeAt(idx);
    refreshList();
    refreshTimeline();
    emit chaptersChanged();
}

void ChapterWidget::jumpToChapter(int idx) {
    if (idx < 0 || idx >= chapters_.size()) return;
    emit seekRequested(chapters_[idx].startSec);
}

void ChapterWidget::onPositionChanged(double sec) {
    currentPos_ = sec;
    timeline_->setPosition(sec);

    // 현재 챕터 인덱스 계산
    int cur = -1;
    for (int i = chapters_.size()-1; i >= 0; --i) {
        if (sec >= chapters_[i].startSec) { cur = i; break; }
    }
    thumbStrip_->setCurrentChapter(cur);
}

void ChapterWidget::generateThumbnails(const QString& filePath) {
    mediaPath_ = filePath;
}

void ChapterWidget::refreshList() {
    lstChapters_->clear();
    for (const Chapter& ch : chapters_) {
        QString icon = ch.isBookmark ? "🔖" : "▶";
        QString text = QString("%1  %2  %3").arg(icon).arg(formatTimeStatic(ch.startSec)).arg(ch.title);
        auto* item = new QListWidgetItem(text);
        if (ch.isBookmark) item->setForeground(QColor("#f0c040"));
        lstChapters_->addItem(item);
    }
    lblCount_->setText(QString("챕터 %1개").arg(chapters_.size()));
}

void ChapterWidget::refreshTimeline() {
    timeline_->setChapters(chapters_, duration_);
    thumbStrip_->setChapters(chapters_);
}

QString ChapterWidget::formatTime(double sec) const {
    return formatTimeStatic(sec);
}

// ─── AI 장면 전환 감지 ────────────────────────────────────────────────────────
void ChapterWidget::detectScenes(const QString& filePath) {
    if (sceneProcess_ && sceneProcess_->state() != QProcess::NotRunning) {
        sceneProcess_->kill();
    }

    // ffmpeg.exe 경로: 앱 디렉토리 또는 PATH에서 탐색
    QString ffmpegPath = QCoreApplication::applicationDirPath() + "/ffmpeg.exe";
    if (!QFile::exists(ffmpegPath)) ffmpegPath = "ffmpeg";

    if (btnDetect_) {
        btnDetect_->setEnabled(false);
        btnDetect_->setText("🔍  분석 중...");
    }

    // ffmpeg -vf "select='gt(scene,0.35)',showinfo" 로 장면 전환 타임코드 추출
    // 출력 예: [Parsed_showinfo_1 @ ...] n:0 pts:0 pts_time:0.000 ...
    sceneProcess_ = new QProcess(this);
    sceneProcess_->setProcessChannelMode(QProcess::MergedChannels);
    connect(sceneProcess_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ChapterWidget::onSceneDetectFinished);

    QStringList args;
    args << "-i" << filePath
         << "-vf" << "select='gt(scene,0.35)',showinfo"
         << "-vsync" << "vfr"
         << "-f" << "null"
         << "-";
    sceneProcess_->start(ffmpegPath, args);
}

void ChapterWidget::onSceneDetectFinished(int exitCode, QProcess::ExitStatus /*status*/) {
    Q_UNUSED(exitCode)

    if (btnDetect_) {
        btnDetect_->setEnabled(true);
        btnDetect_->setText("🤖  AI 장면 자동 감지");
    }

    if (!sceneProcess_) return;
    QString output = QString::fromLocal8Bit(sceneProcess_->readAllStandardOutput());
    sceneProcess_->deleteLater();
    sceneProcess_ = nullptr;

    // pts_time:숫자 패턴으로 타임코드 추출
    QRegularExpression re(R"(pts_time:(\d+\.?\d*))");
    QRegularExpressionMatchIterator it = re.globalMatch(output);

    // 기존 AI 감지 챕터 제거 (북마크는 유지)
    chapters_.erase(std::remove_if(chapters_.begin(), chapters_.end(),
        [](const Chapter& c){ return !c.isBookmark && c.title.startsWith("장면 "); }), chapters_.end());

    int sceneIdx = 1;
    double lastSec = -5.0;  // 최소 5초 간격 필터
    while (it.hasNext()) {
        QRegularExpressionMatch m = it.next();
        double sec = m.captured(1).toDouble();
        if (sec - lastSec < 5.0) continue;  // 너무 짧은 장면 제외
        lastSec = sec;
        Chapter ch;
        ch.startSec = sec;
        ch.title    = QString("장면 %1").arg(sceneIdx++);
        chapters_.append(ch);
    }

    if (chapters_.isEmpty()) {
        // 장면 감지 결과 없으면 균등 분할 (10분 단위)
        if (duration_ > 0) {
            double interval = 600.0;
            for (double t = interval; t < duration_; t += interval) {
                Chapter ch;
                ch.startSec = t;
                ch.title    = QString("장면 %1").arg(sceneIdx++);
                chapters_.append(ch);
            }
        }
    }

    std::sort(chapters_.begin(), chapters_.end(),
        [](const Chapter& a, const Chapter& b){ return a.startSec < b.startSec; });

    refreshList();
    refreshTimeline();
    emit chaptersChanged();
}
