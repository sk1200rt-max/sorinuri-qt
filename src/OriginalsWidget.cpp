#include "OriginalsWidget.h"
#include <QSignalBlocker>
#include <QSet>
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
#include <QRegularExpression>
#include <algorithm>

namespace {
const QStringList SITUATION_PRIORITY = {
    "봄여름", "가을", "겨울", "비오는날", "새벽", "카페", "운동", "산책",
    "드라이브", "캠핑", "일상", "사랑", "이별", "위로", "힐링", "감성", "에너지"
};
const QStringList GENRE_PRIORITY = {
    "K-POP", "인디팝", "팝", "발라드", "감성발라드", "록발라드", "소프트록",
    "전자록", "하드록", "재즈", "R&B", "R&B팝록", "Lo-Fi", "파워팝", "시네마틱", "오케스트라"
};
const QStringList NON_SITUATION_METADATA = {
    "남성보컬", "여성보컬", "여성그룹", "피아노훅", "신스훅", "기타솔로", "허스키보컬"
};
const QStringList BRIGHT_MOODS = {
    "신나는", "활기찬", "밝은", "밝음", "경쾌한", "에너지", "설렘", "happy", "bright"
};
const QStringList CALM_MOODS = {
    "잔잔한", "차분한", "힐링", "평화로운", "아늑함", "따뜻함", "몽환적", "감성적",
    "comforting", "nostalgic", "calm", "warm"
};

QString normalizedToken(const QString& value) {
    return value.trimmed().toCaseFolded();
}

bool tokenEquals(const QString& left, const QString& right) {
    return normalizedToken(left) == normalizedToken(right);
}

bool containsToken(const QStringList& values, const QString& candidate) {
    return std::any_of(values.cbegin(), values.cend(), [&candidate](const QString& value) {
        return tokenEquals(value, candidate);
    });
}

void appendUniqueToken(QStringList& values, const QString& token) {
    const QString trimmed = token.trimmed();
    if (!trimmed.isEmpty() && !containsToken(values, trimmed)) values.append(trimmed);
}

bool containsMoodKeyword(const QString& mood, const QStringList& keywords) {
    const QString normalized = mood.toCaseFolded();
    return std::any_of(keywords.cbegin(), keywords.cend(), [&normalized](const QString& keyword) {
        return normalized.contains(keyword.toCaseFolded());
    });
}

bool isGenreToken(const QString& token) {
    return containsToken(GENRE_PRIORITY, token);
}

bool isSituationToken(const QString& token) {
    return !isGenreToken(token) && !containsToken(NON_SITUATION_METADATA, token);
}

QString normalizedSituationAlias(const QString& token) {
    const QString normalized = normalizedToken(token);
    if (normalized == QStringLiteral("비") || normalized == QStringLiteral("비오는 날")) return QStringLiteral("비오는날");
    if (normalized == QStringLiteral("달리기")) return QStringLiteral("운동");
    if (normalized == QStringLiteral("밤")) return QStringLiteral("새벽");
    return token.trimmed();
}

QStringList splitMetadataValue(const QJsonValue& value) {
    QStringList values;
    if (value.isArray()) {
        for (const QJsonValue& item : value.toArray()) appendUniqueToken(values, item.toString());
    } else if (value.isString()) {
        const QStringList parts = value.toString().split(QRegularExpression(QStringLiteral("[,;/|]")), Qt::SkipEmptyParts);
        for (const QString& part : parts) appendUniqueToken(values, part);
    }
    return values;
}

QString firstMetadataLabel(const QStringList& values) {
    return values.isEmpty() ? QString() : values.first();
}
}

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
static const int ROLE_GENRES   = Qt::UserRole + 7;
static const int ROLE_SITUATIONS = Qt::UserRole + 8;

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

    QString title      = index.data(ROLE_TITLE).toString();
    QString artist     = index.data(ROLE_ARTIST).toString();
    QString duration   = index.data(ROLE_DURATION).toString();
    const QString genreLabel = index.data(ROLE_GENRES).toString();
    const QString situationLabel = index.data(ROLE_SITUATIONS).toString();
    const QString metadata = genreLabel.isEmpty() ? situationLabel : genreLabel;
    const QString subtitle = metadata.isEmpty() ? artist : QStringLiteral("%1  ·  %2").arg(artist, metadata);

    // 제목
    QFont titleFont;
    titleFont.setPixelSize(13);
    titleFont.setBold(isPlaying);
    painter->setFont(titleFont);
    painter->setPen(isPlaying ? QColor(MINT) : QColor(TEXT));
    QRect titleRect(textX, r.top() + 11, textW, 18);
    QString elidedTitle = QFontMetrics(titleFont).elidedText(title, Qt::ElideRight, textW);
    painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter, elidedTitle);

    // 아티스트 · 장르(없으면 상황)
    QFont artistFont;
    artistFont.setPixelSize(11);
    painter->setFont(artistFont);
    painter->setPen(QColor(TEXT2));
    QRect artistRect(textX, r.top() + 33, textW, 16);
    const QString elidedSubtitle = QFontMetrics(artistFont).elidedText(subtitle, Qt::ElideRight, textW);
    painter->drawText(artistRect, Qt::AlignLeft | Qt::AlignVCenter, elidedSubtitle);

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
        int dotX = textX + QFontMetrics(artistFont).horizontalAdvance(elidedSubtitle) + 8;
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
    sortCombo_->addItems({"기본순", "제목순", "장르순", "상황순", "밝은 곡", "잔잔한 곡"});
    sortCombo_->setFixedWidth(94);
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

    // ── 전체·상황별·장르별 탐색 모드 ────────────────────────────────────
    auto* browseBar = new QWidget(this);
    browseBar->setFixedHeight(36);
    browseBar->setStyleSheet(QString("background: %1; border-bottom: 1px solid %2;")
                               .arg(BG).arg(BORDER));
    auto* browseRow = new QHBoxLayout(browseBar);
    browseRow->setContentsMargins(10, 4, 10, 4);
    browseRow->setSpacing(5);
    const QStringList browseLabels = {"전체", "상황별", "장르별"};
    for (int mode = 0; mode < browseLabels.size(); ++mode) {
        auto* btn = new QPushButton(browseLabels.at(mode), browseBar);
        btn->setFixedHeight(25);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFocusPolicy(Qt::NoFocus);
        connect(btn, &QPushButton::clicked, this, [this, mode]() { onBrowseModeClicked(mode); });
        browseBtns_.append(btn);
        browseRow->addWidget(btn);
    }
    browseRow->addStretch();
    root->addWidget(browseBar);
    updateBrowseButtons();

    // ── 선택한 탐색 모드의 실제 데이터 기반 필터 바 ─────────────────────
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
    catRow->addStretch();
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
    listWidget_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    listWidget_->setSpacing(0);
    listWidget_->setUniformItemSizes(true);

    // 커스텀 델리게이트 설정
    delegate_ = new SongItemDelegate(this);
    listWidget_->setItemDelegate(delegate_);

    connect(listWidget_, &QListWidget::itemDoubleClicked,
            this, &OriginalsWidget::onItemDoubleClicked);
    connect(listWidget_, &QListWidget::itemSelectionChanged,
            this, &OriginalsWidget::onSelectionChanged);
    root->addWidget(listWidget_, 1);

    // ── 하단 재생·재생목록 바 ─────────────────────────────────────────────
    auto* actionBar = new QWidget(this);
    actionBar->setFixedHeight(82);
    actionBar->setStyleSheet(QString("background: %1; border-top: 1px solid %2;")
                              .arg(BG2).arg(BORDER));
    auto* actionLayout = new QVBoxLayout(actionBar);
    actionLayout->setContentsMargins(10, 6, 10, 6);
    actionLayout->setSpacing(5);
    auto* playRow = new QHBoxLayout();
    auto* listRow = new QHBoxLayout();
    playRow->setSpacing(8);
    listRow->setSpacing(6);

    auto* playAllBtn = new QPushButton("▶  현재 목록 전체 재생", actionBar);
    playAllBtn->setFixedHeight(30);
    playAllBtn->setCursor(Qt::PointingHandCursor);
    playAllBtn->setFocusPolicy(Qt::NoFocus);
    playAllBtn->setStyleSheet(QString(
        "QPushButton { background: %1; color: #000; border: none;"
        "border-radius: 6px; padding: 0 14px; font-size: 11px; font-weight: 700; }"
        "QPushButton:hover { background: #00f0cc; }"
        "QPushButton:pressed { background: #00b89a; }").arg(MINT));
    connect(playAllBtn, &QPushButton::clicked, this, &OriginalsWidget::onPlayAllClicked);

    playSelectedBtn_ = new QPushButton("▶  선택 재생 (0)", actionBar);
    playSelectedBtn_->setFixedHeight(30);
    playSelectedBtn_->setCursor(Qt::PointingHandCursor);
    playSelectedBtn_->setFocusPolicy(Qt::NoFocus);
    playSelectedBtn_->setStyleSheet(QString(
        "QPushButton { background: %1; color: %2; border: 1px solid %3;"
        "border-radius: 6px; padding: 0 12px; font-size: 11px; font-weight: 600; }"
        "QPushButton:hover:enabled { border-color: %4; color: %4; }"
        "QPushButton:disabled { color: #555; border-color: #292929; }")
        .arg(BG3).arg(TEXT).arg(BORDER).arg(MINT));
    connect(playSelectedBtn_, &QPushButton::clicked, this, &OriginalsWidget::onPlaySelectedClicked);
    updatePlayButtons();

    auto* ytBtn = new QPushButton("▶  YouTube 전체", actionBar);
    ytBtn->setFixedHeight(30);
    ytBtn->setCursor(Qt::PointingHandCursor);
    ytBtn->setFocusPolicy(Qt::NoFocus);
    ytBtn->setStyleSheet(
        "QPushButton { background: #cc0000; color: #fff; border: none;"
        "border-radius: 6px; padding: 0 12px; font-size: 11px; font-weight: 600; }"
        "QPushButton:hover { background: #e00000; }"
        "QPushButton:pressed { background: #aa0000; }");
    connect(ytBtn, &QPushButton::clicked, this, &OriginalsWidget::onYouTubeClicked);

    repeatBtn_ = new QPushButton(actionBar);
    repeatBtn_->setFixedHeight(30);
    repeatBtn_->setCursor(Qt::PointingHandCursor);
    repeatBtn_->setFocusPolicy(Qt::NoFocus);
    repeatBtn_->setStyleSheet(QString(
        "QPushButton { background: %1; color: %2; border: 1px solid %3; border-radius: 6px;"
        "padding: 0 10px; font-size: 11px; } QPushButton:hover { border-color: %4; color: %4; }")
        .arg(BG3).arg(TEXT).arg(BORDER).arg(MINT));
    connect(repeatBtn_, &QPushButton::clicked, this, &OriginalsWidget::onRepeatModeClicked);
    setRepeatMode(repeatMode_);

    auto* saveBtn = new QPushButton("＋ 현재 목록 저장", actionBar);
    saveBtn->setFixedHeight(28);
    saveBtn->setCursor(Qt::PointingHandCursor);
    saveBtn->setFocusPolicy(Qt::NoFocus);
    saveBtn->setStyleSheet(QString("QPushButton { background: transparent; color: %1; border: 1px solid %2;"
        "border-radius: 5px; padding: 0 9px; font-size: 10px; } QPushButton:hover { border-color: %1; }")
        .arg(MINT).arg(BORDER));
    connect(saveBtn, &QPushButton::clicked, this, &OriginalsWidget::onSavePlaylistClicked);

    savedPlaylistCombo_ = new QComboBox(actionBar);
    savedPlaylistCombo_->setMinimumWidth(150);
    savedPlaylistCombo_->setFixedHeight(28);
    savedPlaylistCombo_->addItem("내 재생목록 선택");
    savedPlaylistCombo_->setStyleSheet(QString("QComboBox { background: %1; color: %2; border: 1px solid %3;"
        "border-radius: 5px; padding: 2px 8px; font-size: 10px; } QComboBox QAbstractItemView {"
        "background: %1; color: %2; selection-background-color: %4; }")
        .arg(BG3).arg(TEXT).arg(BORDER).arg(MINT));

    auto* loadBtn = new QPushButton("재생", actionBar);
    auto* deleteBtn = new QPushButton("삭제", actionBar);
    for (QPushButton* btn : {loadBtn, deleteBtn}) {
        btn->setFixedHeight(28);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFocusPolicy(Qt::NoFocus);
        btn->setStyleSheet(QString("QPushButton { background: %1; color: %2; border: 1px solid %3;"
            "border-radius: 5px; padding: 0 9px; font-size: 10px; } QPushButton:hover { border-color: %4; color: %4; }")
            .arg(BG3).arg(TEXT2).arg(BORDER).arg(MINT));
    }
    connect(loadBtn, &QPushButton::clicked, this, &OriginalsWidget::onLoadSavedPlaylistClicked);
    connect(deleteBtn, &QPushButton::clicked, this, &OriginalsWidget::onDeleteSavedPlaylistClicked);

    playRow->addWidget(playAllBtn);
    playRow->addWidget(playSelectedBtn_);
    playRow->addWidget(ytBtn);
    playRow->addStretch();
    playRow->addWidget(repeatBtn_);
    listRow->addWidget(saveBtn);
    listRow->addWidget(savedPlaylistCombo_, 1);
    listRow->addWidget(loadBtn);
    listRow->addWidget(deleteBtn);
    actionLayout->addLayout(playRow);
    actionLayout->addLayout(listRow);
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
        s.audioUrl   = obj["audio_url"].toString();
        s.mood       = obj["mood"].toString();

        for (const QString& category : splitMetadataValue(obj.value("categories")))
            appendUniqueToken(s.categories, category);
        for (const QString& category : splitMetadataValue(obj.value("category")))
            appendUniqueToken(s.categories, category);
        for (const QString& tag : splitMetadataValue(obj.value("tags")))
            appendUniqueToken(s.tags, tag);
        for (const QString& genre : splitMetadataValue(obj.value("genre")))
            appendUniqueToken(s.genres, genre);
        for (const QString& genre : splitMetadataValue(obj.value("genres")))
            appendUniqueToken(s.genres, genre);
        for (const QString& situation : splitMetadataValue(obj.value("situations")))
            appendUniqueToken(s.situations, situation);
        for (const QString& category : s.categories) {
            if (isGenreToken(category)) appendUniqueToken(s.genres, category);
        }
        for (const QString& tag : s.tags) {
            if (isGenreToken(tag)) appendUniqueToken(s.genres, tag);
        }
        songs_.append(s);
    }

    statusLabel_->setText(QString("%1곡").arg(songs_.size()));
    rebuildCategoryButtons();
    applyFilter();

    // 썸네일 fetch (처음 30개 즉시)
    int limit = qMin(songs_.size(), 30);
    for (int i = 0; i < limit; ++i)
        fetchThumbnail(songs_[i]);
}

// ── 썸네일 fetch ──────────────────────────────────────────────────────────────
void OriginalsWidget::fetchThumbnail(const SongInfo& song) {
    if (song.cover.isEmpty()) return;
    QUrl thumbUrl(absoluteUrl(song.cover));
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

QString OriginalsWidget::absoluteUrl(const QString& url) const {
    const QString trimmed = url.trimmed();
    if (trimmed.isEmpty() || trimmed.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive)
        || trimmed.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive)) {
        return trimmed;
    }
    return trimmed.startsWith(QLatin1Char('/')) ? baseUrl_ + trimmed
                                                : baseUrl_ + QLatin1Char('/') + trimmed;
}

QString OriginalsWidget::mediaUrlForSong(const SongInfo& song) const {
    return absoluteUrl(song.audioUrl.isEmpty() ? song.mp3 : song.audioUrl);
}

QStringList OriginalsWidget::situationsForSong(const SongInfo& song) const {
    // Schema v2 has an explicitly curated browsing taxonomy. Only older
    // catalogs need category/tag inference as a backward-compatible fallback.
    if (!song.situations.isEmpty()) return song.situations;
    QStringList situations;
    for (const QString& category : song.categories) {
        const QString normalized = normalizedSituationAlias(category);
        if (isSituationToken(normalized)) appendUniqueToken(situations, normalized);
    }
    for (const QString& tag : song.tags) {
        const QString normalized = normalizedSituationAlias(tag);
        if (containsToken(SITUATION_PRIORITY, normalized)) appendUniqueToken(situations, normalized);
    }
    return situations;
}

QStringList OriginalsWidget::genresForSong(const SongInfo& song) const {
    // Schema v2 genres are authoritative; fallback retains compatibility with
    // existing server catalogs that only have category/tag values.
    if (!song.genres.isEmpty()) return song.genres;
    QStringList genres;
    for (const QString& category : song.categories) {
        if (isGenreToken(category)) appendUniqueToken(genres, category);
    }
    for (const QString& tag : song.tags) {
        if (isGenreToken(tag)) appendUniqueToken(genres, tag);
    }
    return genres;
}

void OriginalsWidget::updateBrowseButtons() {
    for (int index = 0; index < browseBtns_.size(); ++index) {
        const bool active = static_cast<int>(browseMode_) == index;
        browseBtns_.at(index)->setStyleSheet(active
            ? QString("QPushButton { background: %1; color: #000; border: none; border-radius: 12px;"
                      "padding: 0 13px; font-size: 11px; font-weight: 700; }").arg(MINT)
            : QString("QPushButton { background: %1; color: %2; border: 1px solid %3; border-radius: 12px;"
                      "padding: 0 13px; font-size: 11px; } QPushButton:hover { border-color: %4; color: %4; }")
                      .arg(BG2).arg(TEXT2).arg(BORDER).arg(MINT));
    }
}

void OriginalsWidget::rebuildCategoryButtons() {
    if (!catBar_ || !catBar_->layout()) return;
    auto* layout = qobject_cast<QHBoxLayout*>(catBar_->layout());
    if (!layout) return;

    while (QLayoutItem* child = layout->takeAt(0)) {
        if (QWidget* widget = child->widget()) widget->deleteLater();
        delete child;
    }
    catBtns_.clear();

    QStringList discovered;
    if (browseMode_ == BrowseMode::Situation) {
        for (const SongInfo& song : songs_) {
            for (const QString& situation : situationsForSong(song)) appendUniqueToken(discovered, situation);
        }
    } else if (browseMode_ == BrowseMode::Genre) {
        for (const SongInfo& song : songs_) {
            for (const QString& genre : genresForSong(song)) appendUniqueToken(discovered, genre);
        }
    }

    QStringList categories = {QStringLiteral("전체")};
    const QStringList& priority = browseMode_ == BrowseMode::Genre ? GENRE_PRIORITY : SITUATION_PRIORITY;
    for (const QString& category : priority) {
        if (containsToken(discovered, category)) appendUniqueToken(categories, category);
    }
    QStringList remaining;
    for (const QString& category : discovered) {
        if (!containsToken(categories, category)) remaining.append(category);
    }
    std::sort(remaining.begin(), remaining.end(), [](const QString& left, const QString& right) {
        return QString::localeAwareCompare(left, right) < 0;
    });
    for (const QString& category : remaining) appendUniqueToken(categories, category);

    if (!containsToken(categories, activeCategory_)) activeCategory_ = QStringLiteral("전체");
    for (const QString& category : categories) {
        auto* button = new QPushButton(category, catBar_);
        button->setFixedHeight(26);
        button->setCursor(Qt::PointingHandCursor);
        button->setFocusPolicy(Qt::NoFocus);
        const bool active = tokenEquals(category, activeCategory_);
        button->setStyleSheet(active
            ? QString("QPushButton { background: %1; color: #000; border: none; border-radius: 13px;"
                      "padding: 0 12px; font-size: 11px; font-weight: 700; }").arg(MINT)
            : QString("QPushButton { background: %1; color: %2; border: 1px solid %3; border-radius: 13px;"
                      "padding: 0 12px; font-size: 11px; } QPushButton:hover { border-color: %4; color: %4; }")
                      .arg(BG2).arg(TEXT2).arg(BORDER).arg(MINT));
        connect(button, &QPushButton::clicked, this, [this, category]() { onCategoryClicked(category); });
        catBtns_.append(button);
        layout->addWidget(button);
    }
    layout->addStretch();
    catBar_->setMinimumWidth(qMax(1, categories.size()) * 82);
}

void OriginalsWidget::updatePlayButtons() {
    if (!playSelectedBtn_ || !listWidget_) return;
    const int count = listWidget_->selectedItems().size();
    playSelectedBtn_->setText(QStringLiteral("▶  선택 재생 (%1)").arg(count));
    playSelectedBtn_->setEnabled(count > 0);
}

// ── 필터 적용 ─────────────────────────────────────────────────────────────────
void OriginalsWidget::applyFilter() {
    filtered_.clear();
    const QString search = searchText_.trimmed().toCaseFolded();

    for (const SongInfo& song : songs_) {
        const QStringList situations = situationsForSong(song);
        const QStringList genres = genresForSong(song);
        if (!tokenEquals(activeCategory_, QStringLiteral("전체"))) {
            const QStringList filterValues = browseMode_ == BrowseMode::Genre ? genres : situations;
            if (!containsToken(filterValues, activeCategory_)) continue;
        }
        if (!search.isEmpty()) {
            const QString searchable = QStringList{
                song.title, song.artist, song.mood, song.tags.join(QLatin1Char(' ')),
                situations.join(QLatin1Char(' ')), genres.join(QLatin1Char(' '))
            }.join(QLatin1Char(' ')).toCaseFolded();
            if (!searchable.contains(search)) continue;
        }
        filtered_.append(song);
    }

    if (sortIndex_ == 1) {
        std::stable_sort(filtered_.begin(), filtered_.end(), [](const SongInfo& left, const SongInfo& right) {
            return QString::localeAwareCompare(left.title, right.title) < 0;
        });
    } else if (sortIndex_ == 2) {
        std::stable_sort(filtered_.begin(), filtered_.end(), [this](const SongInfo& left, const SongInfo& right) {
            return QString::localeAwareCompare(firstMetadataLabel(genresForSong(left)), firstMetadataLabel(genresForSong(right))) < 0;
        });
    } else if (sortIndex_ == 3) {
        std::stable_sort(filtered_.begin(), filtered_.end(), [this](const SongInfo& left, const SongInfo& right) {
            return QString::localeAwareCompare(firstMetadataLabel(situationsForSong(left)), firstMetadataLabel(situationsForSong(right))) < 0;
        });
    } else if (sortIndex_ == 4) {
        std::stable_sort(filtered_.begin(), filtered_.end(), [](const SongInfo& left, const SongInfo& right) {
            return containsMoodKeyword(left.mood, BRIGHT_MOODS) > containsMoodKeyword(right.mood, BRIGHT_MOODS);
        });
    } else if (sortIndex_ == 5) {
        std::stable_sort(filtered_.begin(), filtered_.end(), [](const SongInfo& left, const SongInfo& right) {
            return containsMoodKeyword(left.mood, CALM_MOODS) > containsMoodKeyword(right.mood, CALM_MOODS);
        });
    }

    updateList();
}

void OriginalsWidget::updateList() {
    listWidget_->clear();

    if (filtered_.isEmpty()) {
        auto* item = new QListWidgetItem("  선택한 조건에 재생 가능한 곡이 없습니다.");
        item->setForeground(QColor(TEXT2));
        listWidget_->addItem(item);
        updatePlayButtons();
        return;
    }

    for (const SongInfo& song : filtered_) {
        const QString mediaUrl = mediaUrlForSong(song);
        const QStringList situations = situationsForSong(song);
        const QStringList genres = genresForSong(song);
        const bool isPlaying = (!currentFile_.isEmpty() && currentFile_ == mediaUrl);

        auto* item = new QListWidgetItem();
        item->setData(ROLE_URL, mediaUrl);
        item->setData(ROLE_YTURL, absoluteUrl(song.youtubeUrl));
        item->setData(ROLE_COVER, song.cover);
        item->setData(ROLE_TITLE, song.title);
        item->setData(ROLE_ARTIST, song.artist.isEmpty() ? QStringLiteral("소리누리") : song.artist);
        item->setData(ROLE_DURATION, song.duration);
        item->setData(ROLE_PLAYING, isPlaying);
        item->setData(ROLE_GENRES, genres.join(QStringLiteral(" · ")));
        item->setData(ROLE_SITUATIONS, situations.join(QStringLiteral(" · ")));
        item->setToolTip(QString("장르: %1\n상황: %2\n분위기: %3\n태그: %4")
            .arg(genres.isEmpty() ? QStringLiteral("미분류") : genres.join(QStringLiteral(", ")))
            .arg(situations.isEmpty() ? QStringLiteral("미분류") : situations.join(QStringLiteral(", ")))
            .arg(song.mood)
            .arg(song.tags.join(QStringLiteral(", "))));
        listWidget_->addItem(item);
    }

    delegate_->setCurrentUrl(currentFile_);
    updatePlayButtons();
}

// ── 슬롯 ─────────────────────────────────────────────────────────────────────
void OriginalsWidget::onSearchChanged(const QString& text) {
    searchText_ = text;
    applyFilter();
}

void OriginalsWidget::onBrowseModeClicked(int mode) {
    if (mode < static_cast<int>(BrowseMode::All) || mode > static_cast<int>(BrowseMode::Genre)) return;
    browseMode_ = static_cast<BrowseMode>(mode);
    activeCategory_ = QStringLiteral("전체");
    updateBrowseButtons();
    rebuildCategoryButtons();
    applyFilter();
}

void OriginalsWidget::onCategoryClicked(const QString& category) {
    activeCategory_ = category;
    rebuildCategoryButtons();
    applyFilter();
}

void OriginalsWidget::onSortChanged(int index) {
    sortIndex_ = index;
    applyFilter();
}

void OriginalsWidget::onSelectionChanged() {
    updatePlayButtons();
}

QList<PlaybackQueue::Entry> OriginalsWidget::queueEntries(bool useYouTube) const {
    QList<PlaybackQueue::Entry> entries;
    for (const SongInfo& song : filtered_) {
        const QString mediaUrl = useYouTube ? absoluteUrl(song.youtubeUrl) : mediaUrlForSong(song);
        if (mediaUrl.isEmpty()) continue;
        PlaybackQueue::Entry entry;
        entry.url = mediaUrl;
        entry.title = song.title;
        entry.artist = song.artist.isEmpty() ? QStringLiteral("소리누리") : song.artist;
        entry.artworkUrl = absoluteUrl(song.cover);
        entry.source = useYouTube ? QStringLiteral("youtube") : QStringLiteral("originals");
        entries.append(entry);
    }
    return entries;
}

QList<PlaybackQueue::Entry> OriginalsWidget::selectedQueueEntries(bool useYouTube) const {
    if (!listWidget_) return {};
    QSet<QString> selectedUrls;
    for (QListWidgetItem* item : listWidget_->selectedItems()) {
        const QString url = item->data(useYouTube ? ROLE_YTURL : ROLE_URL).toString();
        if (!url.isEmpty()) selectedUrls.insert(url);
    }
    QList<PlaybackQueue::Entry> entries;
    for (const PlaybackQueue::Entry& entry : queueEntries(useYouTube)) {
        if (selectedUrls.contains(entry.url)) entries.append(entry);
    }
    return entries;
}

void OriginalsWidget::onItemDoubleClicked(QListWidgetItem* item) {
    if (!item) return;
    QList<PlaybackQueue::Entry> entries = selectedQueueEntries(false);
    if (entries.isEmpty()) entries = queueEntries(false);
    const QString clickedUrl = item->data(ROLE_URL).toString();
    const int startIndex = std::distance(entries.cbegin(), std::find_if(entries.cbegin(), entries.cend(),
        [&clickedUrl](const PlaybackQueue::Entry& entry) { return entry.url == clickedUrl; }));
    if (!entries.isEmpty() && startIndex >= 0 && startIndex < entries.size())
        emit queueRequested(entries, startIndex);
}

void OriginalsWidget::onPlayAllClicked() {
    const QList<PlaybackQueue::Entry> entries = queueEntries(false);
    if (entries.isEmpty()) {
        showToast("현재 목록에 재생 가능한 곡이 없습니다.");
        return;
    }
    emit queueRequested(entries, 0);
}

void OriginalsWidget::onPlaySelectedClicked() {
    const QList<PlaybackQueue::Entry> entries = selectedQueueEntries(false);
    if (entries.isEmpty()) {
        showToast("재생할 곡을 선택해 주세요.");
        return;
    }
    emit queueRequested(entries, 0);
}

void OriginalsWidget::onYouTubeClicked() {
    const QList<PlaybackQueue::Entry> entries = queueEntries(true);
    if (entries.isEmpty()) {
        showToast("YouTube 연결 곡이 없습니다.");
        return;
    }

    int startIndex = 0;
    if (auto* current = listWidget_->currentItem()) {
        const QString selectedUrl = current->data(ROLE_YTURL).toString();
        for (int i = 0; i < entries.size(); ++i) {
            if (entries.at(i).url == selectedUrl) {
                startIndex = i;
                break;
            }
        }
    }
    emit queueRequested(entries, startIndex);
}

void OriginalsWidget::onSavePlaylistClicked() {
    QList<PlaybackQueue::Entry> entries = selectedQueueEntries(false);
    if (entries.isEmpty()) entries = queueEntries(false);
    if (entries.isEmpty()) {
        showToast("저장할 곡이 없습니다.");
        return;
    }
    emit savePlaylistRequested(entries);
}

void OriginalsWidget::onLoadSavedPlaylistClicked() {
    if (!savedPlaylistCombo_ || savedPlaylistCombo_->currentIndex() <= 0) {
        showToast("재생목록을 선택해 주세요.");
        return;
    }
    emit loadSavedPlaylistRequested(savedPlaylistCombo_->currentText());
}

void OriginalsWidget::onDeleteSavedPlaylistClicked() {
    if (!savedPlaylistCombo_ || savedPlaylistCombo_->currentIndex() <= 0) {
        showToast("삭제할 재생목록을 선택해 주세요.");
        return;
    }
    emit deleteSavedPlaylistRequested(savedPlaylistCombo_->currentText());
}

void OriginalsWidget::onRepeatModeClicked() {
    const int next = (static_cast<int>(repeatMode_) + 1) % 3;
    emit repeatModeRequested(static_cast<PlaybackQueue::RepeatMode>(next));
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

void OriginalsWidget::setSavedPlaylistNames(const QStringList& names) {
    if (!savedPlaylistCombo_) return;
    const QString selected = savedPlaylistCombo_->currentText();
    QSignalBlocker blocker(savedPlaylistCombo_);
    savedPlaylistCombo_->clear();
    savedPlaylistCombo_->addItem("내 재생목록 선택");
    savedPlaylistCombo_->addItems(names);
    const int selectedIndex = savedPlaylistCombo_->findText(selected);
    if (selectedIndex > 0) savedPlaylistCombo_->setCurrentIndex(selectedIndex);
}

void OriginalsWidget::setRepeatMode(PlaybackQueue::RepeatMode mode) {
    repeatMode_ = mode;
    if (!repeatBtn_) return;
    const QString text = mode == PlaybackQueue::RepeatMode::All ? QStringLiteral("🔁 전체 반복")
                       : mode == PlaybackQueue::RepeatMode::One ? QStringLiteral("🔂 한 곡 반복")
                       : QStringLiteral("🔁 반복 끔");
    repeatBtn_->setText(text);
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
