#include "MusicWidget.h"
#include "LyricsWidget.h"
#include "MpvCore.h"
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QGraphicsBlurEffect>
#include <QImage>
#include <QLinearGradient>
#include <QFontMetrics>
#include <QDateTime>
#include <cmath>
#include <QFileInfo>
#include <QDir>

// ── 헬퍼: 배지 레이블 생성 ──────────────────────────────────────────
static QLabel* makeBadge(const QString& text, const QString& color, QWidget* parent) {
    auto* lbl = new QLabel(text, parent);
    lbl->setStyleSheet(QString(
        "QLabel {"
        "  color: %1;"
        "  border: 1px solid %1;"
        "  border-radius: 3px;"
        "  padding: 1px 6px;"
        "  font-size: 11px;"
        "  font-family: 'Consolas', monospace;"
        "  background: transparent;"
        "}").arg(color));
    lbl->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    return lbl;
}

// ── 헬퍼: 이미지 블러 처리 ──────────────────────────────────────────
static QPixmap blurPixmap(const QPixmap& src, int radius) {
    if (src.isNull()) return src;
    QImage img = src.toImage().scaled(200, 200, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    // 간단한 박스 블러 (Qt GraphicsEffect 없이)
    for (int pass = 0; pass < radius; ++pass) {
        for (int y = 1; y < img.height() - 1; ++y) {
            for (int x = 1; x < img.width() - 1; ++x) {
                QRgb c[9];
                c[0] = img.pixel(x-1, y-1); c[1] = img.pixel(x, y-1); c[2] = img.pixel(x+1, y-1);
                c[3] = img.pixel(x-1, y);   c[4] = img.pixel(x, y);   c[5] = img.pixel(x+1, y);
                c[6] = img.pixel(x-1, y+1); c[7] = img.pixel(x, y+1); c[8] = img.pixel(x+1, y+1);
                int r=0, g=0, b=0;
                for (int i = 0; i < 9; ++i) { r += qRed(c[i]); g += qGreen(c[i]); b += qBlue(c[i]); }
                img.setPixel(x, y, qRgb(r/9, g/9, b/9));
            }
        }
    }
    return QPixmap::fromImage(img);
}

MusicWidget::MusicWidget(MpvCore* core, QWidget* parent)
    : QWidget(parent), core_(core)
{
    setAutoFillBackground(false);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    specBins_.fill(0.0f, 64);
    specPeak_.fill(0.0f, 64);

    peakTimer_ = new QTimer(this);
    peakTimer_->setInterval(50);
    connect(peakTimer_, &QTimer::timeout, this, [this]() {
        bool changed = false;
        for (int i = 0; i < specPeak_.size(); ++i) {
            if (specPeak_[i] > 0.002f) {
                specPeak_[i] *= 0.92f;
                changed = true;
            }
        }
        if (changed) update();
    });
    peakTimer_->start();

    setupUI();
    setupConnections();
}

void MusicWidget::setupUI() {
    // ── 전체 레이아웃 ────────────────────────────────────────────────
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ── 콘텐츠 영역 (좌+우) ─────────────────────────────────────────
    auto* contentWidget = new QWidget(this);
    contentWidget->setStyleSheet("background: transparent;");
    auto* contentLayout = new QHBoxLayout(contentWidget);
    contentLayout->setContentsMargins(24, 20, 24, 12);
    contentLayout->setSpacing(24);

    // ── 좌측 패널 ───────────────────────────────────────────────────
    auto* leftPanel = new QWidget(contentWidget);
    leftPanel->setFixedWidth(300);
    leftPanel->setStyleSheet("background: transparent;");
    auto* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(8);
    leftLayout->setAlignment(Qt::AlignHCenter | Qt::AlignTop);

    // 앨범 아트
    albumArtLabel_ = new QLabel(leftPanel);
    albumArtLabel_->setFixedSize(280, 280);
    albumArtLabel_->setAlignment(Qt::AlignCenter);
    albumArtLabel_->setStyleSheet(
        "QLabel {"
        "  border: 2px solid rgba(79,195,247,0.4);"
        "  border-radius: 12px;"
        "  background: #111;"
        "}");
    // 기본 음악 아이콘
    albumArtLabel_->setText("♪");
    albumArtLabel_->setStyleSheet(
        "QLabel {"
        "  border: 2px solid rgba(79,195,247,0.3);"
        "  border-radius: 12px;"
        "  background: #111;"
        "  color: rgba(79,195,247,0.5);"
        "  font-size: 72px;"
        "}");
    leftLayout->addWidget(albumArtLabel_, 0, Qt::AlignHCenter);
    leftLayout->addSpacing(12);

    // 트랙 제목
    titleLabel_ = new QLabel("소리누리 HiFi", leftPanel);
    titleLabel_->setAlignment(Qt::AlignCenter);
    titleLabel_->setStyleSheet(
        "color: #ffffff; font-size: 18px; font-weight: 700;"
        "background: transparent;");
    titleLabel_->setWordWrap(true);
    leftLayout->addWidget(titleLabel_);

    // 아티스트
    artistLabel_ = new QLabel("파일을 열어주세요", leftPanel);
    artistLabel_->setAlignment(Qt::AlignCenter);
    artistLabel_->setStyleSheet(
        "color: #aaa; font-size: 13px; background: transparent;");
    leftLayout->addWidget(artistLabel_);

    // 앨범
    albumLabel_ = new QLabel("", leftPanel);
    albumLabel_->setAlignment(Qt::AlignCenter);
    albumLabel_->setStyleSheet(
        "color: #777; font-size: 12px; background: transparent;");
    leftLayout->addWidget(albumLabel_);

    leftLayout->addSpacing(8);

    // 배지 행
    badgeRow_ = new QWidget(leftPanel);
    badgeRow_->setStyleSheet("background: transparent;");
    auto* badgeLayout = new QHBoxLayout(badgeRow_);
    badgeLayout->setContentsMargins(0, 0, 0, 0);
    badgeLayout->setSpacing(4);
    badgeLayout->setAlignment(Qt::AlignHCenter);

    codecBadge_ = makeBadge("FLAC", "#4fc3f7", badgeRow_);
    bitBadge_   = makeBadge("16bit", "#888", badgeRow_);
    rateBadge_  = makeBadge("44.1kHz", "#888", badgeRow_);
    chBadge_    = makeBadge("Stereo", "#888", badgeRow_);
    badgeLayout->addWidget(codecBadge_);
    badgeLayout->addWidget(bitBadge_);
    badgeLayout->addWidget(rateBadge_);
    badgeLayout->addWidget(chBadge_);
    leftLayout->addWidget(badgeRow_);

    // BIT-PERFECT 배지
    bpBadge_ = new QLabel("BIT-PERFECT", leftPanel);
    bpBadge_->setAlignment(Qt::AlignCenter);
    bpBadge_->setStyleSheet(
        "QLabel {"
        "  color: #4caf50;"
        "  border: 1px solid #4caf50;"
        "  border-radius: 3px;"
        "  padding: 2px 10px;"
        "  font-size: 11px;"
        "  font-family: 'Consolas', monospace;"
        "  background: rgba(76,175,80,0.1);"
        "}");
    leftLayout->addWidget(bpBadge_, 0, Qt::AlignHCenter);
    leftLayout->addSpacing(8);

    // 스펙트럼 영역 (커스텀 페인팅 - 이 위젯의 paintEvent에서 처리)
    // 스펙트럼은 고정 높이 영역을 확보만 함
    auto* specPlaceholder = new QWidget(leftPanel);
    specPlaceholder->setFixedHeight(60);
    specPlaceholder->setObjectName("specArea");
    specPlaceholder->setStyleSheet("background: transparent;");
    leftLayout->addWidget(specPlaceholder);
    leftLayout->addSpacing(8);

    // 재생 컨트롤
    auto* ctrlWidget = new QWidget(leftPanel);
    ctrlWidget->setStyleSheet("background: transparent;");
    auto* ctrlLayout = new QHBoxLayout(ctrlWidget);
    ctrlLayout->setContentsMargins(0, 0, 0, 0);
    ctrlLayout->setSpacing(12);
    ctrlLayout->setAlignment(Qt::AlignHCenter);

    auto makeCtrlBtn = [](const QString& icon, int size, QWidget* p) {
        auto* btn = new QPushButton(icon, p);
        btn->setFixedSize(size, size);
        btn->setFlat(true);
        btn->setStyleSheet(
            "QPushButton { color: #aaa; font-size: 16px; background: transparent; border: none; }"
            "QPushButton:hover { color: #fff; }"
            "QPushButton:pressed { color: #4fc3f7; }");
        return btn;
    };

    btnShuffle_ = makeCtrlBtn("⇄", 32, ctrlWidget);
    btnPrev_    = makeCtrlBtn("⏮", 32, ctrlWidget);
    btnPlay_    = new QPushButton("▶", ctrlWidget);
    btnPlay_->setFixedSize(52, 52);
    btnPlay_->setStyleSheet(
        "QPushButton {"
        "  color: #fff; font-size: 18px;"
        "  background: rgba(79,195,247,0.15);"
        "  border: 2px solid rgba(79,195,247,0.6);"
        "  border-radius: 26px;"
        "}"
        "QPushButton:hover {"
        "  background: rgba(79,195,247,0.25);"
        "  border-color: #4fc3f7;"
        "}"
        "QPushButton:pressed { background: rgba(79,195,247,0.4); }");
    btnNext_    = makeCtrlBtn("⏭", 32, ctrlWidget);
    btnRepeat_  = makeCtrlBtn("↺", 32, ctrlWidget);

    ctrlLayout->addWidget(btnShuffle_);
    ctrlLayout->addWidget(btnPrev_);
    ctrlLayout->addWidget(btnPlay_);
    ctrlLayout->addWidget(btnNext_);
    ctrlLayout->addWidget(btnRepeat_);
    leftLayout->addWidget(ctrlWidget);
    leftLayout->addStretch();

    // ── 우측 패널 (가사) ────────────────────────────────────────────
    lyricsWidget_ = new LyricsWidget(contentWidget);
    lyricsWidget_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    contentLayout->addWidget(leftPanel);
    contentLayout->addWidget(lyricsWidget_, 1);

    // ── 시크바 ──────────────────────────────────────────────────────
    auto* seekWidget = new QWidget(this);
    seekWidget->setStyleSheet("background: transparent;");
    seekWidget->setFixedHeight(28);
    auto* seekLayout = new QHBoxLayout(seekWidget);
    seekLayout->setContentsMargins(20, 0, 20, 0);
    seekLayout->setSpacing(10);

    timeCurrent_ = new QLabel("00:00", seekWidget);
    timeCurrent_->setStyleSheet("color: #aaa; font-size: 11px; font-family: 'Consolas';");
    timeCurrent_->setFixedWidth(40);

    seekSlider_ = new QSlider(Qt::Horizontal, seekWidget);
    seekSlider_->setRange(0, 1000);
    seekSlider_->setStyleSheet(
        "QSlider::groove:horizontal {"
        "  height: 3px; background: #333; border-radius: 1px;"
        "}"
        "QSlider::sub-page:horizontal {"
        "  background: #4fc3f7; border-radius: 1px;"
        "}"
        "QSlider::handle:horizontal {"
        "  width: 12px; height: 12px; margin: -5px 0;"
        "  background: #fff; border-radius: 6px;"
        "}");

    timeDuration_ = new QLabel("00:00", seekWidget);
    timeDuration_->setStyleSheet("color: #aaa; font-size: 11px; font-family: 'Consolas';");
    timeDuration_->setFixedWidth(40);
    timeDuration_->setAlignment(Qt::AlignRight);

    seekLayout->addWidget(timeCurrent_);
    seekLayout->addWidget(seekSlider_);
    seekLayout->addWidget(timeDuration_);

    // ── 하단 컨트롤바 ───────────────────────────────────────────────
    auto* bottomBar = new QWidget(this);
    bottomBar->setStyleSheet("background: transparent;");
    bottomBar->setFixedHeight(44);
    auto* bottomLayout = new QHBoxLayout(bottomBar);
    bottomLayout->setContentsMargins(20, 0, 20, 0);
    bottomLayout->setSpacing(8);

    btnEq_ = new QPushButton("EQ", bottomBar);
    btnEq_->setFixedSize(44, 30);
    btnEq_->setStyleSheet(
        "QPushButton { color: #aaa; border: 1px solid #444; border-radius: 4px;"
        "  background: transparent; font-size: 12px; }"
        "QPushButton:hover { color: #4fc3f7; border-color: #4fc3f7; }");

    speedLabel_ = new QLabel("1.0x", bottomBar);
    speedLabel_->setFixedSize(44, 30);
    speedLabel_->setAlignment(Qt::AlignCenter);
    speedLabel_->setStyleSheet(
        "color: #aaa; border: 1px solid #444; border-radius: 4px;"
        "font-size: 12px; font-family: 'Consolas';");

    bottomLayout->addWidget(btnEq_);
    bottomLayout->addWidget(speedLabel_);
    bottomLayout->addStretch();

    btnVolume_ = new QPushButton("🔊", bottomBar);
    btnVolume_->setFixedSize(30, 30);
    btnVolume_->setFlat(true);
    btnVolume_->setStyleSheet(
        "QPushButton { color: #aaa; background: transparent; border: none; font-size: 14px; }"
        "QPushButton:hover { color: #fff; }");

    volSlider_ = new QSlider(Qt::Horizontal, bottomBar);
    volSlider_->setRange(0, 100);
    volSlider_->setValue(80);
    volSlider_->setFixedWidth(120);
    volSlider_->setStyleSheet(
        "QSlider::groove:horizontal { height: 3px; background: #333; border-radius: 1px; }"
        "QSlider::sub-page:horizontal { background: #4fc3f7; border-radius: 1px; }"
        "QSlider::handle:horizontal { width: 12px; height: 12px; margin: -5px 0;"
        "  background: #fff; border-radius: 6px; }");

    btnSettings_ = new QPushButton("⚙", bottomBar);
    btnSettings_->setFixedSize(30, 30);
    btnSettings_->setFlat(true);
    btnSettings_->setStyleSheet(
        "QPushButton { color: #aaa; background: transparent; border: none; font-size: 16px; }"
        "QPushButton:hover { color: #fff; }");

    bottomLayout->addWidget(btnVolume_);
    bottomLayout->addWidget(volSlider_);
    bottomLayout->addSpacing(8);
    bottomLayout->addWidget(btnSettings_);

    // ── 상태바 ──────────────────────────────────────────────────────
    statusBar_ = new QLabel("소리누리 HiFi · 파일을 열어주세요", this);
    statusBar_->setFixedHeight(22);
    statusBar_->setAlignment(Qt::AlignCenter);
    statusBar_->setStyleSheet(
        "color: #555; font-size: 11px; font-family: 'Consolas', monospace;"
        "background: rgba(0,0,0,0.4); padding: 0 12px;");

    // ── 전체 조립 ────────────────────────────────────────────────────
    mainLayout->addWidget(contentWidget, 1);
    mainLayout->addWidget(seekWidget);
    mainLayout->addWidget(bottomBar);
    mainLayout->addWidget(statusBar_);
}

void MusicWidget::setupConnections() {
    connect(btnPlay_, &QPushButton::clicked, this, &MusicWidget::playPauseRequested);
    connect(btnPrev_, &QPushButton::clicked, this, &MusicWidget::prevRequested);
    connect(btnNext_, &QPushButton::clicked, this, &MusicWidget::nextRequested);
    connect(btnShuffle_, &QPushButton::clicked, this, [this]() {
        isShuffle_ = !isShuffle_;
        btnShuffle_->setStyleSheet(isShuffle_
            ? "QPushButton { color: #4fc3f7; font-size: 16px; background: transparent; border: none; }"
            : "QPushButton { color: #aaa; font-size: 16px; background: transparent; border: none; }"
              "QPushButton:hover { color: #fff; }");
        emit shuffleToggled(isShuffle_);
    });
    connect(btnRepeat_, &QPushButton::clicked, this, [this]() {
        isRepeat_ = !isRepeat_;
        btnRepeat_->setStyleSheet(isRepeat_
            ? "QPushButton { color: #4fc3f7; font-size: 16px; background: transparent; border: none; }"
            : "QPushButton { color: #aaa; font-size: 16px; background: transparent; border: none; }"
              "QPushButton:hover { color: #fff; }");
        emit repeatToggled(isRepeat_);
    });
    connect(seekSlider_, &QSlider::sliderMoved, this, [this](int val) {
        if (duration_ > 0)
            emit seekRequested(duration_ * val / 1000.0);
    });
    connect(volSlider_, &QSlider::valueChanged, this, &MusicWidget::volumeChanged);
    connect(btnEq_, &QPushButton::clicked, this, &MusicWidget::eqRequested);
    connect(btnSettings_, &QPushButton::clicked, this, &MusicWidget::settingsRequested);
}

void MusicWidget::loadMeta(const MusicMeta& meta) {
    currentMeta_ = meta;

    // 제목/아티스트/앨범
    titleLabel_->setText(meta.title.isEmpty() ? "알 수 없는 트랙" : meta.title);
    artistLabel_->setText(meta.artist.isEmpty() ? "알 수 없는 아티스트" : meta.artist);
    QString albumText = meta.album;
    if (!meta.year.isEmpty()) albumText += " · " + meta.year;
    albumLabel_->setText(albumText);

    // 배지 업데이트
    codecBadge_->setText(meta.codec.isEmpty() ? "PCM" : meta.codec.toUpper());
    bitBadge_->setText(meta.bitDepth > 0 ? QString("%1bit").arg(meta.bitDepth) : "16bit");
    if (meta.sampleRate >= 1000)
        rateBadge_->setText(QString("%1kHz").arg(meta.sampleRate / 1000.0, 0, 'f',
                            meta.sampleRate % 1000 == 0 ? 0 : 1));
    else
        rateBadge_->setText("44.1kHz");

    QString chText = "Stereo";
    if (meta.channels == 1) chText = "Mono";
    else if (meta.channels == 6) chText = "5.1";
    else if (meta.channels == 8) chText = "7.1";
    chBadge_->setText(chText);

    // 앨범 아트
    if (!meta.albumArt.isNull()) {
        updateAlbumArt(meta.albumArt);
        updateBlurBackground(meta.albumArt);
    } else {
        albumArtLabel_->setPixmap(QPixmap());
        albumArtLabel_->setText("♪");
        blurBg_ = QPixmap();
    }

    // 상태바
    QString status = QString("%1 · DECODE · %2 · %3kHz · %4bit · BIT-PERFECT")
        .arg(meta.codec.isEmpty() ? "PCM" : meta.codec.toUpper())
        .arg(meta.channels == 2 ? "2.0" : meta.channels == 6 ? "5.1" : "2.0")
        .arg(meta.sampleRate / 1000.0, 0, 'f', meta.sampleRate % 1000 == 0 ? 0 : 1)
        .arg(meta.bitDepth > 0 ? meta.bitDepth : 16);
    if (meta.hasReplayGain)
        status += QString(" · ReplayGain: %1dB").arg(meta.replayGain, 0, 'f', 1);
    statusBar_->setText(status);

    // 가사 검색
    if (lyricsWidget_)
        lyricsWidget_->loadForTrack(meta.title, meta.artist,
                                    QString());

    update();
}

void MusicWidget::updateAlbumArt(const QPixmap& art) {
    if (art.isNull()) return;
    // 원형 마스크 적용
    QPixmap scaled = art.scaled(280, 280, Qt::KeepAspectRatioByExpanding,
                                 Qt::SmoothTransformation);
    QPixmap rounded(280, 280);
    rounded.fill(Qt::transparent);
    QPainter p(&rounded);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addRoundedRect(0, 0, 280, 280, 12, 12);
    p.setClipPath(path);
    p.drawPixmap(0, 0, scaled);
    p.end();
    albumArtLabel_->setPixmap(rounded);
    albumArtLabel_->setText("");
    albumArtLabel_->setStyleSheet(
        "QLabel {"
        "  border: 2px solid rgba(79,195,247,0.4);"
        "  border-radius: 12px;"
        "  background: transparent;"
        "}");
}

void MusicWidget::updateBlurBackground(const QPixmap& art) {
    blurBg_ = blurPixmap(art, 8);
    update();
}

void MusicWidget::updatePosition(double pos, double duration) {
    duration_ = duration;
    timeCurrent_->setText(formatTime(pos));
    timeDuration_->setText(formatTime(duration));
    if (duration > 0 && !seekSlider_->isSliderDown())
        seekSlider_->setValue(static_cast<int>(pos / duration * 1000));

    // 가사 위치 업데이트
    if (lyricsWidget_)
        lyricsWidget_->setPosition(pos);
}

void MusicWidget::setPlaying(bool playing) {
    isPlaying_ = playing;
    btnPlay_->setText(playing ? "⏸" : "▶");
}

void MusicWidget::updateSpectrum(const QVector<float>& bins) {
    if (bins.size() != specBins_.size()) return;
    for (int i = 0; i < bins.size(); ++i) {
        specBins_[i] = bins[i];
        if (bins[i] > specPeak_[i]) specPeak_[i] = bins[i];
    }
    update();
}

void MusicWidget::paintEvent(QPaintEvent* /*e*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // ── 배경 ────────────────────────────────────────────────────────
    p.fillRect(rect(), QColor(8, 8, 8));

    if (!blurBg_.isNull()) {
        // 블러된 앨범 아트를 배경으로 (매우 어둡게)
        QPixmap bg = blurBg_.scaled(size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        p.setOpacity(0.12);
        p.drawPixmap(0, 0, bg);
        p.setOpacity(1.0);
        // 어두운 오버레이
        p.fillRect(rect(), QColor(0, 0, 0, 180));
    }

    // ── 스펙트럼 그리기 ─────────────────────────────────────────────
    // 좌측 패널 내 스펙트럼 영역 찾기
    QWidget* specArea = findChild<QWidget*>("specArea");
    if (specArea && !specBins_.isEmpty()) {
        QRect specRect = specArea->geometry();
        // 좌측 패널의 위치를 고려
        QWidget* leftPanel = specArea->parentWidget();
        if (leftPanel) {
            QPoint offset = leftPanel->mapTo(this, QPoint(0, 0));
            specRect.translate(offset);
        }
        drawSpectrum(p, specRect);
    }
}

void MusicWidget::drawSpectrum(QPainter& p, const QRect& rect) {
    if (specBins_.isEmpty()) return;

    const int n = specBins_.size();
    const float barW = static_cast<float>(rect.width()) / n;
    const int gap = 1;

    for (int i = 0; i < n; ++i) {
        float val = qBound(0.0f, specBins_[i], 1.0f);
        int barH = static_cast<int>(val * rect.height());

        // 그라데이션 색상: 저음=파랑, 중음=청록, 고음=흰색
        float t = static_cast<float>(i) / n;
        int r = static_cast<int>(21  + t * (255 - 21));
        int g = static_cast<int>(101 + t * (255 - 101));
        int b = static_cast<int>(192 + t * (255 - 192));
        p.fillRect(
            rect.left() + static_cast<int>(i * barW),
            rect.bottom() - barH,
            static_cast<int>(barW) - gap,
            barH,
            QColor(r, g, b, 200));

        // 피크 홀드 라인
        float peakVal = qBound(0.0f, specPeak_[i], 1.0f);
        int peakH = static_cast<int>(peakVal * rect.height());
        if (peakH > 2) {
            p.fillRect(
                rect.left() + static_cast<int>(i * barW),
                rect.bottom() - peakH,
                static_cast<int>(barW) - gap,
                2,
                QColor(255, 255, 255, 180));
        }
    }
}

void MusicWidget::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    update();
}

QString MusicWidget::formatTime(double secs) const {
    if (secs < 0) secs = 0;
    int s = static_cast<int>(secs);
    int m = s / 60; s %= 60;
    int h = m / 60; m %= 60;
    if (h > 0)
        return QString("%1:%2:%3").arg(h).arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
    return QString("%1:%2").arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
}
