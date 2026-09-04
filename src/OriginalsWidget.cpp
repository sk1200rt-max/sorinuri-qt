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
static const int ROLE_SELECTION_MODE = Qt::UserRole + 9;

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

    // ── 선택 모드 체크 표시 + 썸네일 ──────────────────────────────────────
    const bool selectionMode = index.data(ROLE_SELECTION_MODE).toBool();
    int leftPad = PAD + (isPlaying ? 5 : 2);
    if (selectionMode) {
        const QRect checkRect(r.left() + leftPad, r.top() + (r.height() - 18) / 2, 18, 18);
        painter->setPen(QPen(QColor(isSelected ? MINT : TEXT2), 1.5));
        painter->setBrush(isSelected ? QColor(MINT) : Qt::NoBrush);
        painter->drawRoundedRect(checkRect, 5, 5);
        if (isSelected) {
            painter->setPen(QPen(QColor("#06201c"), 2));
            painter->drawLine(checkRect.left() + 4, checkRect.center().y(), checkRect.left() + 8, checkRect.bottom() - 4);
            painter->drawLine(checkRect.left() + 8, checkRect.bottom() - 4, checkRect.right() - 3, checkRect.top() + 4);
        }
        leftPad += 25;
    }
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
    // 오리지널은 설정 바를 나열하는 화면이 아니라, 선택하고 바로 감상하는 독립 카탈로그다.
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* catalogHeader = new QWidget(this);
    catalogHeader->setObjectName("originalsCatalogHeader");
    catalogHeader->setStyleSheet(
        "QWidget#originalsCatalogHeader{background:#0D1617;border-bottom:1px solid #203231;}"
        "QLabel{background:transparent;}");
    auto* headerLayout = new QVBoxLayout(catalogHeader);
    headerLayout->setContentsMargins(32, 22, 32, 16);
    headerLayout->setSpacing(13);
    auto* titleRow = new QHBoxLayout();
    titleRow->setContentsMargins(0, 0, 0, 0);
    auto* titleBlock = new QVBoxLayout();
    titleBlock->setContentsMargins(0, 0, 0, 0);
    titleBlock->setSpacing(3);
    auto* eyebrow = new QLabel(QStringLiteral("SORINURI ORIGINALS  ·  CURATED MUSIC"), catalogHeader);
    eyebrow->setStyleSheet("color:#00D4B4;font-size:10px;font-weight:800;letter-spacing:1.5px;");
    auto* title = new QLabel(QStringLiteral("당신의 순간에 맞는 음악"), catalogHeader);
    title->setStyleSheet("color:#F1F7F5;font-size:26px;font-weight:750;letter-spacing:-0.4px;");
    auto* subtitle = new QLabel(QStringLiteral("장르와 상황으로 고르고, 선택한 곡만 바로 이어서 들을 수 있습니다."), catalogHeader);
    subtitle->setStyleSheet("color:#8FA39F;font-size:12px;");
    titleBlock->addWidget(eyebrow);
    titleBlock->addWidget(title);
    titleBlock->addWidget(subtitle);
    titleRow->addLayout(titleBlock, 1);
    statusLabel_ = new QLabel(catalogHeader);
    statusLabel_->setStyleSheet("color:#829793;font-size:11px;font-weight:700;padding:5px 9px;background:#132221;border:1px solid #29423E;border-radius:9px;");
    titleRow->addWidget(statusLabel_, 0, Qt::AlignTop);
    headerLayout->addLayout(titleRow);

    auto* commandRow = new QHBoxLayout();
    commandRow->setContentsMargins(0, 0, 0, 0);
    commandRow->setSpacing(8);
    searchEdit_ = new QLineEdit(catalogHeader);
    searchEdit_->setPlaceholderText(QStringLiteral("제목, 아티스트, 태그 검색"));
    searchEdit_->setMinimumWidth(280);
    searchEdit_->setFixedHeight(36);
    searchEdit_->setStyleSheet(
        "QLineEdit{background:#101D1D;color:#EAF5F2;border:1px solid #2B4541;border-radius:8px;padding:0 12px;font-size:12px;}"
        "QLineEdit:focus{border-color:#00D4B4;background:#132524;}");
    connect(searchEdit_, &QLineEdit::textChanged, this, &OriginalsWidget::onSearchChanged);
    sortCombo_ = new QComboBox(catalogHeader);
    sortCombo_->addItems({"기본순", "제목순", "장르순", "상황순", "밝은 곡", "잔잔한 곡"});
    sortCombo_->setFixedSize(100, 36);
    sortCombo_->setFocusPolicy(Qt::NoFocus);
    sortCombo_->setStyleSheet(
        "QComboBox{background:#132221;color:#C9D9D5;border:1px solid #2B4541;border-radius:8px;padding:0 10px;font-size:11px;font-weight:600;}"
        "QComboBox:hover{border-color:#00D4B4;}QComboBox::drop-down{border:none;}"
        "QComboBox QAbstractItemView{background:#12201F;color:#EAF5F2;selection-background-color:#19564C;border:1px solid #34514C;}");
    connect(sortCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OriginalsWidget::onSortChanged);
    selectModeBtn_ = new QPushButton(QStringLiteral("곡 선택"), catalogHeader);
    selectModeBtn_->setFixedHeight(36);
    selectModeBtn_->setCursor(Qt::PointingHandCursor);
    selectModeBtn_->setFocusPolicy(Qt::NoFocus);
    selectModeBtn_->setStyleSheet(
        "QPushButton{background:#17322E;color:#D5F5EE;border:1px solid #3D665D;border-radius:8px;padding:0 13px;font-size:11px;font-weight:750;}"
        "QPushButton:hover{background:#00B89D;color:#06221E;border-color:#00D4B4;}");
    connect(selectModeBtn_, &QPushButton::clicked, this, &OriginalsWidget::onSelectionModeClicked);
    commandRow->addWidget(searchEdit_, 1);
    commandRow->addWidget(sortCombo_);
    commandRow->addWidget(selectModeBtn_);
    headerLayout->addLayout(commandRow);
    root->addWidget(catalogHeader);

    auto* discoveryBar = new QWidget(this);
    discoveryBar->setObjectName("originalsDiscoveryBar");
    discoveryBar->setStyleSheet("QWidget#originalsDiscoveryBar{background:#0A1011;border-bottom:1px solid #1D2C2B;}");
    auto* discoveryLayout = new QVBoxLayout(discoveryBar);
    discoveryLayout->setContentsMargins(32, 10, 32, 9);
    discoveryLayout->setSpacing(7);
    auto* browseRow = new QHBoxLayout();
    browseRow->setContentsMargins(0, 0, 0, 0);
    browseRow->setSpacing(6);
    const QStringList browseLabels = {QStringLiteral("전체 음악"), QStringLiteral("상황별"), QStringLiteral("장르별")};
    for (int mode = 0; mode < browseLabels.size(); ++mode) {
        auto* button = new QPushButton(browseLabels.at(mode), discoveryBar);
        button->setFixedHeight(28);
        button->setCursor(Qt::PointingHandCursor);
        button->setFocusPolicy(Qt::NoFocus);
        connect(button, &QPushButton::clicked, this, [this, mode]() { onBrowseModeClicked(mode); });
        browseBtns_.append(button);
        browseRow->addWidget(button);
    }
    browseRow->addStretch(1);
    discoveryLayout->addLayout(browseRow);

    auto* catScroll = new QScrollArea(discoveryBar);
    catScroll->setFrameShape(QFrame::NoFrame);
    catScroll->setWidgetResizable(false);
    catScroll->setFixedHeight(34);
    catScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    catScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    catScroll->setStyleSheet("QScrollArea{background:transparent;border:none;}");
    catBar_ = new QWidget(catScroll);
    catBar_->setStyleSheet("background:transparent;");
    auto* catRow = new QHBoxLayout(catBar_);
    catRow->setContentsMargins(0, 0, 0, 0);
    catRow->setSpacing(5);
    catRow->addStretch(1);
    catScroll->setWidget(catBar_);
    discoveryLayout->addWidget(catScroll);
    root->addWidget(discoveryBar);
    updateBrowseButtons();

    auto* listFrame = new QWidget(this);
    listFrame->setObjectName("originalsListFrame");
    listFrame->setStyleSheet("QWidget#originalsListFrame{background:#0B1011;}");
    auto* listLayout = new QVBoxLayout(listFrame);
    listLayout->setContentsMargins(24, 12, 24, 0);
    listLayout->setSpacing(7);
    auto* listHeader = new QHBoxLayout();
    auto* listHint = new QLabel(QStringLiteral("곡을 두 번 클릭하면 바로 재생합니다"), listFrame);
    listHint->setStyleSheet("color:#637976;font-size:10px;");
    auto* listSortHint = new QLabel(QStringLiteral("선택 재생 · YouTube 연속 재생 지원"), listFrame);
    listSortHint->setStyleSheet("color:#6F8A85;font-size:10px;");
    listHeader->addWidget(listHint);
    listHeader->addStretch(1);
    listHeader->addWidget(listSortHint);
    listLayout->addLayout(listHeader);

    listWidget_ = new QListWidget(listFrame);
    listWidget_->setStyleSheet(
        "QListWidget{background:#0F1718;border:1px solid #1F3231;border-radius:10px;outline:none;padding:2px 0;}"
        "QListWidget::item{border:none;}QListWidget::item:selected{background:rgba(0,212,180,0.08);}"
        "QListWidget::item:hover{background:rgba(255,255,255,0.04);}"
        "QScrollBar:vertical{background:transparent;width:7px;}QScrollBar::handle:vertical{background:#2A403D;border-radius:3px;min-height:30px;}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}");
    listWidget_->setMouseTracking(true);
    listWidget_->setSelectionMode(QAbstractItemView::SingleSelection);
    listWidget_->setSpacing(0);
    listWidget_->setUniformItemSizes(true);
    delegate_ = new SongItemDelegate(this);
    listWidget_->setItemDelegate(delegate_);
    connect(listWidget_, &QListWidget::itemDoubleClicked, this, &OriginalsWidget::onItemDoubleClicked);
    connect(listWidget_, &QListWidget::itemSelectionChanged, this, &OriginalsWidget::onSelectionChanged);
    listLayout->addWidget(listWidget_, 1);
    root->addWidget(listFrame, 1);

    // 선택·반복·저장 재생목록은 카탈로그 밖의 큰 2단 바가 아니라 한 개의 감상 컨텍스트로 묶는다.
    auto* actionBar = new QWidget(this);
    actionBar->setObjectName("originalsActionContext");
    actionBar->setFixedHeight(72);
    actionBar->setStyleSheet("QWidget#originalsActionContext{background:#101A1A;border-top:1px solid #263B38;}");
    auto* actionLayout = new QHBoxLayout(actionBar);
    actionLayout->setContentsMargins(28, 12, 28, 12);
    actionLayout->setSpacing(8);

    auto makeAction = [actionBar](const QString& text, bool primary = false) {
        auto* button = new QPushButton(text, actionBar);
        button->setFixedHeight(34);
        button->setCursor(Qt::PointingHandCursor);
        button->setFocusPolicy(Qt::NoFocus);
        button->setStyleSheet(primary
            ? "QPushButton{background:#00C7AA;color:#05231E;border:1px solid #5BE3D1;border-radius:8px;padding:0 13px;font-size:11px;font-weight:800;}QPushButton:hover{background:#38DDC4;}"
            : "QPushButton{background:#152725;color:#C7DAD6;border:1px solid #314C47;border-radius:8px;padding:0 11px;font-size:11px;font-weight:700;}QPushButton:hover{border-color:#00D4B4;color:#EEFFFB;background:#19332F;}");
        return button;
    };
    auto* playAllBtn = makeAction(QStringLiteral("현재 목록 재생"), true);
    connect(playAllBtn, &QPushButton::clicked, this, &OriginalsWidget::onPlayAllClicked);
    playSelectedBtn_ = makeAction(QStringLiteral("선택 재생 (0)"));
    connect(playSelectedBtn_, &QPushButton::clicked, this, &OriginalsWidget::onPlaySelectedClicked);
    selectAllBtn_ = makeAction(QStringLiteral("전체 선택"));
    clearSelectionBtn_ = makeAction(QStringLiteral("선택 해제"));
    connect(selectAllBtn_, &QPushButton::clicked, this, &OriginalsWidget::onSelectAllClicked);
    connect(clearSelectionBtn_, &QPushButton::clicked, this, &OriginalsWidget::onClearSelectionClicked);
    youTubeBtn_ = makeAction(QStringLiteral("YouTube 전체 듣기"));
    youTubeBtn_->setStyleSheet(
        "QPushButton{background:#472024;color:#FFEAEA;border:1px solid #93474C;border-radius:8px;padding:0 12px;font-size:11px;font-weight:800;}"
        "QPushButton:hover{background:#B92F3B;border-color:#EA6771;}");
    connect(youTubeBtn_, &QPushButton::clicked, this, &OriginalsWidget::onYouTubeClicked);
    repeatBtn_ = makeAction(QStringLiteral("반복: 끔"));
    connect(repeatBtn_, &QPushButton::clicked, this, &OriginalsWidget::onRepeatModeClicked);
    setRepeatMode(repeatMode_);

    auto* saveBtn = makeAction(QStringLiteral("목록 저장"));
    connect(saveBtn, &QPushButton::clicked, this, &OriginalsWidget::onSavePlaylistClicked);
    savedPlaylistCombo_ = new QComboBox(actionBar);
    savedPlaylistCombo_->setMinimumWidth(150);
    savedPlaylistCombo_->setFixedHeight(34);
    savedPlaylistCombo_->addItem(QStringLiteral("내 재생목록"));
    savedPlaylistCombo_->setFocusPolicy(Qt::NoFocus);
    savedPlaylistCombo_->setStyleSheet(
        "QComboBox{background:#111F1E;color:#B9CDCA;border:1px solid #314C47;border-radius:8px;padding:0 9px;font-size:11px;}"
        "QComboBox:hover{border-color:#00D4B4;}QComboBox::drop-down{border:none;}"
        "QComboBox QAbstractItemView{background:#12201F;color:#EAF5F2;selection-background-color:#19564C;border:1px solid #34514C;}");
    auto* loadBtn = makeAction(QStringLiteral("재생"));
    auto* deleteBtn = makeAction(QStringLiteral("삭제"));
    connect(loadBtn, &QPushButton::clicked, this, &OriginalsWidget::onLoadSavedPlaylistClicked);
    connect(deleteBtn, &QPushButton::clicked, this, &OriginalsWidget::onDeleteSavedPlaylistClicked);

    actionLayout->addWidget(playAllBtn);
    actionLayout->addWidget(playSelectedBtn_);
    actionLayout->addWidget(selectAllBtn_);
    actionLayout->addWidget(clearSelectionBtn_);
    actionLayout->addWidget(youTubeBtn_);
    actionLayout->addStretch(1);
    actionLayout->addWidget(repeatBtn_);
    actionLayout->addWidget(saveBtn);
    actionLayout->addWidget(savedPlaylistCombo_);
    actionLayout->addWidget(loadBtn);
    actionLayout->addWidget(deleteBtn);
    root->addWidget(actionBar);

    toastLabel_ = new QLabel(this);
    toastLabel_->setStyleSheet("background:#00D4B4;color:#06221E;border-radius:8px;padding:8px 16px;font-size:12px;font-weight:800;");
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
    updateSelectionControls();
}

void OriginalsWidget::updateSelectionControls() {
    if (!listWidget_) return;
    const int count = listWidget_->selectedItems().size();
    if (playSelectedBtn_) {
        playSelectedBtn_->setText(QStringLiteral("▶  선택한 곡 재생 (%1)").arg(count));
        playSelectedBtn_->setEnabled(count > 0);
    }
    if (youTubeBtn_) {
        youTubeBtn_->setText(count > 0
            ? QStringLiteral("▶  YouTube 선택 듣기 (%1)").arg(count)
            : QStringLiteral("▶  YouTube 전체 듣기"));
    }
    const QString selectionStyle = selectionMode_
        ? QString("QPushButton { background: %1; color: #06201c; border: none; border-radius: 6px; padding: 0 10px; font-size: 10px; font-weight: 800; }").arg(MINT)
        : QString("QPushButton { background: %1; color: %2; border: 1px solid %3; border-radius: 6px; padding: 0 10px; font-size: 10px; font-weight: 700; } QPushButton:hover { border-color: %4; color: %4; }")
              .arg(BG3).arg(TEXT2).arg(BORDER).arg(MINT);
    if (selectModeBtn_) {
        selectModeBtn_->setText(selectionMode_ ? QStringLiteral("선택 중") : QStringLiteral("곡 선택"));
        selectModeBtn_->setStyleSheet(selectionStyle);
    }
    if (selectAllBtn_) selectAllBtn_->setEnabled(selectionMode_ && listWidget_->count() > 0);
    if (clearSelectionBtn_) clearSelectionBtn_->setEnabled(selectionMode_ && count > 0);
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
    QSet<QString> selectedUrls;
    if (listWidget_) {
        for (QListWidgetItem* selected : listWidget_->selectedItems()) {
            const QString url = selected->data(ROLE_URL).toString();
            if (!url.isEmpty()) selectedUrls.insert(url);
        }
        listWidget_->clear();
    }

    if (filtered_.isEmpty()) {
        auto* item = new QListWidgetItem("  선택한 조건에 재생 가능한 곡이 없습니다.");
        item->setForeground(QColor(TEXT2));
        listWidget_->addItem(item);
        updateSelectionControls();
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
        item->setData(ROLE_SELECTION_MODE, selectionMode_);
        if (selectionMode_ && selectedUrls.contains(mediaUrl)) item->setSelected(true);
        item->setToolTip(QString("장르: %1\n상황: %2\n분위기: %3\n태그: %4")
            .arg(genres.isEmpty() ? QStringLiteral("미분류") : genres.join(QStringLiteral(", ")))
            .arg(situations.isEmpty() ? QStringLiteral("미분류") : situations.join(QStringLiteral(", ")))
            .arg(song.mood)
            .arg(song.tags.join(QStringLiteral(", "))));
        listWidget_->addItem(item);
    }

    delegate_->setCurrentUrl(currentFile_);
    updateSelectionControls();
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
    updateSelectionControls();
    if (listWidget_) listWidget_->viewport()->update();
}

void OriginalsWidget::onSelectionModeClicked() {
    if (!listWidget_) return;
    selectionMode_ = !selectionMode_;
    listWidget_->setSelectionMode(selectionMode_
        ? QAbstractItemView::ExtendedSelection
        : QAbstractItemView::SingleSelection);
    if (!selectionMode_) listWidget_->clearSelection();
    updateList();
}

void OriginalsWidget::onSelectAllClicked() {
    if (!listWidget_ || !selectionMode_) return;
    listWidget_->selectAll();
    updateSelectionControls();
}

void OriginalsWidget::onClearSelectionClicked() {
    if (!listWidget_) return;
    listWidget_->clearSelection();
    updateSelectionControls();
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
    QList<PlaybackQueue::Entry> entries = selectedQueueEntries(true);
    if (entries.isEmpty()) entries = queueEntries(true);
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
