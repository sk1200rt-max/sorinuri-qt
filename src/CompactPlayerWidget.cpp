#include "CompactPlayerWidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QApplication>
#include <QScreen>
#include <QFileInfo>
#include <QResizeEvent>
#include <cmath>

// ─── 색상 상수 ────────────────────────────────────────────────────────────────
static const QColor kTeal(0, 200, 180);
static const QColor kBg(14, 14, 16);
static const QColor kBg2(20, 20, 24);
static const QColor kBorder(38, 38, 44);
static const QColor kText(220, 220, 225);
static const QColor kTextDim(100, 100, 110);
static const QColor kAccent(0, 200, 180);

// ─── 유틸리티 ────────────────────────────────────────────────────────────────
static QPixmap blurPixmap(const QPixmap& src, int radius) {
    if (src.isNull()) return src;
    QImage img = src.toImage().scaled(200, 200, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    for (int pass = 0; pass < radius; ++pass)
        for (int y = 1; y < img.height()-1; ++y)
            for (int x = 1; x < img.width()-1; ++x) {
                int r=0,g=0,b=0;
                for (int dy=-1;dy<=1;++dy) for (int dx=-1;dx<=1;++dx) {
                    QRgb c=img.pixel(x+dx,y+dy); r+=qRed(c); g+=qGreen(c); b+=qBlue(c);
                }
                img.setPixel(x,y,qRgb(r/9,g/9,b/9));
            }
    return QPixmap::fromImage(img);
}

static QColor extractDominant(const QPixmap& src) {
    if (src.isNull()) return kTeal;
    QImage img = src.toImage().scaled(40,40,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
    long long r=0,g=0,b=0,cnt=0;
    for (int y=0;y<img.height();++y) for (int x=0;x<img.width();++x) {
        QRgb c=img.pixel(x,y);
        int br=(qRed(c)+qGreen(c)+qBlue(c))/3;
        if (br>30&&br<220){r+=qRed(c);g+=qGreen(c);b+=qBlue(c);++cnt;}
    }
    if (!cnt) return kTeal;
    QColor avg(r/cnt,g/cnt,b/cnt);
    int h,s,v; avg.getHsv(&h,&s,&v);
    return QColor::fromHsv(h,qMin(255,s+80),qMax(80,v));
}

// ─── 생성자 ──────────────────────────────────────────────────────────────────
CompactPlayerWidget::CompactPlayerWidget(QWidget* parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    setMinimumSize(280, 480);
    setMaximumSize(500, 900);
    resize(340, 620);

    specBins_.fill(0.0f, 32);
    specPeak_.fill(0.0f, 32);

    // 스펙트럼 피크 감쇠 타이머
    peakTimer_ = new QTimer(this);
    peakTimer_->setInterval(50);
    connect(peakTimer_, &QTimer::timeout, this, [this]() {
        bool ch = false;
        for (int i = 0; i < specPeak_.size(); ++i)
            if (specPeak_[i] > 0.002f) { specPeak_[i] *= 0.90f; ch = true; }
        if (ch) update();
    });
    peakTimer_->start();

    // 앨범아트 회전 타이머
    rotationTimer_ = new QTimer(this);
    rotationTimer_->setInterval(33);
    connect(rotationTimer_, &QTimer::timeout, this, [this]() {
        if (isPlaying_) {
            rotationAngle_ += 0.3;
            if (rotationAngle_ >= 360.0) rotationAngle_ -= 360.0;
            update();
        }
    });
    rotationTimer_->start();

    setupUI();
    setupConnections();

    // 화면 우측 하단에 배치
    if (QScreen* scr = QApplication::primaryScreen()) {
        QRect sg = scr->availableGeometry();
        move(sg.right() - width() - 24, sg.bottom() - height() - 48);
    }
}

// ─── UI 구성 ─────────────────────────────────────────────────────────────────
void CompactPlayerWidget::setupUI() {
    // 전체 레이아웃은 paintEvent에서 직접 그리고,
    // 인터랙티브 위젯만 절대 좌표로 배치

    // ── 상단 우측 버튼 3개 ─────────────────────────────────────────
    auto makeTopBtn = [this](const QString& text, const QString& tip) {
        auto* btn = new QPushButton(text, this);
        btn->setFixedSize(24, 24);
        btn->setToolTip(tip);
        btn->setStyleSheet(
            "QPushButton{background:transparent;border:none;color:#666;font-size:12px;}"
            "QPushButton:hover{color:#ccc;}");
        return btn;
    };
    btnPin_    = makeTopBtn("📌", "항상 위에 고정");
    btnExpand_ = makeTopBtn("⊞", "전체 플레이어로 돌아가기");
    btnClose_  = makeTopBtn("✕", "닫기");
    btnPin_->setCheckable(true);

    // ── 시크바 ─────────────────────────────────────────────────────
    seekSlider_ = new QSlider(Qt::Horizontal, this);
    seekSlider_->setRange(0, 1000);
    seekSlider_->setStyleSheet(
        "QSlider::groove:horizontal{background:#1e1e24;height:3px;border-radius:2px;}"
        "QSlider::handle:horizontal{background:#00c8b4;width:10px;height:10px;margin:-3.5px 0;border-radius:5px;}"
        "QSlider::sub-page:horizontal{background:#00c8b4;border-radius:2px;}");

    timeCurrent_ = new QLabel("00:00", this);
    timeCurrent_->setStyleSheet("font-size:10px;color:#666;font-family:'Consolas';");
    timeCurrent_->setFixedWidth(36);

    timeDuration_ = new QLabel("00:00", this);
    timeDuration_->setStyleSheet("font-size:10px;color:#666;font-family:'Consolas';");
    timeDuration_->setFixedWidth(36);
    timeDuration_->setAlignment(Qt::AlignRight);

    // ── 컨트롤 버튼 ────────────────────────────────────────────────
    auto makeCtrlBtn = [this](const QString& text, bool big) {
        auto* btn = new QPushButton(text, this);
        int sz = big ? 44 : 28;
        btn->setFixedSize(sz, sz);
        if (big) {
            btn->setStyleSheet(QString(
                "QPushButton{background:#00c8b4;color:#0e0e0e;border-radius:%1px;"
                "border:none;font-size:16px;font-weight:bold;}"
                "QPushButton:hover{background:#00e0cc;}").arg(sz/2));
        } else {
            btn->setStyleSheet(
                "QPushButton{background:transparent;border:none;color:#888;font-size:14px;}"
                "QPushButton:hover{color:white;}");
        }
        return btn;
    };
    btnShuffle_ = makeCtrlBtn("⇌", false);
    btnPrev_    = makeCtrlBtn("⏮", false);
    btnPlay_    = makeCtrlBtn("▶", true);
    btnNext_    = makeCtrlBtn("⏭", false);
    btnRepeat_  = makeCtrlBtn("↺", false);
    btnShuffle_->setCheckable(true);
    btnRepeat_->setCheckable(true);

    // ── 볼륨 ────────────────────────────────────────────────────────
    btnVolume_ = new QPushButton("🔊", this);
    btnVolume_->setFixedSize(22, 22);
    btnVolume_->setStyleSheet(
        "QPushButton{background:transparent;border:none;font-size:12px;color:#666;}"
        "QPushButton:hover{color:#ccc;}");

    volSlider_ = new QSlider(Qt::Horizontal, this);
    volSlider_->setRange(0, 100);
    volSlider_->setValue(100);
    volSlider_->setFixedWidth(80);
    volSlider_->setStyleSheet(
        "QSlider::groove:horizontal{background:#1e1e24;height:3px;border-radius:2px;}"
        "QSlider::handle:horizontal{background:#666;width:8px;height:8px;margin:-2.5px 0;border-radius:4px;}"
        "QSlider::sub-page:horizontal{background:#555;border-radius:2px;}");

    // ── 재생목록 ────────────────────────────────────────────────────
    playlistWidget_ = new QListWidget(this);
    playlistWidget_->setStyleSheet(
        "QListWidget{background:#0e0e10;border:none;color:#aaa;font-size:11px;outline:none;}"
        "QListWidget::item{padding:6px 12px;border-bottom:1px solid #1a1a1e;}"
        "QListWidget::item:hover{background:#16161c;color:#ccc;}"
        "QListWidget::item:selected{background:#1a2a2a;color:#00c8b4;"
        "border-left:3px solid #00c8b4;}");
    playlistWidget_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // 트랙 정보 레이블
    titleLabel_ = new QLabel("트랙 제목", this);
    titleLabel_->setStyleSheet("font-size:14px;font-weight:bold;color:#e8e8ec;"
                               "background:transparent;");
    titleLabel_->setAlignment(Qt::AlignCenter);
    titleLabel_->setWordWrap(true);

    artistLabel_ = new QLabel("아티스트", this);
    artistLabel_->setStyleSheet("font-size:11px;color:#888;background:transparent;");
    artistLabel_->setAlignment(Qt::AlignCenter);

    codecLabel_ = new QLabel("", this);
    codecLabel_->setStyleSheet("font-size:10px;color:#555;font-family:'Consolas';"
                               "background:transparent;");
    codecLabel_->setAlignment(Qt::AlignCenter);
}

void CompactPlayerWidget::setupConnections() {
    connect(btnPlay_,   &QPushButton::clicked, this, &CompactPlayerWidget::playPauseRequested);
    connect(btnPrev_,   &QPushButton::clicked, this, &CompactPlayerWidget::prevRequested);
    connect(btnNext_,   &QPushButton::clicked, this, &CompactPlayerWidget::nextRequested);
    connect(btnExpand_, &QPushButton::clicked, this, &CompactPlayerWidget::expandRequested);
    connect(btnClose_,  &QPushButton::clicked, this, &CompactPlayerWidget::expandRequested);
    connect(btnPin_,    &QPushButton::toggled, this, [this](bool on) {
        isPinned_ = on;
        Qt::WindowFlags flags = windowFlags();
        if (on) flags |= Qt::WindowStaysOnTopHint;
        else    flags &= ~Qt::WindowStaysOnTopHint;
        setWindowFlags(flags);
        show();
        emit alwaysOnTopToggled(on);
    });
    connect(btnShuffle_, &QPushButton::toggled, this, &CompactPlayerWidget::shuffleToggled);
    connect(btnRepeat_,  &QPushButton::toggled, this, &CompactPlayerWidget::repeatToggled);
    connect(seekSlider_, &QSlider::sliderMoved, this, [this](int v) {
        if (duration_ > 0) emit seekRequested(duration_ * v / 1000.0);
    });
    connect(volSlider_, &QSlider::valueChanged, this, &CompactPlayerWidget::volumeChanged);
    connect(playlistWidget_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        emit trackSelected(playlistWidget_->row(item));
    });
}

// ─── 레이아웃 업데이트 (resizeEvent) ────────────────────────────────────────
void CompactPlayerWidget::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    const int W = width();
    const int H = height();

    // 분할선 위치 클램핑 (플레이어 최소 300px, 목록 최소 80px)
    splitterY_ = qBound(300, splitterY_, H - 80);

    // ── 상단 우측 버튼 배치 ─────────────────────────────────────────
    btnClose_->move(W - 28, 8);
    btnExpand_->move(W - 56, 8);
    btnPin_->move(W - 84, 8);

    // ── 시크바 영역 (splitterY_ - 70 ~ splitterY_ - 44) ────────────
    const int seekY = splitterY_ - 68;
    timeCurrent_->move(12, seekY + 2);
    seekSlider_->setGeometry(52, seekY, W - 104, 20);
    timeDuration_->move(W - 50, seekY + 2);

    // ── 컨트롤 버튼 영역 (splitterY_ - 44 ~ splitterY_ - 4) ────────
    const int ctrlY = splitterY_ - 46;
    const int ctrlCenterX = W / 2;
    btnPlay_->move(ctrlCenterX - 22, ctrlY);
    btnPrev_->move(ctrlCenterX - 22 - 36, ctrlY + 8);
    btnNext_->move(ctrlCenterX + 22 + 8,  ctrlY + 8);
    btnShuffle_->move(ctrlCenterX - 22 - 72, ctrlY + 8);
    btnRepeat_->move(ctrlCenterX + 22 + 44,  ctrlY + 8);

    // ── 볼륨 (splitterY_ - 20 ~ splitterY_ - 4) ────────────────────
    const int volY = splitterY_ - 22;
    btnVolume_->move(W - 12 - 80 - 28, volY);
    volSlider_->move(W - 12 - 80, volY);

    // ── 트랙 정보 레이블 ────────────────────────────────────────────
    // 앨범아트 원 반지름: 플레이어 섹션 높이에 따라 동적 계산
    const int artAreaH = splitterY_ - 160;  // 컨트롤/시크바 영역 제외
    const int artR = qBound(60, artAreaH / 2 - 10, 100);
    const int artCenterY = 44 + artR + 8;   // 44 = 상단 버튼 영역

    const int infoTop = artCenterY + artR + 10;
    titleLabel_->setGeometry(12, infoTop, W - 24, 36);
    artistLabel_->setGeometry(12, infoTop + 36, W - 24, 18);
    codecLabel_->setGeometry(12, infoTop + 54, W - 24, 16);

    // ── 재생목록 ────────────────────────────────────────────────────
    playlistWidget_->setGeometry(0, splitterY_ + 8, W, H - splitterY_ - 8);
}

// ─── 렌더링 ──────────────────────────────────────────────────────────────────
void CompactPlayerWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    const int W = width();
    const int H = height();

    // ── 외곽 라운드 배경 ────────────────────────────────────────────
    QPainterPath outerPath;
    outerPath.addRoundedRect(rect(), 10, 10);
    p.setClipPath(outerPath);

    // 블러 배경 (앨범아트 기반)
    if (!blurBg_.isNull()) {
        QPixmap scaled = blurBg_.scaled(W, splitterY_, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        p.drawPixmap(0, 0, scaled);
        // 어두운 오버레이
        p.fillRect(0, 0, W, splitterY_, QColor(10, 10, 14, 190));
    } else {
        p.fillRect(0, 0, W, splitterY_, kBg);
    }

    // 재생목록 배경
    p.fillRect(0, splitterY_, W, H - splitterY_, QColor(10, 10, 12));

    // ── 분할선 ──────────────────────────────────────────────────────
    p.setPen(QPen(kBorder, 1));
    p.drawLine(0, splitterY_, W, splitterY_);

    // ── 앨범아트 원형 ────────────────────────────────────────────────
    const int artAreaH = splitterY_ - 160;
    const int artR = qBound(60, artAreaH / 2 - 10, 100);
    const int artCX = W / 2;
    const int artCY = 44 + artR + 8;

    // 원형 그림자
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 60));
    p.drawEllipse(QPoint(artCX + 2, artCY + 4), artR + 4, artR + 4);

    // 앨범아트 회전 클리핑
    p.save();
    QPainterPath artClip;
    artClip.addEllipse(QPoint(artCX, artCY), artR, artR);
    p.setClipPath(artClip);

    if (!albumArtPixmap_.isNull()) {
        p.translate(artCX, artCY);
        p.rotate(rotationAngle_);
        QPixmap scaled = albumArtPixmap_.scaled(
            artR * 2, artR * 2, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        p.drawPixmap(-artR, -artR, scaled);
    } else {
        // 기본 배경
        p.fillPath(artClip, QColor(30, 30, 36));
        p.setPen(QColor(60, 60, 70));
        QFont f = p.font();
        f.setPixelSize(artR / 2);
        p.setFont(f);
        p.drawText(QRect(artCX - artR, artCY - artR, artR * 2, artR * 2),
                   Qt::AlignCenter, "♪");
    }
    p.restore();

    // 원형 테두리 (도미넌트 컬러)
    p.setPen(QPen(dominantColor_.lighter(120), 2));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QPoint(artCX, artCY), artR, artR);

    // 재생 중 표시 (원 테두리 외곽 펄스)
    if (isPlaying_) {
        QColor pulseColor = dominantColor_;
        pulseColor.setAlpha(40);
        p.setPen(QPen(pulseColor, 4));
        p.drawEllipse(QPoint(artCX, artCY), artR + 4, artR + 4);
    }

    // ── 스펙트럼 (앨범아트 아래) ─────────────────────────────────────
    const int specTop = artCY + artR + 4;
    const int specH   = 28;
    if (specTop + specH < splitterY_ - 100) {
        drawSpectrum(p, QRect(20, specTop, W - 40, specH));
    }

    // ── 상단 타이틀 바 영역 ──────────────────────────────────────────
    // "소리누리" 로고 텍스트
    p.setPen(kTeal);
    QFont logoFont = p.font();
    logoFont.setPixelSize(11);
    logoFont.setBold(true);
    p.setFont(logoFont);
    p.drawText(QRect(12, 10, 100, 20), Qt::AlignVCenter, "소리누리");

    // ── 시크바 배경 강조선 ───────────────────────────────────────────
    p.setPen(QPen(kBorder, 1));
    p.drawLine(0, splitterY_ - 76, W, splitterY_ - 76);

    // ── 재생목록 헤더 ────────────────────────────────────────────────
    p.fillRect(0, splitterY_, W, 28, QColor(14, 14, 18));
    p.setPen(kTextDim);
    QFont hdrFont = p.font();
    hdrFont.setPixelSize(10);
    hdrFont.setBold(true);
    p.setFont(hdrFont);
    p.drawText(QRect(12, splitterY_ + 6, 80, 16), Qt::AlignVCenter, "재생목록");

    // 트랙 수
    if (playlistWidget_->count() > 0) {
        p.setPen(QColor(60, 60, 70));
        p.drawText(QRect(W - 60, splitterY_ + 6, 50, 16),
                   Qt::AlignVCenter | Qt::AlignRight,
                   QString("%1곡").arg(playlistWidget_->count()));
    }

    // ── 분할선 드래그 핸들 ──────────────────────────────────────────
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(60, 60, 70, 120));
    p.drawRoundedRect(W/2 - 20, splitterY_ - 3, 40, 6, 3, 3);
}

void CompactPlayerWidget::drawSpectrum(QPainter& p, const QRect& rect) {
    if (specBins_.isEmpty()) return;
    const int n = qMin(specBins_.size(), 24);
    const float barW = static_cast<float>(rect.width()) / n;

    for (int i = 0; i < n; ++i) {
        float v = specBins_[i];
        float pk = specPeak_[i];
        int barH = qRound(v * rect.height());
        int pkH  = qRound(pk * rect.height());

        // 바
        QColor barColor = dominantColor_;
        barColor.setAlpha(160);
        p.fillRect(QRectF(rect.left() + i * barW + 1, rect.bottom() - barH,
                          barW - 2, barH), barColor);

        // 피크
        if (pkH > 2) {
            p.fillRect(QRectF(rect.left() + i * barW + 1, rect.bottom() - pkH - 1,
                              barW - 2, 2), dominantColor_);
        }
    }
}

// ─── 마우스 이벤트 ───────────────────────────────────────────────────────────
int CompactPlayerWidget::getResizeEdge(const QPoint& pos) const {
    const int m = kResizeMargin;
    const int W = width(), H = height();
    bool L = pos.x() < m, R = pos.x() > W-m;
    bool T = pos.y() < m, B = pos.y() > H-m;
    if (L && T) return 5; if (R && T) return 6;
    if (L && B) return 7; if (R && B) return 8;
    if (L) return 1; if (R) return 2;
    if (T) return 3; if (B) return 4;
    return 0;
}

void CompactPlayerWidget::mousePressEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) return;

    // 분할선 드래그 감지
    if (std::abs(e->pos().y() - splitterY_) < 8) {
        splitterDragging_ = true;
        return;
    }

    // 리사이즈 감지
    int edge = getResizeEdge(e->pos());
    if (edge != 0) {
        resizing_ = true;
        resizeEdge_ = edge;
        resizeStart_ = e->globalPosition().toPoint();
        resizeStartSize_ = size();
        return;
    }

    // 드래그 이동 (상단 플레이어 섹션에서만)
    if (e->pos().y() < splitterY_) {
        dragging_ = true;
        dragOffset_ = e->globalPosition().toPoint() - pos();
    }
}

void CompactPlayerWidget::mouseMoveEvent(QMouseEvent* e) {
    // 분할선 드래그
    if (splitterDragging_) {
        splitterY_ = qBound(300, e->pos().y(), height() - 80);
        playlistWidget_->setGeometry(0, splitterY_ + 8, width(), height() - splitterY_ - 8);
        update();
        return;
    }

    // 리사이즈
    if (resizing_) {
        QPoint d = e->globalPosition().toPoint() - resizeStart_;
        QSize ns = resizeStartSize_;
        QPoint np = pos();
        switch (resizeEdge_) {
        case 1: ns.setWidth(resizeStartSize_.width()-d.x()); np.setX(pos().x()+d.x()); break;
        case 2: ns.setWidth(resizeStartSize_.width()+d.x()); break;
        case 3: ns.setHeight(resizeStartSize_.height()-d.y()); np.setY(pos().y()+d.y()); break;
        case 4: ns.setHeight(resizeStartSize_.height()+d.y()); break;
        case 5: ns=QSize(resizeStartSize_.width()-d.x(),resizeStartSize_.height()-d.y());
                np=QPoint(pos().x()+d.x(),pos().y()+d.y()); break;
        case 6: ns=QSize(resizeStartSize_.width()+d.x(),resizeStartSize_.height()-d.y());
                np.setY(pos().y()+d.y()); break;
        case 7: ns=QSize(resizeStartSize_.width()-d.x(),resizeStartSize_.height()+d.y());
                np.setX(pos().x()+d.x()); break;
        case 8: ns=QSize(resizeStartSize_.width()+d.x(),resizeStartSize_.height()+d.y()); break;
        }
        if (ns.width() >= minimumWidth() && ns.height() >= minimumHeight())
            setGeometry(QRect(np, ns));
        return;
    }

    // 드래그 이동
    if (dragging_) {
        move(e->globalPosition().toPoint() - dragOffset_);
        return;
    }

    // 커서 변경
    if (std::abs(e->pos().y() - splitterY_) < 8) {
        setCursor(Qt::SizeVerCursor);
    } else {
        int edge = getResizeEdge(e->pos());
        static const Qt::CursorShape cs[] = {
            Qt::ArrowCursor, Qt::SizeHorCursor, Qt::SizeHorCursor,
            Qt::SizeVerCursor, Qt::SizeVerCursor,
            Qt::SizeFDiagCursor, Qt::SizeBDiagCursor,
            Qt::SizeBDiagCursor, Qt::SizeFDiagCursor
        };
        setCursor(cs[edge]);
    }
}

void CompactPlayerWidget::mouseReleaseEvent(QMouseEvent*) {
    dragging_ = false;
    resizing_ = false;
    splitterDragging_ = false;
}

// ─── 공개 메서드 ─────────────────────────────────────────────────────────────
void CompactPlayerWidget::setMeta(const QString& title, const QString& artist,
                                   const QString& album, const QPixmap& art,
                                   const QString& codec, int bitDepth, int sampleRate,
                                   int channels, bool bitPerfect) {
    Q_UNUSED(album)
    titleLabel_->setText(title.isEmpty() ? "알 수 없는 트랙" : title);
    artistLabel_->setText(artist.isEmpty() ? "" : artist);

    QString codecInfo;
    if (!codec.isEmpty()) codecInfo += codec.toUpper();
    if (sampleRate > 0) {
        codecInfo += QString("  ·  %1kHz").arg(sampleRate / 1000.0, 0, 'f', 1);
    }
    if (bitDepth > 0) codecInfo += QString("  ·  %1bit").arg(bitDepth);
    if (channels == 1) codecInfo += "  ·  Mono";
    else if (channels == 2) codecInfo += "  ·  Stereo";
    if (bitPerfect) codecInfo += "  ·  BIT-PERFECT";
    codecLabel_->setText(codecInfo);

    updateAlbumArt(art);
    update();
}

void CompactPlayerWidget::updateAlbumArt(const QPixmap& art) {
    albumArtPixmap_ = art;
    if (!art.isNull()) {
        blurBg_ = blurPixmap(art, 8);
        dominantColor_ = extractDominant(art);
    } else {
        blurBg_ = QPixmap();
        dominantColor_ = kTeal;
    }
    update();
}

void CompactPlayerWidget::updatePosition(double pos, double duration) {
    duration_ = duration;
    if (!seekSlider_->isSliderDown()) {
        seekSlider_->setValue(duration > 0 ? qRound(pos / duration * 1000) : 0);
    }
    timeCurrent_->setText(formatTime(pos));
    timeDuration_->setText(formatTime(duration));
}

void CompactPlayerWidget::setPlaying(bool playing) {
    isPlaying_ = playing;
    btnPlay_->setText(playing ? "⏸" : "▶");
}

void CompactPlayerWidget::setPlaylist(const QStringList& paths, int currentIndex) {
    playlistWidget_->clear();
    for (int i = 0; i < paths.size(); ++i) {
        QFileInfo fi(paths[i]);
        auto* item = new QListWidgetItem(
            QString("%1.  %2").arg(i + 1).arg(fi.completeBaseName()),
            playlistWidget_);
        item->setData(Qt::UserRole, i);
    }
    currentTrackIdx_ = currentIndex;
    if (currentIndex >= 0 && currentIndex < playlistWidget_->count()) {
        playlistWidget_->setCurrentRow(currentIndex);
        playlistWidget_->scrollToItem(playlistWidget_->item(currentIndex));
    }
}

void CompactPlayerWidget::updateSpectrum(const QVector<float>& bins) {
    int n = qMin(bins.size(), specBins_.size());
    for (int i = 0; i < n; ++i) {
        specBins_[i] = bins[i];
        if (bins[i] > specPeak_[i]) specPeak_[i] = bins[i];
    }
    update();
}

QString CompactPlayerWidget::formatTime(double secs) const {
    int s = qRound(secs);
    return QString("%1:%2").arg(s/60, 2, 10, QChar('0')).arg(s%60, 2, 10, QChar('0'));
}
