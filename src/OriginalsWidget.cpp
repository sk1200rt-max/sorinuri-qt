#include "OriginalsWidget.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QDesktopServices>
#include <QUrl>
#include <QScrollArea>
#include <QFrame>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QListWidgetItem>
#include <QPixmap>
#include <QNetworkRequest>
#include <QApplication>

// ── 카테고리 목록 ────────────────────────────────────────────────────────────
const QStringList OriginalsWidget::CATEGORIES = {
    "전체", "봄여름", "힐링", "위로", "사랑", "겨울",
    "가을", "비오는날", "새벽", "카페", "운동", "재즈", "이별"
};

// ── 공통 스타일 상수 ─────────────────────────────────────────────────────────
static const QString MINT   = "#00D4B4";
static const QString BG     = "#111111";
static const QString BG2    = "#1a1a1a";
static const QString BORDER = "#2a2a2a";
static const QString TEXT   = "#e0e0e0";
static const QString TEXT2  = "#888888";

// ── 생성자 ───────────────────────────────────────────────────────────────────
OriginalsWidget::OriginalsWidget(QWidget* parent) : QWidget(parent) {
    setStyleSheet(QString("background: %1; color: %2;").arg(BG).arg(TEXT));

    // API URL을 설정에서 로드
    apiUrl_ = settings_.value("originals/api_url",
        "https://sorinuri.com/api/songs.json").toString();

    setupUI();

    // 네트워크 매니저
    nam_ = new QNetworkAccessManager(this);
    connect(nam_, &QNetworkAccessManager::finished,
            this, &OriginalsWidget::onFetchFinished);

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

    // 초기 fetch
    QTimer::singleShot(300, this, &OriginalsWidget::fetchSongs);
}

// ── UI 구성 ──────────────────────────────────────────────────────────────────
void OriginalsWidget::setupUI() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── 헤더 (타이틀 + 정렬) ─────────────────────────────────────────────────
    auto* header = new QWidget(this);
    header->setStyleSheet(QString("background: %1; border-bottom: 1px solid %2;").arg(BG).arg(BORDER));
    auto* hRow = new QHBoxLayout(header);
    hRow->setContentsMargins(12, 8, 12, 8);
    hRow->setSpacing(8);

    auto* titleLbl = new QLabel("♪  SORINURI ORIGINALS", header);
    titleLbl->setStyleSheet(QString(
        "color: %1; font-size: 12px; font-weight: 800; letter-spacing: 2px;"
        "background: transparent;").arg(MINT));

    statusLabel_ = new QLabel("로딩 중...", header);
    statusLabel_->setStyleSheet("color: #555; font-size: 10px; background: transparent;");

    sortCombo_ = new QComboBox(header);
    sortCombo_->addItems({"기본순", "제목순", "밝은 곡 먼저", "잔잔한 곡 먼저"});
    sortCombo_->setFixedHeight(22);
    sortCombo_->setStyleSheet(QString(
        "QComboBox { background: %1; color: %2; border: 1px solid %3;"
        "border-radius: 4px; padding: 0 8px; font-size: 11px; }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView { background: %1; color: %2; border: 1px solid %3; }")
        .arg(BG2).arg(TEXT).arg(BORDER));
    connect(sortCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &OriginalsWidget::onSortChanged);

    hRow->addWidget(titleLbl);
    hRow->addWidget(statusLabel_);
    hRow->addStretch();
    hRow->addWidget(sortCombo_);
    root->addWidget(header);

    // ── 검색창 ───────────────────────────────────────────────────────────────
    auto* searchWrap = new QWidget(this);
    searchWrap->setStyleSheet(QString("background: %1; border-bottom: 1px solid %2;").arg(BG).arg(BORDER));
    auto* sRow = new QHBoxLayout(searchWrap);
    sRow->setContentsMargins(10, 6, 10, 6);
    sRow->setSpacing(6);

    searchEdit_ = new QLineEdit(searchWrap);
    searchEdit_->setPlaceholderText("🔍  제목, 태그, 분위기 검색...");
    searchEdit_->setFixedHeight(26);
    searchEdit_->setStyleSheet(QString(
        "QLineEdit { background: %1; color: %2; border: 1px solid %3;"
        "border-radius: 5px; padding: 0 10px; font-size: 12px; }"
        "QLineEdit:focus { border-color: %4; }")
        .arg(BG2).arg(TEXT).arg(BORDER).arg(MINT));
    connect(searchEdit_, &QLineEdit::textChanged,
            this, &OriginalsWidget::onSearchChanged);
    sRow->addWidget(searchEdit_);
    root->addWidget(searchWrap);

    // ── 카테고리 버튼 바 ─────────────────────────────────────────────────────
    catBar_ = new QWidget(this);
    catBar_->setStyleSheet(QString("background: %1; border-bottom: 1px solid %2;").arg(BG).arg(BORDER));
    auto* catScroll = new QScrollArea(this);
    catScroll->setWidget(catBar_);
    catScroll->setWidgetResizable(true);
    catScroll->setFixedHeight(38);
    catScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    catScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    catScroll->setStyleSheet("QScrollArea { border: none; background: transparent; }");

    auto* catRow = new QHBoxLayout(catBar_);
    catRow->setContentsMargins(8, 4, 8, 4);
    catRow->setSpacing(4);

    for (const QString& cat : CATEGORIES) {
        auto* btn = new QPushButton(cat, catBar_);
        btn->setFixedHeight(24);
        btn->setCheckable(false);
        btn->setFocusPolicy(Qt::NoFocus);
        bool isActive = (cat == activeCategory_);
        btn->setStyleSheet(isActive
            ? QString("QPushButton { background: %1; color: #000; border: none;"
                      "border-radius: 4px; padding: 0 10px; font-size: 11px; font-weight: 700; }").arg(MINT)
            : QString("QPushButton { background: %1; color: %2; border: 1px solid %3;"
                      "border-radius: 4px; padding: 0 10px; font-size: 11px; }"
                      "QPushButton:hover { border-color: %4; color: %4; }").arg(BG2).arg(TEXT2).arg(BORDER).arg(MINT));
        connect(btn, &QPushButton::clicked, this, [this, cat]() {
            onCategoryClicked(cat);
        });
        catRow->addWidget(btn);
        catBtns_.append(btn);
    }
    catRow->addStretch();
    root->addWidget(catScroll);

    // ── 곡 목록 ──────────────────────────────────────────────────────────────
    listWidget_ = new QListWidget(this);
    listWidget_->setStyleSheet(QString(
        "QListWidget { background: %1; border: none; outline: none; }"
        "QListWidget::item { padding: 8px 12px; border-bottom: 1px solid %2; }"
        "QListWidget::item:selected { background: #1a3a5c; }"
        "QListWidget::item:hover { background: rgba(0,212,180,0.06); }")
        .arg(BG).arg(BORDER));
    listWidget_->setAlternatingRowColors(false);
    listWidget_->setIconSize(QSize(36, 36));
    connect(listWidget_, &QListWidget::itemDoubleClicked,
            this, &OriginalsWidget::onItemDoubleClicked);
    root->addWidget(listWidget_, 1);

    // ── 하단 버튼 바 ─────────────────────────────────────────────────────────
    auto* footer = new QWidget(this);
    footer->setStyleSheet(QString("background: %1; border-top: 1px solid %2;").arg(BG).arg(BORDER));
    auto* fRow = new QHBoxLayout(footer);
    fRow->setContentsMargins(10, 6, 10, 6);
    fRow->setSpacing(8);

    auto makeFBtn = [&](const QString& text, const QString& tip, bool primary) {
        auto* btn = new QPushButton(text, footer);
        btn->setFixedHeight(26);
        btn->setToolTip(tip);
        btn->setFocusPolicy(Qt::NoFocus);
        if (primary) {
            btn->setStyleSheet(QString(
                "QPushButton { background: %1; color: #000; border: none;"
                "border-radius: 5px; padding: 0 14px; font-size: 12px; font-weight: 700; }"
                "QPushButton:hover { background: #00efd0; }").arg(MINT));
        } else {
            btn->setStyleSheet(QString(
                "QPushButton { background: %1; color: %2; border: 1px solid %3;"
                "border-radius: 5px; padding: 0 14px; font-size: 12px; }"
                "QPushButton:hover { border-color: %4; color: %4; }").arg(BG2).arg(TEXT2).arg(BORDER).arg(MINT));
        }
        return btn;
    };

    auto* btnAll = makeFBtn("▶  전체 재생", "전체 목록을 M3U로 재생", true);
    auto* btnYT  = makeFBtn("▶  YouTube에서 보기", "선택한 곡을 YouTube에서 열기", false);
    connect(btnAll, &QPushButton::clicked, this, &OriginalsWidget::onPlayAllClicked);
    connect(btnYT,  &QPushButton::clicked, this, &OriginalsWidget::onYouTubeClicked);

    fRow->addWidget(btnAll);
    fRow->addWidget(btnYT);
    fRow->addStretch();
    root->addWidget(footer);

    // ── 토스트 알림 ──────────────────────────────────────────────────────────
    toastLabel_ = new QLabel(this);
    toastLabel_->setStyleSheet(QString(
        "background: %1; color: #000; border-radius: 8px;"
        "padding: 8px 16px; font-size: 12px; font-weight: 700;").arg(MINT));
    toastLabel_->setVisible(false);
    toastLabel_->setAlignment(Qt::AlignCenter);
    toastLabel_->raise();
}

// ── API fetch ────────────────────────────────────────────────────────────────
void OriginalsWidget::fetchSongs() {
    statusLabel_->setText("갱신 중...");
    QNetworkRequest request(QUrl(apiUrl_));
    request.setRawHeader(QByteArray("Cache-Control"), QByteArray("no-cache"));
    nam_->get(request);
}

void OriginalsWidget::onFetchFinished(QNetworkReply* reply) {
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        statusLabel_->setText("오프라인 상태입니다");
        if (songs_.isEmpty()) {
            listWidget_->clear();
            auto* item = new QListWidgetItem("⚠  네트워크에 연결할 수 없습니다.");
            item->setForeground(QColor("#888"));
            listWidget_->addItem(item);
        }
        return;
    }

    QJsonParseError err;
    auto doc = QJsonDocument::fromJson(reply->readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray()) {
        statusLabel_->setText("데이터 오류");
        return;
    }

    rawSongs_ = doc.array();

    // 새 곡 알림
    int newCount = rawSongs_.size();
    if (lastSongCount_ >= 0 && newCount > lastSongCount_) {
        int added = newCount - lastSongCount_;
        showToast(QString("♪  새 곡 %1곡이 추가됐습니다!  [바로 듣기]").arg(added));
    }
    lastSongCount_ = newCount;

    // 파싱
    songs_.clear();
    for (const auto& v : rawSongs_) {
        QJsonObject o = v.toObject();
        SongInfo s;
        s.id         = o["id"].toString();
        s.title      = o["title"].toString();
        s.artist     = o["artist"].toString();
        s.duration   = o["duration"].toString();
        s.mp3        = o["mp3"].toString();
        s.cover      = o["cover"].toString();
        s.youtubeId  = o["youtube_id"].toString();
        s.youtubeUrl = o["youtube_url"].toString();
        s.mood       = o["mood"].toString();
        for (const auto& c : o["categories"].toArray())
            s.categories << c.toString();
        for (const auto& t : o["tags"].toArray())
            s.tags << t.toString();
        songs_.append(s);
    }

    statusLabel_->setText(QString("%1곡").arg(songs_.size()));
    applyFilter();
}

// ── 필터 / 정렬 ──────────────────────────────────────────────────────────────
void OriginalsWidget::applyFilter() {
    filtered_.clear();
    QString q = searchText_.toLower();

    for (const SongInfo& s : songs_) {
        // 카테고리 필터
        if (activeCategory_ != "전체" && !s.categories.contains(activeCategory_))
            continue;
        // 검색 필터
        if (!q.isEmpty()) {
            bool match = s.title.toLower().contains(q)
                      || s.mood.toLower().contains(q)
                      || s.tags.join(' ').toLower().contains(q)
                      || s.artist.toLower().contains(q);
            if (!match) continue;
        }
        filtered_.append(s);
    }

    // 정렬
    static const QStringList BRIGHT_MOODS = {"밝음", "에너지", "설렘"};
    static const QStringList CALM_MOODS   = {"아늑함", "따뜻함", "몽환적", "쓸쓸함"};

    if (sortIndex_ == 1) {
        std::sort(filtered_.begin(), filtered_.end(),
            [](const SongInfo& a, const SongInfo& b){ return a.title < b.title; });
    } else if (sortIndex_ == 2) {
        std::stable_sort(filtered_.begin(), filtered_.end(),
            [](const SongInfo& a, const SongInfo& b){
                bool ab = BRIGHT_MOODS.contains(a.mood);
                bool bb = BRIGHT_MOODS.contains(b.mood);
                if (ab != bb) return ab > bb;
                return false;
            });
    } else if (sortIndex_ == 3) {
        std::stable_sort(filtered_.begin(), filtered_.end(),
            [](const SongInfo& a, const SongInfo& b){
                bool ac = CALM_MOODS.contains(a.mood);
                bool bc = CALM_MOODS.contains(b.mood);
                if (ac != bc) return ac > bc;
                return false;
            });
    }

    updateList();
}

void OriginalsWidget::updateList() {
    listWidget_->clear();
    for (const SongInfo& s : filtered_) {
        QString mp3Url = baseUrl_ + s.mp3;
        bool isPlaying = (!currentFile_.isEmpty() && currentFile_ == mp3Url);

        QString label = isPlaying
            ? QString("▶  %1  —  %2   %3").arg(s.title, s.artist, s.duration)
            : QString("    %1  —  %2   %3").arg(s.title, s.artist, s.duration);

        auto* item = new QListWidgetItem(label);
        item->setData(Qt::UserRole, mp3Url);
        item->setData(Qt::UserRole + 1, s.youtubeUrl);

        if (isPlaying) {
            item->setForeground(QColor(MINT));
            QFont f = item->font();
            f.setBold(true);
            item->setFont(f);
        }

        // 분위기 배지 (툴팁)
        QString tip = QString("분위기: %1\n태그: %2\n카테고리: %3")
            .arg(s.mood)
            .arg(s.tags.join(", "))
            .arg(s.categories.join(", "));
        item->setToolTip(tip);

        listWidget_->addItem(item);
    }

    if (filtered_.isEmpty()) {
        auto* item = new QListWidgetItem("검색 결과가 없습니다.");
        item->setForeground(QColor("#555"));
        listWidget_->addItem(item);
    }
}

// ── 슬롯 ─────────────────────────────────────────────────────────────────────
void OriginalsWidget::onSearchChanged(const QString& text) {
    searchText_ = text;
    applyFilter();
}

void OriginalsWidget::onCategoryClicked(const QString& category) {
    activeCategory_ = category;

    // 버튼 스타일 갱신
    for (int i = 0; i < catBtns_.size(); ++i) {
        bool active = (CATEGORIES[i] == category);
        catBtns_[i]->setStyleSheet(active
            ? QString("QPushButton { background: %1; color: #000; border: none;"
                      "border-radius: 4px; padding: 0 10px; font-size: 11px; font-weight: 700; }").arg(MINT)
            : QString("QPushButton { background: %1; color: %2; border: 1px solid %3;"
                      "border-radius: 4px; padding: 0 10px; font-size: 11px; }"
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
    QString url = item->data(Qt::UserRole).toString();
    if (!url.isEmpty() && url.startsWith("http"))
        emit playRequested(url);
}

void OriginalsWidget::onPlayAllClicked() {
    emit playlistRequested(m3uUrl_);
}

void OriginalsWidget::onYouTubeClicked() {
    auto* cur = listWidget_->currentItem();
    if (!cur) return;
    QString ytUrl = cur->data(Qt::UserRole + 1).toString();
    if (!ytUrl.isEmpty())
        QDesktopServices::openUrl(QUrl(ytUrl));
}

// ── 공개 메서드 ──────────────────────────────────────────────────────────────
void OriginalsWidget::setCurrentFile(const QString& filePath) {
    currentFile_ = filePath;
    updateList();  // 재생 중 표시 갱신
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
    // 중앙 하단 배치
    int x = (width() - toastLabel_->width()) / 2;
    int y = height() - toastLabel_->height() - 48;
    toastLabel_->move(x, y);
    toastLabel_->setVisible(true);
    toastLabel_->raise();
    toastTimer_->start(4000);
}
