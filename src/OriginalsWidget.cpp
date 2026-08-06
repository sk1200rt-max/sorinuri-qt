#include "OriginalsWidget.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QDesktopServices>
#include <QUrl>
#include <QScrollArea>
#include <QFrame>
#include <QListWidgetItem>
#include <QNetworkRequest>
#include <QApplication>
#include <QPainterPath>
#include <QFontMetrics>
#include <algorithm>

// ── 카테고리 목록 ────────────────────────────────────────────────────────────
const QStringList OriginalsWidget::CATEGORIES = {
    "전체", "봄여름", "힐링", "위로", "사랑", "겨울",
    "가을", "비오는날", "새벽", "카페", "운동", "재즈", "이별"
};

// ── 공통 스타일 상수 ─────────────────────────────────────────────────────────
static const QString MINT    = "#00D4B4";
static const QString BG      = "#0e0e0e";
static const QString BG2     = "#1a1a1a";
static const QString BG3     = "#222222";
static const QString BORDER  = "#2a2a2a";
static const QString TEXT    = "#e0e0e0";
static const QString TEXT2   = "#888888";

// ── 아이템 역할 상수 ─────────────────────────────────────────────────────────
static const int ROLE_URL      = Qt::UserRole;
static const int ROLE_YTURL    = Qt::UserRole + 1;
static const int ROLE_COVER    = Qt::UserRole + 2;
static const int ROLE_TITLE    = Qt::UserRole + 3;
static const int ROLE_ARTIST   = Qt::UserRole + 4;
static const int ROLE_DURATION = Qt::UserRole + 5;
static const int ROLE_PLAYING  = Qt::UserRole + 6;

// ── 분위기 분류 ──────────────────────────────────────────────────────────────
static const QStringList BRIGHT_MOODS = {"신나는", "활기찬", "밝은", "경쾌한", "에너지"};
static const QStringList CALM_MOODS   = {"잔잔한", "차분한", "힐링", "평화로운", "아늑함"};

// ════════════════════════════════════════════════════════════════════════════
// SongItemDelegate — 썸네일 + 제목 + 아티스트 + 재생시간 렌더링
// ════════════════════════════════════════════════════════════════════════════
SongItemDelegate::SongItemDelegate(QObject* parent)
    : QAbstractItemDelegate(parent) {}

void SongItemDelegate::setThumbnail(const QString& coverId, const QPixmap& px) {
    if (!px.isNull()) {
        thumbCache_[coverId] = px.scaled(THUMB_W, THUMB_H,
                                          Qt::KeepAspectRatioByExpanding,
                                          Qt::SmoothTransformation);
    }
}

void SongItemDelegate::paint(QPainter* painter,
                              const QStyleOptionViewItem& option,
                              const QModelIndex& index) const {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setRenderHint(QPainter::SmoothPixmapTransform);

    const QRect r = option.rect;
    bool isPlaying  = index.data(ROLE_PLAYING).toBool();
    bool isSelected = (option.state & QStyle::State_Selected);
    bool isHovered  = (option.state & QStyle::State_MouseOver);

    // ── 배경 ──────────────────────────────────────────────────────────────
    if (isSelected)
        painter->fillRect(r, QColor(0, 212, 180, 25));
    else if (isHovered)
        painter->fillRect(r, QColor(255, 255, 255, 6));

    // 재생 중 왼쪽 민트 바
    if (isPlaying)
        painter->fillRect(QRect(r.left(), r.top() + 2, 3, r.height() - 4),
                          QColor(MINT));

    // ── 썸네일 ────────────────────────────────────────────────────────────
    int leftPad = PAD + (isPlaying ? 5 : 2);
    QRect thumbRect(r.left() + leftPad,
                    r.top() + (r.height() - THUMB_H) / 2,
                    THUMB_W, THUMB_H);

    QString coverId = index.data(ROLE_COVER).toString();
    if (thumbCache_.contains(coverId) && !thumbCache_[coverId].isNull()) {
        QPainterPath path;
        path.addRoundedRect(thumbRect, 5, 5);
        painter->setClipPath(path);
        painter->drawPixmap(thumbRect, thumbCache_[coverId]);
        painter->setClipping(false);
    } else {
        // 플레이스홀더
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(BG3));
        painter->drawRoundedRect(thumbRect, 5, 5);
        painter->setPen(QColor(TEXT2));
        QFont noteFont;
        noteFont.setPixelSize(18);
        painter->setFont(noteFont);
        painter->drawText(thumbRect, Qt::AlignCenter, "♪");
    }

    // ── 텍스트 영역 ───────────────────────────────────────────────────────
    int textX = thumbRect.right() + PAD + 4;
    int durW  = 48;
    int textW = r.right() - textX - durW - PAD;

    QString title    = index.data(ROLE_TITLE).toString();
    QString artist   = index.data(ROLE_ARTIST).toString();
    QString duration = index.data(ROLE_DURATION).toString();

    // 제목
    QFont titleFont;
    titleFont.setPixelSize(13);
    titleFont.setBold(isPlaying);
    painter->setFont(titleFont);
    painter->setPen(isPlaying ? QColor(MINT) : QColor(TEXT));
    QRect titleRect(textX, r.top() + 11, textW, 18);
    QString elidedTitle = QFontMetrics(titleFont).elidedText(title, Qt::ElideRight, textW);
    painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter, elidedTitle);

    // 아티스트
    QFont artistFont;
    artistFont.setPixelSize(11);
    painter->setFont(artistFont);
    painter->setPen(QColor(TEXT2));
    QRect artistRect(textX, r.top() + 33, textW, 16);
    QString elidedArtist = QFontMetrics(artistFont).elidedText(artist, Qt::ElideRight, textW);
    painter->drawText(artistRect, Qt::AlignLeft | Qt::AlignVCenter, elidedArtist);

    // ── 재생시간 (오른쪽 정렬) ────────────────────────────────────────────
    if (!duration.isEmpty()) {
        QFont durFont;
        durFont.setPixelSize(11);
        painter->setFont(durFont);
        painter->setPen(QColor(TEXT2));
        QRect durRect(r.right() - durW - PAD, r.top(), durW, r.height());
        painter->drawText(durRect, Qt::AlignRight | Qt::AlignVCenter, duration);
    }

    // ── 재생 중 표시: 아티스트 옆에 작은 도트 ─────────────────────────────
    if (isPlaying) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(MINT));
        int dotY = r.top() + 36;
        int dotX = textX + QFontMetrics(artistFont).horizontalAdvance(elidedArtist) + 8;
        for (int i = 0; i < 3; ++i)
            painter->drawEllipse(dotX + i * 6, dotY, 3, 3);
    }

    // ── 구분선 ────────────────────────────────────────────────────────────
    painter->setPen(QPen(QColor(BORDER), 1));
    painter->drawLine(r.left() + PAD, r.bottom(), r.right() - PAD, r.bottom());

    painter->restore();
}

QSize SongItemDelegate::sizeHint(const QStyleOptionViewItem&,
                                  const QModelIndex&) const {
    return QSize(0, ITEM_H);
}

// ════════════════════════════════════════════════════════════════════════════
// OriginalsWidget
// ════════════════════════════════════════════════════════════════════════════
OriginalsWidget::OriginalsWidget(QWidget* parent) : QWidget(parent) {
    setStyleSheet(QString("background: %1; color: %2;").arg(BG).arg(TEXT));

    // API URL을 설정에서 로드
    apiUrl_ = settings_.value("originals/api_url",
        "https://sorinuri.com/api/songs.json").toString();

    setupUI();

    // 네트워크 매니저
    nam_ = new QNetworkAccessManager(this);

    // 5분 자동 갱신 타이머
    refreshTimer_ = new QTimer(this);
    refreshTimer_->setInterval(5 * 60 * 1000);
    connect(refreshTimer_, &QTimer::timeout, this, &OriginalsWidget::fetchSongs);
    refreshTimer_->start();

    // 토스트 타이머
    toastTimer_ = new QTimer(this);
    toastTimer_->setSingleShot(true);
    connect(toastTimer_, &QTimer::timeout, this, [this]() {
        toastLabel_->setVisible(false);
    });

    // 초기 fetch (300ms 딜레이 - UI 완전 초기화 후)
    QTimer::singleShot(300, this, &OriginalsWidget::fetchSongs);
}

// ── UI 구성 ──────────────────────────────────────────────────────────────────
void OriginalsWidget::setupUI() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── 헤더 ─────────────────────────────────────────────────────────────
    auto* header = new QWidget(this);
    header->setFixedHeight(50);
    header->setStyleSheet(QString(
        "background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "stop:0 #0a1a18, stop:0.6 %1, stop:1 %1);"
        "border-bottom: 1px solid %2;").arg(BG2).arg(BORDER));
    auto* hRow = new QHBoxLayout(header);
    hRow->setContentsMargins(14, 0, 14, 0);
    hRow->setSpacing(10);

    auto* titleLbl = new QLabel("♪  SORINURI ORIGINALS", header);
    titleLbl->setStyleSheet(QString(
        "color: %1; font-size: 13px; font-weight: 800; letter-spacing: 2px;"
        "background: transparent;").arg(MINT));

    statusLabel_ = new QLabel("", header);
    statusLabel_->setStyleSheet("color: #555; font-size: 10px; background: transparent;");

    hRow->addWidget(titleLbl);
    hRow->addStretch();
    hRow->addWidget(statusLabel_);
    root->addWidget(header);

    // ── 검색 + 정렬 바 ───────────────────────────────────────────────────
    auto* searchBar = new QWidget(this);
    searchBar->setFixedHeight(44);
    searchBar->setStyleSheet(QString("background: %1; border-bottom: 1px solid %2;")
                              .arg(BG2).arg(BORDER));
    auto* sRow = new QHBoxLayout(searchBar);
    sRow->setContentsMargins(10, 6, 10, 6);
    sRow->setSpacing(8);

    searchEdit_ = new QLineEdit(searchBar);
    searchEdit_->setPlaceholderText("🔍  제목, 아티스트, 태그 검색...");
    searchEdit_->setStyleSheet(QString(
        "QLineEdit { background: %1; color: %2; border: 1px solid %3;"
        "border-radius: 5px; padding: 4px 10px; font-size: 11px; }"
        "QLineEdit:focus { border-color: %4; }").arg(BG3).arg(TEXT).arg(BORDER).arg(MINT));
    connect(searchEdit_, &QLineEdit::textChanged,
            this, &OriginalsWidget::onSearchChanged);

    sortCombo_ = new QComboBox(searchBar);
    sortCombo_->addItems({"기본순", "제목순", "밝은 곡", "잔잔한 곡"});
    sortCombo_->setFixedWidth(90);
    sortCombo_->setStyleSheet(QString(
        "QComboBox { background: %1; color: %2; border: 1px solid %3;"
        "border-radius: 5px; padding: 3px 8px; font-size: 11px; }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView { background: %1; color: %2; "
        "selection-background-color: %4; }").arg(BG3).arg(TEXT).arg(BORDER).arg(MINT));
    connect(sortCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &OriginalsWidget::onSortChanged);

    sRow->addWidget(searchEdit_, 1);
    sRow->addWidget(sortCombo_);
    root->addWidget(searchBar);

    // ── 카테고리 필터 바 ─────────────────────────────────────────────────
    catBar_ = new QWidget(this);
    catBar_->setFixedHeight(38);
    catBar_->setStyleSheet(QString("background: %1; border-bottom: 1px solid %2;")
                            .arg(BG).arg(BORDER));

    auto* catScroll = new QScrollArea(this);
    catScroll->setWidget(catBar_);
    catScroll->setWidgetResizable(false);
    catScroll->setFixedHeight(40);
    catScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    catScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    catScroll->setStyleSheet("QScrollArea { border: none; background: transparent; }");

    auto* catRow = new QHBoxLayout(catBar_);
    catRow->setContentsMargins(10, 4, 10, 4);
    catRow->setSpacing(5);

    for (const QString& cat : CATEGORIES) {
        auto* btn = new QPushButton(cat, catBar_);
        btn->setFixedHeight(26);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFocusPolicy(Qt::NoFocus);
        bool isActive = (cat == activeCategory_);
        btn->setStyleSheet(isActive
            ? QString("QPushButton { background: %1; color: #000; border: none;"
                      "border-radius: 13px; padding: 0 12px; font-size: 11px; font-weight: 700; }").arg(MINT)
            : QString("QPushButton { background: %1; color: %2; border: 1px solid %3;"
                      "border-radius: 13px; padding: 0 12px; font-size: 11px; }"
                      "QPushButton:hover { border-color: %4; color: %4; }").arg(BG2).arg(TEXT2).arg(BORDER).arg(MINT));
        connect(btn, &QPushButton::clicked, this, [this, cat]() {
            onCategoryClicked(cat);
        });
        catBtns_.append(btn);
        catRow->addWidget(btn);
    }
    catRow->addStretch();
    catBar_->setMinimumWidth(CATEGORIES.size() * 82);
    root->addWidget(catScroll);

    // ── 곡 목록 ──────────────────────────────────────────────────────────
    listWidget_ = new QListWidget(this);
    listWidget_->setStyleSheet(QString(
        "QListWidget { background: %1; border: none; outline: none; }"
        "QListWidget::item { border: none; }"
        "QListWidget::item:selected { background: rgba(0,212,180,0.08); }"
        "QListWidget::item:hover { background: rgba(255,255,255,0.04); }"
        "QScrollBar:vertical { background: %2; width: 6px; border: none; }"
        "QScrollBar::handle:vertical { background: #333; border-radius: 3px; min-height: 30px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
    ).arg(BG).arg(BG));
    listWidget_->setMouseTracking(true);
    listWidget_->setSpacing(0);
    listWidget_->setUniformItemSizes(true);

    // 커스텀 델리게이트 설정
    delegate_ = new SongItemDelegate(this);
    listWidget_->setItemDelegate(delegate_);

    connect(listWidget_, &QListWidget::itemDoubleClicked,
            this, &OriginalsWidget::onItemDoubleClicked);
    root->addWidget(listWidget_, 1);

    // ── 하단 액션 바 ─────────────────────────────────────────────────────
    auto* actionBar = new QWidget(this);
    actionBar->setFixedHeight(48);
    actionBar->setStyleSheet(QString("background: %1; border-top: 1px solid %2;")
                              .arg(BG2).arg(BORDER));
    auto* aRow = new QHBoxLayout(actionBar);
    aRow->setContentsMargins(12, 8, 12, 8);
    aRow->setSpacing(8);

    auto* playAllBtn = new QPushButton("▶  전체 재생", actionBar);
    playAllBtn->setFixedHeight(32);
    playAllBtn->setCursor(Qt::PointingHandCursor);
    playAllBtn->setFocusPolicy(Qt::NoFocus);
    playAllBtn->setStyleSheet(QString(
        "QPushButton { background: %1; color: #000; border: none;"
        "border-radius: 6px; padding: 0 18px; font-size: 12px; font-weight: 700; }"
        "QPushButton:hover { background: #00f0cc; }"
        "QPushButton:pressed { background: #00b89a; }").arg(MINT));
    connect(playAllBtn, &QPushButton::clicked, this, &OriginalsWidget::onPlayAllClicked);

    auto* ytBtn = new QPushButton("▶  YouTube", actionBar);
    ytBtn->setFixedHeight(32);
    ytBtn->setCursor(Qt::PointingHandCursor);
    ytBtn->setFocusPolicy(Qt::NoFocus);
    ytBtn->setStyleSheet(
        "QPushButton { background: #cc0000; color: #fff; border: none;"
        "border-radius: 6px; padding: 0 14px; font-size: 11px; font-weight: 600; }"
        "QPushButton:hover { background: #e00000; }"
        "QPushButton:pressed { background: #aa0000; }");
    connect(ytBtn, &QPushButton::clicked, this, &OriginalsWidget::onYouTubeClicked);

    aRow->addWidget(playAllBtn);
    aRow->addWidget(ytBtn);
    aRow->addStretch();
    root->addWidget(actionBar);

    // ── 토스트 알림 (플로팅) ─────────────────────────────────────────────
    toastLabel_ = new QLabel(this);
    toastLabel_->setStyleSheet(QString(
        "background: %1; color: #000; border-radius: 8px;"
        "padding: 8px 16px; font-size: 12px; font-weight: 700;").arg(MINT));
    toastLabel_->setVisible(false);
    toastLabel_->setAttribute(Qt::WA_TransparentForMouseEvents);
    toastLabel_->raise();
}

// ── 네트워크: songs.json fetch ────────────────────────────────────────────────
void OriginalsWidget::fetchSongs() {
    statusLabel_->setText("갱신 중...");
    QUrl fetchUrl(apiUrl_);
    QNetworkRequest req;
    req.setUrl(fetchUrl);
    req.setHeader(QNetworkRequest::UserAgentHeader, "SorinuriPlayer/6.0");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = nam_->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onFetchFinished(reply);
    });
}

void OriginalsWidget::onFetchFinished(QNetworkReply* reply) {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        statusLabel_->setText("오프라인");
        if (songs_.isEmpty()) {
            listWidget_->clear();
            auto* item = new QListWidgetItem("  ⚠  네트워크 연결을 확인해 주세요.");
            item->setForeground(QColor(TEXT2));
            listWidget_->addItem(item);
        }
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) {
        statusLabel_->setText("데이터 오류");
        return;
    }

    QJsonArray arr = doc.array();
    int newCount = arr.size();

    // 새 곡 알림
    if (lastSongCount_ >= 0 && newCount > lastSongCount_) {
        int diff = newCount - lastSongCount_;
        showToast(QString("♪  새 곡 %1곡이 추가됐습니다!").arg(diff));
    }
    lastSongCount_ = newCount;
    rawSongs_ = arr;

    // 파싱
    songs_.clear();
    for (const auto& val : arr) {
        QJsonObject obj = val.toObject();
        SongInfo s;
        s.id         = obj["id"].toString();
        s.title      = obj["title"].toString();
        s.artist     = obj["artist"].toString();
        s.duration   = obj["duration"].toString();
        s.mp3        = obj["mp3"].toString();
        s.cover      = obj["cover"].toString();
        s.youtubeId  = obj["youtube_id"].toString();
        s.youtubeUrl = obj["youtube_url"].toString();
        s.mood       = obj["mood"].toString();
        for (const auto& c : obj["categories"].toArray())
            s.categories << c.toString();
        for (const auto& t : obj["tags"].toArray())
            s.tags << t.toString();
        songs_.append(s);
    }

    statusLabel_->setText(QString("%1곡").arg(songs_.size()));
    applyFilter();

    // 썸네일 fetch (처음 30개 즉시)
    int limit = qMin(songs_.size(), 30);
    for (int i = 0; i < limit; ++i)
        fetchThumbnail(songs_[i]);
}

// ── 썸네일 fetch ──────────────────────────────────────────────────────────────
void OriginalsWidget::fetchThumbnail(const SongInfo& song) {
    if (song.cover.isEmpty()) return;
    QUrl thumbUrl(baseUrl_ + song.cover);
    QNetworkRequest req;
    req.setUrl(thumbUrl);
    req.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                     QNetworkRequest::PreferCache);
    QNetworkReply* reply = nam_->get(req);
    reply->setProperty("coverId", song.cover);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onThumbFinished(reply);
    });
}

void OriginalsWidget::onThumbFinished(QNetworkReply* reply) {
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) return;

    QString coverId = reply->property("coverId").toString();
    QByteArray data = reply->readAll();
    QPixmap px;
    if (!px.loadFromData(data)) return;

    delegate_->setThumbnail(coverId, px);
    listWidget_->update();
}

// ── 필터 적용 ─────────────────────────────────────────────────────────────────
void OriginalsWidget::applyFilter() {
    filtered_.clear();
    QString search = searchText_.trimmed().toLower();

    for (const SongInfo& s : songs_) {
        if (activeCategory_ != "전체" && !s.categories.contains(activeCategory_))
            continue;
        if (!search.isEmpty()) {
            bool match = s.title.toLower().contains(search)
                      || s.artist.toLower().contains(search)
                      || s.mood.toLower().contains(search)
                      || s.tags.join(" ").toLower().contains(search);
            if (!match) continue;
        }
        filtered_.append(s);
    }

    if (sortIndex_ == 1) {
        std::stable_sort(filtered_.begin(), filtered_.end(),
            [](const SongInfo& a, const SongInfo& b){ return a.title < b.title; });
    } else if (sortIndex_ == 2) {
        std::stable_sort(filtered_.begin(), filtered_.end(),
            [](const SongInfo& a, const SongInfo& b){
                return BRIGHT_MOODS.contains(a.mood) > BRIGHT_MOODS.contains(b.mood);
            });
    } else if (sortIndex_ == 3) {
        std::stable_sort(filtered_.begin(), filtered_.end(),
            [](const SongInfo& a, const SongInfo& b){
                return CALM_MOODS.contains(a.mood) > CALM_MOODS.contains(b.mood);
            });
    }

    updateList();
}

void OriginalsWidget::updateList() {
    listWidget_->clear();

    if (filtered_.isEmpty()) {
        auto* item = new QListWidgetItem("  검색 결과가 없습니다.");
        item->setForeground(QColor(TEXT2));
        listWidget_->addItem(item);
        return;
    }

    for (const SongInfo& s : filtered_) {
        QString mp3Url  = baseUrl_ + s.mp3;
        bool isPlaying  = (!currentFile_.isEmpty() && currentFile_ == mp3Url);

        auto* item = new QListWidgetItem();
        item->setData(ROLE_URL,      mp3Url);
        item->setData(ROLE_YTURL,    s.youtubeUrl);
        item->setData(ROLE_COVER,    s.cover);
        item->setData(ROLE_TITLE,    s.title);
        item->setData(ROLE_ARTIST,   s.artist.isEmpty() ? "소리누리" : s.artist);
        item->setData(ROLE_DURATION, s.duration);
        item->setData(ROLE_PLAYING,  isPlaying);
        item->setToolTip(QString("분위기: %1\n태그: %2\n카테고리: %3")
            .arg(s.mood)
            .arg(s.tags.join(", "))
            .arg(s.categories.join(", ")));

        listWidget_->addItem(item);
    }

    delegate_->setCurrentUrl(currentFile_);
}

// ── 슬롯 ─────────────────────────────────────────────────────────────────────
void OriginalsWidget::onSearchChanged(const QString& text) {
    searchText_ = text;
    applyFilter();
}

void OriginalsWidget::onCategoryClicked(const QString& category) {
    activeCategory_ = category;
    for (int i = 0; i < catBtns_.size(); ++i) {
        bool active = (CATEGORIES[i] == category);
        catBtns_[i]->setStyleSheet(active
            ? QString("QPushButton { background: %1; color: #000; border: none;"
                      "border-radius: 13px; padding: 0 12px; font-size: 11px; font-weight: 700; }").arg(MINT)
            : QString("QPushButton { background: %1; color: %2; border: 1px solid %3;"
                      "border-radius: 13px; padding: 0 12px; font-size: 11px; }"
                      "QPushButton:hover { border-color: %4; color: %4; }").arg(BG2).arg(TEXT2).arg(BORDER).arg(MINT));
    }
    applyFilter();
}

void OriginalsWidget::onSortChanged(int index) {
    sortIndex_ = index;
    applyFilter();
}

void OriginalsWidget::onItemDoubleClicked(QListWidgetItem* item) {
    if (!item) return;
    QString url = item->data(ROLE_URL).toString();
    if (!url.isEmpty() && url.startsWith("http"))
        emit playRequested(url);
}

void OriginalsWidget::onPlayAllClicked() {
    emit playlistRequested(m3uUrl_);
}

void OriginalsWidget::onYouTubeClicked() {
    auto* cur = listWidget_->currentItem();
    if (!cur) return;
    QString ytUrl = cur->data(ROLE_YTURL).toString();
    if (!ytUrl.isEmpty())
        QDesktopServices::openUrl(QUrl(ytUrl));
}

// ── 공개 메서드 ──────────────────────────────────────────────────────────────
void OriginalsWidget::setCurrentFile(const QString& fileUrl) {
    currentFile_ = fileUrl;
    delegate_->setCurrentUrl(fileUrl);
    updateList();
}

void OriginalsWidget::setApiUrl(const QString& url) {
    if (url.isEmpty()) return;
    apiUrl_ = url;
    settings_.setValue("originals/api_url", url);
    fetchSongs();
}

// ── 토스트 알림 ──────────────────────────────────────────────────────────────
void OriginalsWidget::showToast(const QString& msg) {
    toastLabel_->setText(msg);
    toastLabel_->adjustSize();
    int x = (width() - toastLabel_->width()) / 2;
    int y = height() - toastLabel_->height() - 56;
    toastLabel_->move(x, y);
    toastLabel_->setVisible(true);
    toastLabel_->raise();
    toastTimer_->start(4000);
}
