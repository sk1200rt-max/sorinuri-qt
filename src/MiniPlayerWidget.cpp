#include "MiniPlayerWidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QScreen>
#include <QApplication>
#include <QHBoxLayout>
#include <QVBoxLayout>

MiniPlayerWidget::MiniPlayerWidget(QWidget* parent)
    : QWidget(parent)
{
    // 항상 위에 + 프레임 없음 + 작업표시줄 미표시
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(420, 80);

    // 화면 우측 하단 배치
    if (auto* screen = QApplication::primaryScreen()) {
        QRect sg = screen->availableGeometry();
        move(sg.right() - width() - 20, sg.bottom() - height() - 20);
    }

    setupUI();
}

void MiniPlayerWidget::setupUI() {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(10, 8, 10, 8);
    root->setSpacing(10);

    // 앨범 아트
    albumArtLabel_ = new QLabel(this);
    albumArtLabel_->setFixedSize(56, 56);
    albumArtLabel_->setAlignment(Qt::AlignCenter);
    albumArtLabel_->setStyleSheet(
        "QLabel { background:#1e1e1e; border-radius:6px; color:#555; font-size:20px; }");
    albumArtLabel_->setText("♪");
    root->addWidget(albumArtLabel_);

    // 중앙: 트랙 정보 + 시크바
    auto* center = new QVBoxLayout;
    center->setSpacing(4);
    center->setContentsMargins(0, 0, 0, 0);

    auto* infoRow = new QHBoxLayout;
    infoRow->setSpacing(6);

    titleLabel_ = new QLabel("트랙 제목", this);
    titleLabel_->setStyleSheet("color:white; font-size:13px; font-weight:bold;");
    titleLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    artistLabel_ = new QLabel("아티스트", this);
    artistLabel_->setStyleSheet("color:#888; font-size:11px;");
    artistLabel_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    codecLabel_ = new QLabel("FLAC", this);
    codecLabel_->setStyleSheet(
        "color:#00c8b4; border:1px solid #00c8b4; border-radius:2px;"
        "padding:0 4px; font-size:10px; font-family:'Consolas';");
    codecLabel_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    infoRow->addWidget(titleLabel_, 1);
    infoRow->addWidget(artistLabel_);
    infoRow->addWidget(codecLabel_);
    center->addLayout(infoRow);

    // 시크바 + 시간
    auto* seekRow = new QHBoxLayout;
    seekRow->setSpacing(6);
    seekSlider_ = new QSlider(Qt::Horizontal, this);
    seekSlider_->setRange(0, 1000);
    seekSlider_->setFixedHeight(4);
    seekSlider_->setStyleSheet(
        "QSlider::groove:horizontal{background:#2a2a2a;height:4px;border-radius:2px;}"
        "QSlider::handle:horizontal{background:#00c8b4;width:10px;height:10px;margin:-3px 0;border-radius:5px;}"
        "QSlider::sub-page:horizontal{background:#00c8b4;border-radius:2px;}");
    connect(seekSlider_, &QSlider::sliderMoved, this, [this](int v) {
        if (duration_ > 0) emit seekRequested(duration_ * v / 1000.0);
    });

    timeLabel_ = new QLabel("00:00", this);
    timeLabel_->setStyleSheet("color:#555; font-size:10px; font-family:'Consolas';");
    timeLabel_->setFixedWidth(38);
    timeLabel_->setAlignment(Qt::AlignRight);

    seekRow->addWidget(seekSlider_, 1);
    seekRow->addWidget(timeLabel_);
    center->addLayout(seekRow);

    root->addLayout(center, 1);

    // 우측: 컨트롤 버튼
    auto* ctrlCol = new QVBoxLayout;
    ctrlCol->setSpacing(4);
    ctrlCol->setAlignment(Qt::AlignVCenter);

    auto* btnRow = new QHBoxLayout;
    btnRow->setSpacing(4);

    auto makeBtn = [this](const QString& text, int size) {
        auto* btn = new QPushButton(text, this);
        btn->setFixedSize(size, size);
        btn->setStyleSheet(
            "QPushButton{background:transparent;border:none;color:#aaa;font-size:14px;}"
            "QPushButton:hover{color:white;}");
        return btn;
    };

    btnPrev_   = makeBtn("⏮", 24);
    btnPlay_   = new QPushButton("▶", this);
    btnPlay_->setFixedSize(32, 32);
    btnPlay_->setStyleSheet(
        "QPushButton{background:#00c8b4;color:#0e0e0e;border-radius:16px;border:none;font-size:14px;font-weight:bold;}"
        "QPushButton:hover{background:#00e0cc;}");
    btnNext_   = makeBtn("⏭", 24);
    btnExpand_ = makeBtn("⊞", 20);
    btnExpand_->setToolTip("메인 플레이어로 돌아가기");

    btnRow->addWidget(btnPrev_);
    btnRow->addWidget(btnPlay_);
    btnRow->addWidget(btnNext_);

    ctrlCol->addLayout(btnRow);
    ctrlCol->addWidget(btnExpand_, 0, Qt::AlignRight);

    root->addLayout(ctrlCol);

    connect(btnPlay_,   &QPushButton::clicked, this, &MiniPlayerWidget::playPauseRequested);
    connect(btnPrev_,   &QPushButton::clicked, this, &MiniPlayerWidget::prevRequested);
    connect(btnNext_,   &QPushButton::clicked, this, &MiniPlayerWidget::nextRequested);
    connect(btnExpand_, &QPushButton::clicked, this, &MiniPlayerWidget::expandRequested);
}

void MiniPlayerWidget::setMeta(const QString& title, const QString& artist,
                                const QPixmap& art, const QString& codec, int bitDepth) {
    titleLabel_->setText(title.isEmpty() ? "알 수 없는 트랙" : title);
    artistLabel_->setText(artist.isEmpty() ? "" : artist);
    codecLabel_->setText(codec.isEmpty() ? "PCM" : codec.toUpper());

    if (!art.isNull()) {
        // 원형 마스크 적용
        QPixmap scaled = art.scaled(56, 56, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        QPixmap rounded(56, 56);
        rounded.fill(Qt::transparent);
        QPainter p(&rounded);
        p.setRenderHint(QPainter::Antialiasing);
        QPainterPath path;
        path.addRoundedRect(0, 0, 56, 56, 6, 6);
        p.setClipPath(path);
        p.drawPixmap(0, 0, scaled);
        albumArtLabel_->setPixmap(rounded);
        albumArtLabel_->setText("");
    } else {
        albumArtLabel_->setPixmap(QPixmap());
        albumArtLabel_->setText("♪");
    }
}

void MiniPlayerWidget::updatePosition(double pos, double duration) {
    duration_ = duration;
    if (!seekSlider_->isSliderDown()) {
        seekSlider_->setValue(duration > 0 ? qRound(pos / duration * 1000) : 0);
    }
    int s = qRound(pos);
    timeLabel_->setText(QString("%1:%2").arg(s/60, 2, 10, QChar('0')).arg(s%60, 2, 10, QChar('0')));
}

void MiniPlayerWidget::setPlaying(bool playing) {
    isPlaying_ = playing;
    btnPlay_->setText(playing ? "⏸" : "▶");
}

void MiniPlayerWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    // 반투명 다크 배경
    QPainterPath path;
    path.addRoundedRect(rect(), 10, 10);
    p.fillPath(path, QColor(12, 12, 12, 230));
    // 테두리
    p.setPen(QPen(QColor(40, 40, 40), 1));
    p.drawPath(path);
}

void MiniPlayerWidget::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        dragging_ = true;
        dragOffset_ = e->globalPosition().toPoint() - pos();
    }
}

void MiniPlayerWidget::mouseMoveEvent(QMouseEvent* e) {
    if (dragging_) move(e->globalPosition().toPoint() - dragOffset_);
}

void MiniPlayerWidget::mouseReleaseEvent(QMouseEvent*) {
    dragging_ = false;
}
