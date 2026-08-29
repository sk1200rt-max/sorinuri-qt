#include "CompactPlayerWidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QApplication>
#include <QScreen>
#include <QFileInfo>
#include <QResizeEvent>
#include <QEnterEvent>
#include <cmath>

// ─── 색상 팔레트 ─────────────────────────────────────────────────────────────
static const QColor kBgDeep(10, 10, 12);
static const QColor kBgCard(18, 18, 22, 210);
static const QColor kBorder(40, 40, 48);
static const QColor kText(240, 240, 245);
static const QColor kTextSub(140, 140, 155);
static const QColor kTextDim(70, 70, 82);
static const QColor kAccent(0, 200, 180);

// ─── 유틸리티 ────────────────────────────────────────────────────────────────
static QPixmap makeBlur(const QPixmap& src, int w, int h) {
    if (src.isNull()) return {};
    QImage img = src.toImage().scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    // 빠른 박스 블러 3패스
    for (int pass = 0; pass < 3; ++pass) {
        for (int y = 1; y < img.height()-1; ++y)
            for (int x = 1; x < img.width()-1; ++x) {
                int r=0,g=0,b=0;
                for (int dy=-1;dy<=1;++dy) for (int dx=-1;dx<=1;++dx) {
                    QRgb c=img.pixel(x+dx,y+dy); r+=qRed(c);g+=qGreen(c);b+=qBlue(c);
                }
                img.setPixel(x,y,qRgb(r/9,g/9,b/9));
            }
    }
    return QPixmap::fromImage(img);
}

static QColor extractDominant(const QPixmap& src) {
    if (src.isNull()) return kAccent;
    QImage img = src.toImage().scaled(32,32,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
    long long r=0,g=0,b=0,n=0;
    for (int y=0;y<img.height();++y) for (int x=0;x<img.width();++x) {
        QRgb c=img.pixel(x,y);
        int br=(qRed(c)+qGreen(c)+qBlue(c))/3;
        if(br>25&&br<230){r+=qRed(c);g+=qGreen(c);b+=qBlue(c);++n;}
    }
    if(!n) return kAccent;
    QColor avg(r/n,g/n,b/n);
    int h2,s,v; avg.getHsv(&h2,&s,&v);
    return QColor::fromHsv(h2, qMin(255,s+60), qMax(100,v));
}

static QPixmap makeRoundedPixmap(const QPixmap& src, int w, int h, int radius) {
    QPixmap result(w, h);
    result.fill(Qt::transparent);
    QPainter p(&result);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    QPainterPath path;
    path.addRoundedRect(0, 0, w, h, radius, radius);
    p.setClipPath(path);
    QPixmap scaled = src.scaled(w, h, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    int ox = (scaled.width() - w) / 2;
    int oy = (scaled.height() - h) / 2;
    p.drawPixmap(-ox, -oy, scaled);
    return result;
}

// ─── 생성자 ──────────────────────────────────────────────────────────────────
CompactPlayerWidget::CompactPlayerWidget(QWidget* parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    setMinimumSize(300, 520);
    setMaximumSize(480, 860);
    resize(340, 620);

    specBins_.fill(0.0f, 28);
    specPeak_.fill(0.0f, 28);

    // 스펙트럼 피크 감쇠
    peakTimer_ = new QTimer(this);
    peakTimer_->setInterval(40);
    connect(peakTimer_, &QTimer::timeout, this, [this]() {
        bool changed = false;
        for (int i = 0; i < specPeak_.size(); ++i) {
            if (specPeak_[i] > 0.001f) {
                specPeak_[i] *= 0.88f;
                changed = true;
            }
        }
        if (changed) {
            update();
        } else {
            // 피크 감쇠가 끝났으면 25fps 타이머도 멈춘다. 새 스펙트럼 데이터가
            // 도착하면 updateSpectrum()이 다시 시작한다.
            peakTimer_->stop();
        }
    });

    setupUI();
    setupConnections();

    if (QScreen* scr = QApplication::primaryScreen()) {
        QRect sg = scr->availableGeometry();
        move(sg.right() - width() - 24, sg.bottom() - height() - 48);
    }
}

// ─── UI 구성 (절대 좌표 배치) ────────────────────────────────────────────────
void CompactPlayerWidget::setupUI() {
    // ── 상단 버튼 (항상 위 / 전체 복귀 / 닫기) ─────────────────────
    auto makeTopBtn = [this](const QString& txt, const QString& tip) {
        auto* b = new QPushButton(txt, this);
        b->setFixedSize(22, 22);
        b->setToolTip(tip);
        b->setFocusPolicy(Qt::NoFocus);  // HiDPI: 버튼 클릭 후 포커스 유지
        b->setStyleSheet(
            "QPushButton{background:rgba(255,255,255,0.06);border:1px solid rgba(255,255,255,0.08);"
            "border-radius:11px;color:rgba(200,200,210,0.7);font-size:10px;}"
            "QPushButton:hover{background:rgba(255,255,255,0.14);color:#fff;}");
        return b;
    };
    btnClose_  = makeTopBtn("✕", "전체 플레이어로 돌아가기");
    btnExpand_ = makeTopBtn("⊞", "전체 플레이어로 돌아가기");
    btnPin_    = makeTopBtn("📌", "항상 위에 고정");
    btnPin_->setCheckable(true);
    btnPin_->setStyleSheet(btnPin_->styleSheet() +
        "QPushButton:checked{background:rgba(0,200,180,0.25);border-color:rgba(0,200,180,0.4);"
        "color:#00c8b4;}");

    // ── 시크바 ─────────────────────────────────────────────────────
    seekSlider_ = new ClickSeekSlider(Qt::Horizontal, this);
    seekSlider_->setRange(0, 1000);
    seekSlider_->setCursor(Qt::PointingHandCursor);
    seekSlider_->setFocusPolicy(Qt::NoFocus);  // HiDPI: 키 이벤트가 MainWindow로 전달되도록
    seekSlider_->setStyleSheet(
        "QSlider{background:transparent;}"
        "QSlider::groove:horizontal{background:rgba(255,255,255,0.10);height:3px;border-radius:2px;}"
        "QSlider::handle:horizontal{background:#fff;width:12px;height:12px;"
        "margin:-4.5px 0;border-radius:6px;}"
        "QSlider::sub-page:horizontal{background:#00c8b4;border-radius:2px;}");

    timeCurrent_ = new QLabel("0:00", this);
    timeCurrent_->setStyleSheet("font-size:10px;color:rgba(180,180,190,0.7);"
                                "font-family:'Segoe UI';background:transparent;");
    timeCurrent_->setFixedWidth(32);

    timeDuration_ = new QLabel("0:00", this);
    timeDuration_->setStyleSheet("font-size:10px;color:rgba(180,180,190,0.7);"
                                 "font-family:'Segoe UI';background:transparent;");
    timeDuration_->setFixedWidth(32);
    timeDuration_->setAlignment(Qt::AlignRight);

    // ── 컨트롤 버튼 ────────────────────────────────────────────────
    auto makeCtrl = [this](const QString& txt, int sz, bool accent) {
        auto* b = new QPushButton(txt, this);
        b->setFixedSize(sz, sz);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);  // HiDPI: 버튼 클릭 후 포커스 유지
        if (accent) {
            b->setStyleSheet(QString(
                "QPushButton{background:#00c8b4;color:#0a0a0c;border-radius:%1px;"
                "border:none;font-size:%2px;font-weight:700;}"
                "QPushButton:hover{background:#00ddc9;}"
                "QPushButton:pressed{background:#00b5a3;}").arg(sz/2).arg(sz/3));
        } else {
            b->setStyleSheet(
                "QPushButton{background:rgba(255,255,255,0.07);border:1px solid rgba(255,255,255,0.10);"
                "border-radius:50%;color:rgba(220,220,230,0.85);font-size:13px;}"
                "QPushButton:hover{background:rgba(255,255,255,0.14);color:#fff;}"
                "QPushButton:pressed{background:rgba(255,255,255,0.05);}");
        }
        return b;
    };
    btnShuffle_ = makeCtrl("⇌", 30, false);
    btnPrev_    = makeCtrl("⏮", 36, false);
    btnPlay_    = makeCtrl("▶", 52, true);
    btnNext_    = makeCtrl("⏭", 36, false);
    btnRepeat_  = makeCtrl("↺", 30, false);
    btnShuffle_->setCheckable(true);
    btnRepeat_->setCheckable(true);
    btnShuffle_->setStyleSheet(btnShuffle_->styleSheet() +
        "QPushButton:checked{color:#00c8b4;border-color:rgba(0,200,180,0.4);}");
    btnRepeat_->setStyleSheet(btnRepeat_->styleSheet() +
        "QPushButton:checked{color:#00c8b4;border-color:rgba(0,200,180,0.4);}");

    // ── 볼륨 ────────────────────────────────────────────────────────
    btnVolume_ = new QPushButton("", this);
    btnVolume_->setFixedSize(18, 18);
    btnVolume_->setStyleSheet(
        "QPushButton{background:transparent;border:none;color:rgba(160,160,175,0.7);font-size:11px;}"
        "QPushButton:hover{color:#fff;}");

    volSlider_ = new QSlider(Qt::Horizontal, this);
    volSlider_->setFocusPolicy(Qt::NoFocus);  // HiDPI: 키 이벤트가 MainWindow로 전달되도록
    volSlider_->setRange(0, 100);
    volSlider_->setValue(100);
    volSlider_->setFixedWidth(72);
    volSlider_->setCursor(Qt::PointingHandCursor);
    volSlider_->setStyleSheet(
        "QSlider{background:transparent;}"
        "QSlider::groove:horizontal{background:rgba(255,255,255,0.10);height:2px;border-radius:1px;}"
        "QSlider::handle:horizontal{background:rgba(220,220,230,0.8);width:8px;height:8px;"
        "margin:-3px 0;border-radius:4px;}"
        "QSlider::sub-page:horizontal{background:rgba(220,220,230,0.5);border-radius:1px;}");

    // ── 트랙 정보 레이블 ────────────────────────────────────────────
    titleLabel_ = new QLabel("", this);
    titleLabel_->setStyleSheet(
        "font-size:16px;font-weight:700;color:#f0f0f5;"
        "font-family:'Segoe UI','맑은 고딕';background:transparent;");
    titleLabel_->setAlignment(Qt::AlignCenter);
    titleLabel_->setWordWrap(false);

    artistLabel_ = new QLabel("", this);
    artistLabel_->setStyleSheet(
        "font-size:12px;color:rgba(180,180,195,0.8);"
        "font-family:'Segoe UI','맑은 고딕';background:transparent;");
    artistLabel_->setAlignment(Qt::AlignCenter);

    codecLabel_ = new QLabel("", this);
    codecLabel_->setStyleSheet(
        "font-size:10px;color:rgba(100,200,185,0.7);"
        "font-family:'Consolas','Courier New';background:transparent;letter-spacing:1px;");
    codecLabel_->setAlignment(Qt::AlignCenter);

    // ── 재생목록 ────────────────────────────────────────────────────
    playlistWidget_ = new QListWidget(this);
    playlistWidget_->setStyleSheet(
        "QListWidget{background:transparent;border:none;color:rgba(180,180,195,0.75);"
        "font-size:11px;font-family:'Segoe UI','맑은 고딕';outline:none;}"
        "QListWidget::item{padding:8px 16px;border-bottom:1px solid rgba(255,255,255,0.04);}"
        "QListWidget::item:hover{background:rgba(255,255,255,0.05);color:rgba(220,220,235,0.9);}"
        "QListWidget::item:selected{background:rgba(0,200,180,0.12);color:#00c8b4;"
        "border-left:2px solid #00c8b4;}");
    playlistWidget_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    playlistWidget_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    playlistWidget_->setFrameShape(QFrame::NoFrame);
}

void CompactPlayerWidget::setupConnections() {
    connect(btnPlay_,   &QPushButton::clicked, this, &CompactPlayerWidget::playPauseRequested);
    connect(btnPrev_,   &QPushButton::clicked, this, &CompactPlayerWidget::prevRequested);
    connect(btnNext_,   &QPushButton::clicked, this, &CompactPlayerWidget::nextRequested);
    connect(btnExpand_, &QPushButton::clicked, this, &CompactPlayerWidget::expandRequested);
    connect(btnClose_,  &QPushButton::clicked, this, &CompactPlayerWidget::expandRequested);
    connect(btnPin_,    &QPushButton::toggled, this, [this](bool on) {
        isPinned_ = on;
        Qt::WindowFlags f = windowFlags();
        if (on) f |= Qt::WindowStaysOnTopHint;
        else    f &= ~Qt::WindowStaysOnTopHint;
        setWindowFlags(f);
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

// ─── 레이아웃 (resizeEvent) ──────────────────────────────────────────────────
void CompactPlayerWidget::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    const int W = width();
    const int H = height();

    // 분할선 클램핑
    splitterY_ = qBound(320, splitterY_, H - 100);

    // ── 상단 버튼 (우측 정렬, y=12) ────────────────────────────────
    btnClose_->move(W - 12 - 22, 12);
    btnExpand_->move(W - 12 - 22 - 28, 12);
    btnPin_->move(W - 12 - 22 - 56, 12);

    // ── 앨범아트 영역 계산 ──────────────────────────────────────────
    // 상단 버튼(44px) ~ 트랙정보(70px) ~ 컨트롤(90px) ~ 시크바(30px) = 234px 필요
    // 남은 공간을 앨범아트에 할당
    const int artTop  = 44;
    const int artBot  = splitterY_ - 90 - 30 - 70;  // 컨트롤+시크바+트랙정보
    const int artSize = qBound(100, qMin(artBot - artTop - 16, W - 64), 200);
    const int artX    = (W - artSize) / 2;
    const int artY    = artTop + (artBot - artTop - artSize) / 2;
    artRect_ = QRect(artX, artY, artSize, artSize);

    // ── 트랙 정보 (앨범아트 아래) ───────────────────────────────────
    const int infoTop = artY + artSize + 12;
    titleLabel_->setGeometry(12, infoTop, W - 24, 22);
    artistLabel_->setGeometry(12, infoTop + 24, W - 24, 16);
    codecLabel_->setGeometry(12, infoTop + 42, W - 24, 14);

    // ── 시크바 영역 ─────────────────────────────────────────────────
    const int seekY = splitterY_ - 88;
    timeCurrent_->move(12, seekY + 4);
    seekSlider_->setGeometry(48, seekY, W - 96, 22);
    timeDuration_->move(W - 44, seekY + 4);

    // ── 컨트롤 버튼 ─────────────────────────────────────────────────
    const int ctrlY   = splitterY_ - 62;
    const int cx      = W / 2;
    btnPlay_->move(cx - 26, ctrlY);
    btnPrev_->move(cx - 26 - 44, ctrlY + 8);
    btnNext_->move(cx + 26 + 8,  ctrlY + 8);
    btnShuffle_->move(cx - 26 - 84, ctrlY + 11);
    btnRepeat_->move(cx + 26 + 54,  ctrlY + 11);

    // ── 볼륨 (시크바 아래 우측) ─────────────────────────────────────
    const int volY = splitterY_ - 24;
    btnVolume_->move(W - 12 - 72 - 24, volY);
    volSlider_->move(W - 12 - 72, volY);

    // ── 재생목록 ────────────────────────────────────────────────────
    const int listTop = splitterY_ + 32;  // 헤더 32px
    playlistWidget_->setGeometry(0, listTop, W, H - listTop);
}

// ─── 렌더링 ──────────────────────────────────────────────────────────────────
void CompactPlayerWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    const int W = width();
    const int H = height();

    // ── 외곽 라운드 클리핑 ───────────────────────────────────────────
    QPainterPath outerClip;
    outerClip.addRoundedRect(rect(), 14, 14);
    p.setClipPath(outerClip);

    // ── 블러 배경 (플레이어 섹션) ────────────────────────────────────
    if (!blurBg_.isNull()) {
        QPixmap scaled = blurBg_.scaled(W, splitterY_, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        p.drawPixmap(0, 0, scaled);
        // 딥 다크 오버레이 (위쪽은 더 어둡게, 아래쪽은 약간 밝게)
        QLinearGradient overlay(0, 0, 0, splitterY_);
        overlay.setColorAt(0.0, QColor(8, 8, 10, 210));
        overlay.setColorAt(0.5, QColor(10, 10, 14, 185));
        overlay.setColorAt(1.0, QColor(12, 12, 16, 200));
        p.fillRect(0, 0, W, splitterY_, overlay);
    } else {
        p.fillRect(0, 0, W, splitterY_, kBgDeep);
    }

    // ── 재생목록 배경 ────────────────────────────────────────────────
    p.fillRect(0, splitterY_, W, H - splitterY_, QColor(10, 10, 13));

    // ── 앨범아트 (라운드 사각형) ─────────────────────────────────────
    if (!artRect_.isEmpty()) {
        // 그림자
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 80));
        p.drawRoundedRect(artRect_.adjusted(4, 6, 4, 6), 14, 14);

        if (!albumArtPixmap_.isNull()) {
            // 라운드 앨범아트
            QPixmap rounded = makeRoundedPixmap(albumArtPixmap_,
                artRect_.width(), artRect_.height(), 14);
            p.drawPixmap(artRect_.topLeft(), rounded);

            // 재생 중 테두리 (도미넌트 컬러)
            if (isPlaying_) {
                p.setPen(QPen(dominantColor_.lighter(130), 1.5));
                p.setBrush(Qt::NoBrush);
                p.drawRoundedRect(artRect_, 14, 14);
            }
        } else {
            // 기본 앨범아트 배경
            p.setBrush(QColor(28, 28, 34));
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(artRect_, 14, 14);
            p.setPen(QColor(60, 60, 72));
            QFont f = p.font();
            f.setPixelSize(artRect_.height() / 3);
            p.setFont(f);
            p.drawText(artRect_, Qt::AlignCenter, "♪");
        }
    }

    // ── 스펙트럼 (앨범아트 하단 오버레이) ───────────────────────────
    if (!specBins_.isEmpty() && !artRect_.isEmpty()) {
        const int specH = 32;
        QRect specRect(artRect_.left(), artRect_.bottom() - specH,
                       artRect_.width(), specH);
        // 페이드 마스크
        QLinearGradient specFade(0, specRect.top(), 0, specRect.bottom());
        specFade.setColorAt(0, QColor(0,0,0,0));
        specFade.setColorAt(1, QColor(0,0,0,120));
        p.fillRect(specRect, specFade);
        drawSpectrum(p, specRect);
    }

    // ── 분할선 ──────────────────────────────────────────────────────
    p.setPen(QPen(QColor(255, 255, 255, 12), 1));
    p.drawLine(0, splitterY_, W, splitterY_);

    // ── 재생목록 헤더 ────────────────────────────────────────────────
    // 반투명 헤더 배경
    p.fillRect(0, splitterY_, W, 32, QColor(14, 14, 17));

    p.setPen(QColor(100, 100, 115));
    QFont hf = p.font();
    hf.setPixelSize(10);
    hf.setBold(true);
    hf.setLetterSpacing(QFont::AbsoluteSpacing, 1.5);
    p.setFont(hf);
    p.drawText(QRect(16, splitterY_ + 8, 80, 16), Qt::AlignVCenter, "PLAYLIST");

    if (playlistWidget_->count() > 0) {
        p.setPen(QColor(70, 70, 82));
        QFont cf = p.font();
        cf.setPixelSize(10);
        cf.setBold(false);
        cf.setLetterSpacing(QFont::AbsoluteSpacing, 0);
        p.setFont(cf);
        p.drawText(QRect(W - 60, splitterY_ + 8, 50, 16),
                   Qt::AlignVCenter | Qt::AlignRight,
                   QString("%1 tracks").arg(playlistWidget_->count()));
    }

    // ── 분할선 드래그 핸들 ──────────────────────────────────────────
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 255, 255, 18));
    p.drawRoundedRect(W/2 - 18, splitterY_ - 4, 36, 4, 2, 2);

    // ── 상단 로고 ────────────────────────────────────────────────────
    p.setPen(QColor(0, 200, 180, 160));
    QFont lf = p.font();
    lf.setPixelSize(11);
    lf.setBold(true);
    lf.setLetterSpacing(QFont::AbsoluteSpacing, 0.5);
    p.setFont(lf);
    p.drawText(QRect(14, 12, 80, 22), Qt::AlignVCenter, "소리누리");

    // ── 외곽 테두리 ──────────────────────────────────────────────────
    p.setClipping(false);
    p.setPen(QPen(QColor(255, 255, 255, 14), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(0.5, 0.5, W-1, H-1), 14, 14);
}

void CompactPlayerWidget::drawSpectrum(QPainter& p, const QRect& rect) {
    const int n = qMin(specBins_.size(), 24);
    if (n == 0) return;
    const float barW = static_cast<float>(rect.width()) / n;

    for (int i = 0; i < n; ++i) {
        float v  = specBins_[i];
        float pk = specPeak_[i];
        int barH = qRound(v * rect.height() * 0.9f);
        int pkH  = qRound(pk * rect.height() * 0.9f);

        QColor barColor = dominantColor_;
        barColor.setAlpha(100);
        if (barH > 0) {
            p.fillRect(QRectF(rect.left() + i * barW + 0.5f,
                              rect.bottom() - barH,
                              barW - 1.0f, barH), barColor);
        }
        if (pkH > 1) {
            QColor pkColor = dominantColor_;
            pkColor.setAlpha(180);
            p.fillRect(QRectF(rect.left() + i * barW + 0.5f,
                              rect.bottom() - pkH - 1,
                              barW - 1.0f, 2), pkColor);
        }
    }
}

// ─── 마우스 이벤트 ───────────────────────────────────────────────────────────
int CompactPlayerWidget::getResizeEdge(const QPoint& pos) const {
    const int m = kResizeMargin, W = width(), H = height();
    bool L=pos.x()<m, R=pos.x()>W-m, T=pos.y()<m, B=pos.y()>H-m;
    if(L&&T)return 5; if(R&&T)return 6;
    if(L&&B)return 7; if(R&&B)return 8;
    if(L)return 1; if(R)return 2;
    if(T)return 3; if(B)return 4;
    return 0;
}

void CompactPlayerWidget::mousePressEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) return;
    if (std::abs(e->pos().y() - splitterY_) < 8) { splitterDragging_=true; return; }
    int edge = getResizeEdge(e->pos());
    if (edge) { resizing_=true; resizeEdge_=edge; resizeStart_=e->globalPosition().toPoint(); resizeStartSize_=size(); return; }
    if (e->pos().y() < splitterY_) { dragging_=true; dragOffset_=e->globalPosition().toPoint()-pos(); }
}

void CompactPlayerWidget::mouseMoveEvent(QMouseEvent* e) {
    if (splitterDragging_) {
        splitterY_ = qBound(320, e->pos().y(), height()-100);
        const int listTop = splitterY_ + 32;
        playlistWidget_->setGeometry(0, listTop, width(), height()-listTop);
        update(); return;
    }
    if (resizing_) {
        QPoint d = e->globalPosition().toPoint() - resizeStart_;
        QSize ns = resizeStartSize_; QPoint np = pos();
        switch(resizeEdge_){
        case 1:ns.setWidth(resizeStartSize_.width()-d.x());np.setX(pos().x()+d.x());break;
        case 2:ns.setWidth(resizeStartSize_.width()+d.x());break;
        case 3:ns.setHeight(resizeStartSize_.height()-d.y());np.setY(pos().y()+d.y());break;
        case 4:ns.setHeight(resizeStartSize_.height()+d.y());break;
        case 5:ns=QSize(resizeStartSize_.width()-d.x(),resizeStartSize_.height()-d.y());np=QPoint(pos().x()+d.x(),pos().y()+d.y());break;
        case 6:ns=QSize(resizeStartSize_.width()+d.x(),resizeStartSize_.height()-d.y());np.setY(pos().y()+d.y());break;
        case 7:ns=QSize(resizeStartSize_.width()-d.x(),resizeStartSize_.height()+d.y());np.setX(pos().x()+d.x());break;
        case 8:ns=QSize(resizeStartSize_.width()+d.x(),resizeStartSize_.height()+d.y());break;
        }
        if(ns.width()>=minimumWidth()&&ns.height()>=minimumHeight()) setGeometry(QRect(np,ns));
        return;
    }
    if (dragging_) { move(e->globalPosition().toPoint()-dragOffset_); return; }
    if (std::abs(e->pos().y()-splitterY_)<8) { setCursor(Qt::SizeVerCursor); return; }
    int edge=getResizeEdge(e->pos());
    static const Qt::CursorShape cs[]={Qt::ArrowCursor,Qt::SizeHorCursor,Qt::SizeHorCursor,
        Qt::SizeVerCursor,Qt::SizeVerCursor,Qt::SizeFDiagCursor,Qt::SizeBDiagCursor,
        Qt::SizeBDiagCursor,Qt::SizeFDiagCursor};
    setCursor(cs[edge]);
}

void CompactPlayerWidget::mouseReleaseEvent(QMouseEvent*) {
    dragging_=false; resizing_=false; splitterDragging_=false;
}

// ─── 공개 메서드 ─────────────────────────────────────────────────────────────
void CompactPlayerWidget::setMeta(const QString& title, const QString& artist,
                                   const QString& album, const QPixmap& art,
                                   const QString& codec, int bitDepth, int sampleRate,
                                   int channels, bool bitPerfect) {
    Q_UNUSED(album)

    // 제목 말줄임 처리
    QFontMetrics fm(titleLabel_->font());
    QString t = title.isEmpty() ? "알 수 없는 트랙" : title;
    titleLabel_->setText(fm.elidedText(t, Qt::ElideRight, width() - 32));
    artistLabel_->setText(artist.isEmpty() ? "" : artist);

    // 코덱 정보 (간결하게)
    QStringList parts;
    if (!codec.isEmpty()) parts << codec.toUpper();
    if (sampleRate > 0) parts << QString("%1kHz").arg(sampleRate/1000.0, 0, 'f', 1);
    if (bitDepth > 0)   parts << QString("%1bit").arg(bitDepth);
    if (channels == 1)  parts << "Mono";
    else if (channels == 2) parts << "Stereo";
    if (bitPerfect)     parts << "BIT-PERFECT";
    codecLabel_->setText(parts.join("  ·  "));

    // 앨범아트 업데이트
    albumArtPixmap_ = art;
    if (!art.isNull()) {
        blurBg_ = makeBlur(art, 200, 200);
        dominantColor_ = extractDominant(art);
    } else {
        blurBg_ = {};
        dominantColor_ = kAccent;
    }
    update();
}

void CompactPlayerWidget::updatePosition(double pos, double duration) {
    duration_ = duration;
    if (!seekSlider_->isSliderDown())
        seekSlider_->setValue(duration > 0 ? qRound(pos / duration * 1000) : 0);
    auto fmt = [](double s) {
        int si = qRound(s);
        return QString("%1:%2").arg(si/60).arg(si%60, 2, 10, QChar('0'));
    };
    timeCurrent_->setText(fmt(pos));
    timeDuration_->setText(fmt(duration));
}

void CompactPlayerWidget::setPlaying(bool playing) {
    isPlaying_ = playing;
    btnPlay_->setText(playing ? "⏸" : "▶");
    update();
}

void CompactPlayerWidget::setPlaylist(const QStringList& paths, int currentIndex) {
    playlistWidget_->clear();
    for (int i = 0; i < paths.size(); ++i) {
        QFileInfo fi(paths[i]);
        QString name = fi.completeBaseName();
        auto* item = new QListWidgetItem(playlistWidget_);
        item->setText(QString("%1").arg(name));
        item->setData(Qt::UserRole, i);
        if (i == currentIndex) {
            item->setForeground(QColor(0, 200, 180));
        }
    }
    if (currentIndex >= 0 && currentIndex < playlistWidget_->count()) {
        playlistWidget_->setCurrentRow(currentIndex);
        playlistWidget_->scrollToItem(playlistWidget_->item(currentIndex));
    }
}

void CompactPlayerWidget::updateSpectrum(const QVector<float>& bins) {
    int n = qMin(bins.size(), specBins_.size());
    bool hasPeak = false;
    for (int i = 0; i < n; ++i) {
        specBins_[i] = bins[i];
        if (bins[i] > specPeak_[i]) specPeak_[i] = bins[i];
        hasPeak = hasPeak || bins[i] > 0.001f;
    }
    if (hasPeak && peakTimer_ && !peakTimer_->isActive()) peakTimer_->start();
    update();
}
