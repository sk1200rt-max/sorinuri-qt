#include "ChapterWidget.h"
#include "MpvCore.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QListWidgetItem>
#include <QProcess>
#include <QDir>
#include <QStandardPaths>
#include <QFont>
#include <QIcon>
#include <QVariantList>
#include <QVariantMap>

static const char* CHAPTER_STYLE = R"(
QWidget { background: transparent; color: #cccccc; font-family: 'Malgun Gothic'; }
QLabel#lblSection { color: #00c8b4; font-weight: bold; font-size: 11px; }
QLabel#lblCount { color: #666; font-size: 10px; }
QListWidget {
    background: #141414; border: 1px solid #2a2a2a; border-radius: 4px;
    color: #cccccc; font-size: 11px;
}
QListWidget::item {
    padding: 5px 8px; border-bottom: 1px solid #1e1e1e;
}
QListWidget::item:selected {
    background: #1a3a3a; color: #00c8b4;
}
QListWidget::item:hover {
    background: #1e1e1e;
}
QPushButton#btnBm {
    background: #1e1e1e; border: 1px solid #333; border-radius: 4px;
    color: #aaa; font-size: 10px; padding: 4px 10px;
}
QPushButton#btnBm:hover { border-color: #00c8b4; color: #00c8b4; }
QPushButton#btnBmRemove {
    background: #1e1e1e; border: 1px solid #333; border-radius: 4px;
    color: #cc4444; font-size: 10px; padding: 4px 10px;
}
QPushButton#btnBmRemove:hover { border-color: #cc4444; }
)";

ChapterWidget::ChapterWidget(MpvCore* core, QWidget* parent)
    : QWidget(parent), core_(core)
{
    setStyleSheet(CHAPTER_STYLE);
    buildUI();
}

void ChapterWidget::buildUI() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 8, 12, 8);
    root->setSpacing(6);

    // ── 제목 + 챕터 수 ────────────────────────────────────
    auto* rowTop = new QHBoxLayout;
    auto* lblTitle = new QLabel("챕터 & 북마크");
    lblTitle->setObjectName("lblSection");
    QFont f = lblTitle->font(); f.setPointSize(11); f.setBold(true);
    lblTitle->setFont(f);

    lblCount_ = new QLabel("챕터 0개");
    lblCount_->setObjectName("lblCount");
    lblCount_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    rowTop->addWidget(lblTitle);
    rowTop->addStretch();
    rowTop->addWidget(lblCount_);
    root->addLayout(rowTop);

    // ── 챕터 목록 ─────────────────────────────────────────
    lstChapters_ = new QListWidget;
    root->addWidget(lstChapters_);

    // ── 북마크 버튼 ───────────────────────────────────────
    auto* rowBtns = new QHBoxLayout;
    btnAddBm_ = new QPushButton("⊕  현재 위치 북마크");
    btnAddBm_->setObjectName("btnBm");
    btnRemoveBm_ = new QPushButton("✕  북마크 삭제");
    btnRemoveBm_->setObjectName("btnBmRemove");
    btnRemoveBm_->setEnabled(false);

    rowBtns->addWidget(btnAddBm_);
    rowBtns->addWidget(btnRemoveBm_);
    root->addLayout(rowBtns);

    // ── 단축키 안내 ───────────────────────────────────────
    auto* lblHint = new QLabel("단축키: B 북마크 추가  |  [ ] 챕터 이동");
    lblHint->setStyleSheet("color:#444; font-size:9px;");
    root->addWidget(lblHint);

    root->addStretch();

    // 시그널 연결
    connect(lstChapters_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item){
        int idx = lstChapters_->row(item);
        jumpToChapter(idx);
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
}

void ChapterWidget::loadChapters() {
    if (!core_) return;

    // MPV에서 챕터 목록 가져오기
    QVariant chaptersVar = core_->getProperty("chapter-list");
    QVariantList chapList = chaptersVar.toList();

    // 기존 챕터(북마크 아닌 것)만 제거
    chapters_.erase(std::remove_if(chapters_.begin(), chapters_.end(),
        [](const Chapter& c){ return !c.isBookmark; }), chapters_.end());

    for (const QVariant& v : chapList) {
        QVariantMap m = v.toMap();
        Chapter ch;
        ch.startSec = m.value("time", 0.0).toDouble();
        ch.title    = m.value("title", QString("챕터 %1").arg(chapters_.size()+1)).toString();
        ch.isBookmark = false;
        chapters_.append(ch);
    }

    // 시간순 정렬
    std::sort(chapters_.begin(), chapters_.end(),
        [](const Chapter& a, const Chapter& b){ return a.startSec < b.startSec; });

    refreshList();
    emit chaptersChanged();
}

void ChapterWidget::addBookmark(double sec) {
    Chapter bm;
    bm.startSec  = sec;
    bm.title     = QString("북마크 %1").arg(formatTime(sec));
    bm.isBookmark = true;
    chapters_.append(bm);

    std::sort(chapters_.begin(), chapters_.end(),
        [](const Chapter& a, const Chapter& b){ return a.startSec < b.startSec; });

    refreshList();
    emit chaptersChanged();
}

void ChapterWidget::removeBookmark(int idx) {
    if (idx < 0 || idx >= chapters_.size()) return;
    if (!chapters_[idx].isBookmark) return;
    chapters_.removeAt(idx);
    refreshList();
    emit chaptersChanged();
}

void ChapterWidget::jumpToChapter(int idx) {
    if (idx < 0 || idx >= chapters_.size()) return;
    emit seekRequested(chapters_[idx].startSec);
}

void ChapterWidget::onPositionChanged(double sec) {
    currentPos_ = sec;
}

void ChapterWidget::refreshList() {
    lstChapters_->clear();
    for (const Chapter& ch : chapters_) {
        QString icon = ch.isBookmark ? "🔖" : "▶";
        QString text = QString("%1  %2  %3")
            .arg(icon)
            .arg(formatTime(ch.startSec))
            .arg(ch.title);
        auto* item = new QListWidgetItem(text);
        if (ch.isBookmark) {
            item->setForeground(QColor("#f0c040"));
        }
        lstChapters_->addItem(item);
    }
    lblCount_->setText(QString("챕터 %1개").arg(chapters_.size()));
}

QString ChapterWidget::formatTime(double sec) const {
    int h = (int)sec / 3600;
    int m = ((int)sec % 3600) / 60;
    int s = (int)sec % 60;
    if (h > 0)
        return QString("%1:%2:%3").arg(h).arg(m,2,10,QChar('0')).arg(s,2,10,QChar('0'));
    return QString("%1:%2").arg(m,2,10,QChar('0')).arg(s,2,10,QChar('0'));
}

void ChapterWidget::generateThumbnails(const QString& filePath) {
    mediaPath_ = filePath;
    // 썸네일 생성은 별도 스레드에서 ffmpeg로 처리
    // 현재는 기본 구현만 제공
}
