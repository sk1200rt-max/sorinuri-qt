#include "MediaInfoOverlay.h"
#include "MpvCore.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QPropertyAnimation>
#include <QSettings>
#include <QScrollArea>
#include <QMouseEvent>
#include <QEvent>

// ── 색상 상수 (랜딩페이지와 동일) ─────────────────────────────────
static const QString BG_MAIN   = "#0d0d1a";
static const QString BG_CARD_A = "#111122";
static const QString BG_CARD_V = "#0f1120";
static const QString BORDER_A  = "#1a3a36";
static const QString BORDER_V  = "#1a2a4a";
static const QString TEAL      = "#00c8b4";
static const QString BLUE      = "#4F8EF7";
static const QString TEXT_DIM  = "#8888a8";
static const QString TEXT_MAIN = "#eeeef8";

static const int PANEL_W = 180;

// ── 헬퍼: 정보 행 ─────────────────────────────────────────────────
static QWidget* makeRow(const QString& label, QLabel*& valLabel,
                        const QString& valColor = TEXT_MAIN) {
    auto* w = new QWidget;
    w->setStyleSheet("background:transparent;");
    auto* h = new QHBoxLayout(w);
    h->setContentsMargins(0,1,0,1); h->setSpacing(4);

    auto* lbl = new QLabel(label);
    lbl->setStyleSheet(QString("color:%1;font-size:11px;background:transparent;").arg(TEXT_DIM));

    valLabel = new QLabel("-");
    valLabel->setStyleSheet(QString("color:%1;font-size:11px;font-weight:600;background:transparent;").arg(valColor));
    valLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    h->addWidget(lbl);
    h->addStretch();
    h->addWidget(valLabel);
    return w;
}

// ── 헬퍼: 뱃지 라벨 ───────────────────────────────────────────────
static QLabel* makeBadge(const QString& text, const QString& bg,
                          const QString& fg = "#ffffff") {
    auto* b = new QLabel(text);
    b->setStyleSheet(QString(
        "background:%1;color:%2;padding:2px 8px;"
        "border-radius:4px;font-size:13px;font-weight:800;").arg(bg, fg));
    b->setFixedHeight(24);
    return b;
}

// ─────────────────────────────────────────────────────────────────
MediaInfoOverlay::MediaInfoOverlay(QWidget* parent)
    : QWidget(parent)
{
    // 레이아웃 없이 절대 위치로 배치 (geometry 애니메이션용)
    setWindowFlags(Qt::Widget);
    // 배경색 직접 지정 (WA_TranslucentBackground 사용 안 함)
    setStyleSheet(QString("MediaInfoOverlay { background:%1; }").arg(BG_MAIN));
    setAttribute(Qt::WA_StyledBackground, true);

    buildUi();
    hide();
}

void MediaInfoOverlay::buildUi() {
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setStyleSheet(QString(
        "QScrollArea { background:%1; border:none; }"
        "QScrollBar:vertical { width:4px; background:transparent; }"
        "QScrollBar::handle:vertical { background:#333355; border-radius:2px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }").arg(BG_MAIN));

    auto* content = new QWidget;
    content->setStyleSheet(QString("background:%1;").arg(BG_MAIN));
    auto* vbox = new QVBoxLayout(content);
    vbox->setContentsMargins(8,10,8,10);
    vbox->setSpacing(2);

    // ── 탭 목록 ────────────────────────────────────────────────────
    auto makeTab = [&](const QString& text, bool active) -> QLabel* {
        auto* t = new QLabel(text);
        if (active) {
            t->setStyleSheet(QString(
                "color:%1;font-size:13px;font-weight:600;"
                "background:rgba(0,200,180,0.15);"
                "border-radius:6px;padding:5px 8px;").arg(TEAL));
        } else {
            t->setStyleSheet(QString(
                "color:%1;font-size:13px;font-weight:400;"
                "background:transparent;"
                "padding:5px 8px;").arg(TEXT_DIM));
        }
        return t;
    };

    tabPlaylist_ = makeTab("재생목록", true);
    tabAudio_    = makeTab("오디오",   false);
    tabVideo_    = makeTab("화질",     false);
    tabSubtitle_ = makeTab("자막",     false);
    tabPro_      = makeTab("전문 기능",false);

    // 탭 클릭 가능하게: 커서 + eventFilter 설치
    auto setupTabClick = [this](QLabel* tab) {
        tab->setCursor(Qt::PointingHandCursor);
        tab->setAttribute(Qt::WA_Hover, true);
        tab->installEventFilter(this);
    };
    setupTabClick(tabPlaylist_);
    setupTabClick(tabAudio_);
    setupTabClick(tabVideo_);
    setupTabClick(tabSubtitle_);
    setupTabClick(tabPro_);

    vbox->addWidget(tabPlaylist_);
    vbox->addWidget(tabAudio_);
    vbox->addWidget(tabVideo_);
    vbox->addWidget(tabSubtitle_);
    vbox->addWidget(tabPro_);
    vbox->addSpacing(8);

    // ── 현재 오디오 카드 ───────────────────────────────────────────
    auto* audioCard = new QWidget;
    audioCard->setStyleSheet(QString(
        "background:%1;"
        "border:1px solid %2;"
        "border-radius:8px;").arg(BG_CARD_A, BORDER_A));
    auto* av = new QVBoxLayout(audioCard);
    av->setContentsMargins(10,8,10,8); av->setSpacing(3);

    auto* audioTitle = new QLabel("현재 오디오");
    audioTitle->setStyleSheet(QString("color:%1;font-size:10px;background:transparent;").arg(TEXT_DIM));
    av->addWidget(audioTitle);

    audioBadge_ = makeBadge("—", TEAL, "#0a0a12");
    av->addWidget(audioBadge_);
    av->addSpacing(2);
    av->addWidget(makeRow("원본",       audioOriginal_));
    av->addWidget(makeRow("출력",       audioOutput_,  TEAL));
    av->addWidget(makeRow("WASAPI",     audioWasapi_,  TEAL));
    av->addWidget(makeRow("비트퍼펙트", audioBitperf_, TEAL));

    vbox->addWidget(audioCard);
    vbox->addSpacing(8);

    // ── 현재 영상 카드 ─────────────────────────────────────────────
    auto* videoCard = new QWidget;
    videoCard->setStyleSheet(QString(
        "background:%1;"
        "border:1px solid %2;"
        "border-radius:8px;").arg(BG_CARD_V, BORDER_V));
    auto* vv = new QVBoxLayout(videoCard);
    vv->setContentsMargins(10,8,10,8); vv->setSpacing(3);

    auto* videoTitle = new QLabel("현재 영상");
    videoTitle->setStyleSheet(QString("color:%1;font-size:10px;background:transparent;").arg(TEXT_DIM));
    vv->addWidget(videoTitle);

    videoBadge_ = makeBadge("—", BLUE, "#ffffff");
    vv->addWidget(videoBadge_);
    vv->addSpacing(2);
    vv->addWidget(makeRow("원본",   videoOriginal_));
    vv->addWidget(makeRow("출력",   videoOutput_,  BLUE));
    vv->addWidget(makeRow("색공간", videoColor_));
    vv->addWidget(makeRow("HDR",    videoHdr_,     BLUE));

    vbox->addWidget(videoCard);
    vbox->addStretch();

    scroll->setWidget(content);

    // scroll을 this 전체에 꽉 채움 (geometry 애니메이션으로 this 크기가 바뀌면 자동 따라옴)
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0,0,0,0);
    rootLayout->setSpacing(0);
    rootLayout->addWidget(scroll);

    // ── geometry 기반 슬라이드 애니메이션 ─────────────────────────
    anim_ = new QPropertyAnimation(this, "geometry", this);
    anim_->setDuration(220);
    anim_->setEasingCurve(QEasingCurve::InOutQuad);
}

void MediaInfoOverlay::connectMpv(MpvCore* core) {
    core_ = core;
    connect(core_, &MpvCore::audioFormatChanged,
            this,  &MediaInfoOverlay::updateAudioInfo);
    connect(core_, &MpvCore::videoInfoChanged,
            this,  &MediaInfoOverlay::updateVideoInfo);
    connect(core_, &MpvCore::fileLoaded,
            this,  &MediaInfoOverlay::onFileLoaded);
}

void MediaInfoOverlay::toggle() {
    if (!parentWidget()) return;
    int ph = parentWidget()->height();

    if (isVisible()) {
        // 닫기: 왼쪽으로 슬라이드 아웃
        anim_->stop();
        disconnect(anim_, &QPropertyAnimation::finished, nullptr, nullptr);
        anim_->setStartValue(QRect(0, 0, PANEL_W, ph));
        anim_->setEndValue  (QRect(-PANEL_W, 0, PANEL_W, ph));
        connect(anim_, &QPropertyAnimation::finished, this, [this](){
            hide();
            disconnect(anim_, &QPropertyAnimation::finished, nullptr, nullptr);
        });
        anim_->start();
    } else {
        // 열기: 오른쪽으로 슬라이드 인
        setGeometry(-PANEL_W, 0, PANEL_W, ph);
        show();
        raise();
        anim_->stop();
        disconnect(anim_, &QPropertyAnimation::finished, nullptr, nullptr);
        anim_->setStartValue(QRect(-PANEL_W, 0, PANEL_W, ph));
        anim_->setEndValue  (QRect(0,        0, PANEL_W, ph));
        anim_->start();
    }
}

void MediaInfoOverlay::onFileLoaded(const QString&) {
    if (!core_) return;

    QVariant w = core_->getProperty("video-params/w");
    QVariant h = core_->getProperty("video-params/h");
    QVariant fps = core_->getProperty("container-fps");
    QVariant codec = core_->getProperty("video-codec");
    if (w.isValid() && h.isValid())
        updateVideoInfo(w.toInt(), h.toInt(), fps.toDouble(), codec.toString());

    QVariant acodec = core_->getProperty("audio-codec-name");
    QVariant ach    = core_->getProperty("audio-channels");
    QVariant asr    = core_->getProperty("audio-samplerate");
    QVariant aout   = core_->getProperty("audio-out-params/format");
    if (acodec.isValid())
        updateAudioInfo(acodec.toString(), ach.toInt(), asr.toInt(), aout.toString());
}

void MediaInfoOverlay::updateAudioInfo(const QString& codec, int channels,
                                        int sampleRate, const QString& output) {
    QString badge = formatCodecBadge(codec);
    audioBadge_->setText(badge.isEmpty() ? "—" : badge);

    QString chStr = formatChannels(channels);
    QString orig = codec.toUpper();
    if (!chStr.isEmpty()) orig += " " + chStr;
    audioOriginal_->setText(orig.isEmpty() ? "—" : orig);

    bool isPassthrough = output.contains("spdif", Qt::CaseInsensitive) ||
                         codec.contains("truehd", Qt::CaseInsensitive) ||
                         codec.contains("dts",    Qt::CaseInsensitive) ||
                         codec.contains("eac3",   Qt::CaseInsensitive) ||
                         codec.contains("ac3",    Qt::CaseInsensitive);
    audioOutput_->setText(isPassthrough ? "패스스루" : "PCM 디코딩");
    audioOutput_->setStyleSheet(QString("color:%1;font-size:11px;font-weight:600;background:transparent;")
                                 .arg(isPassthrough ? TEAL : TEXT_MAIN));

    QSettings s("가온미디어", "소리누리");
    bool exclusive = s.value("audio/exclusive", true).toBool();
    audioWasapi_->setText(exclusive ? "독점" : "공유");
    audioWasapi_->setStyleSheet(QString("color:%1;font-size:11px;font-weight:600;background:transparent;")
                                 .arg(exclusive ? TEAL : TEXT_DIM));

    bool bitperfect = isPassthrough ||
                      output.contains("s32") || output.contains("s24") || output.contains("float");
    audioBitperf_->setText(bitperfect ? "✓" : "—");
    audioBitperf_->setStyleSheet(QString("color:%1;font-size:11px;font-weight:600;background:transparent;")
                                  .arg(bitperfect ? TEAL : TEXT_DIM));
}

void MediaInfoOverlay::updateVideoInfo(int width, int height, double /*fps*/,
                                        const QString& codec) {
    QString badge = codec.toUpper();
    if (badge.contains("HEVC") || badge.contains("H265")) badge = "HEVC";
    else if (badge.contains("H264") || badge.contains("AVC")) badge = "H.264";
    else if (badge.contains("AV1"))  badge = "AV1";
    else if (badge.contains("VP9"))  badge = "VP9";
    videoBadge_->setText(badge.isEmpty() ? "—" : badge);

    QString res;
    if (width >= 3840)      res = "4K";
    else if (width >= 2560) res = "2K";
    else if (width >= 1920) res = "FHD";
    else if (width >= 1280) res = "HD";
    else if (width > 0)     res = QString("%1×%2").arg(width).arg(height);

    QString hdrStr = "SDR", colorStr = "—";
    if (core_) {
        QString pr = core_->getProperty("video-params/primaries").toString();
        QString tr = core_->getProperty("video-params/gamma").toString();

        if      (pr.contains("2020"))  colorStr = "BT.2020";
        else if (pr.contains("709"))   colorStr = "BT.709";

        if      (tr.contains("pq") || tr.contains("smpte2084")) hdrStr = "PQ (HDR10)";
        else if (tr.contains("hlg"))   hdrStr = "HLG";
        else if (tr.contains("dovi"))  hdrStr = "Dolby Vision";
    }

    QString orig = res;
    if (hdrStr != "SDR" && hdrStr != "—")
        orig += " " + hdrStr.section(" ", 0, 0);
    videoOriginal_->setText(orig.isEmpty() ? "—" : orig);

    QString hwdec = "소프트웨어";
    if (core_) {
        QString hd = core_->getProperty("hwdec-current").toString();
        if      (hd.contains("d3d11va"))  hwdec = "D3D11VA";
        else if (hd.contains("d3d12va"))  hwdec = "D3D12VA";
        else if (hd.contains("nvdec"))    hwdec = "NVDEC";
        else if (hd.contains("cuda"))     hwdec = "CUDA";
    }
    videoOutput_->setText(hwdec);
    videoOutput_->setStyleSheet(QString("color:%1;font-size:11px;font-weight:600;background:transparent;")
                                 .arg(hwdec == "소프트웨어" ? TEXT_DIM : BLUE));

    videoColor_->setText(colorStr);
    videoHdr_->setText(hdrStr);
    videoHdr_->setStyleSheet(QString("color:%1;font-size:11px;font-weight:600;background:transparent;")
                              .arg((hdrStr == "SDR" || hdrStr == "—") ? TEXT_DIM : BLUE));
}

// ── 탭 클릭 eventFilter ─────────────────────────────────────────────
bool MediaInfoOverlay::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::MouseButtonRelease) {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            if (obj == tabPlaylist_)  { setActiveTab(tabPlaylist_);  emit tabClicked(0); return true; }
            if (obj == tabAudio_)     { setActiveTab(tabAudio_);     emit tabClicked(1); return true; }
            if (obj == tabVideo_)     { setActiveTab(tabVideo_);     emit tabClicked(2); return true; }
            if (obj == tabSubtitle_)  { setActiveTab(tabSubtitle_);  emit tabClicked(3); return true; }
            if (obj == tabPro_)       { setActiveTab(tabPro_);       emit tabClicked(4); return true; }
        }
    }
    // hover 스타일
    if (event->type() == QEvent::HoverEnter) {
        auto* lbl = qobject_cast<QLabel*>(obj);
        if (lbl && lbl != tabPlaylist_) {  // 현재 활성 탭은 hover 안 바꿼
            bool isActive = lbl->styleSheet().contains("rgba(0,200,180");
            if (!isActive)
                lbl->setStyleSheet(QString(
                    "color:#e0e0e0;font-size:13px;font-weight:500;"
                    "background:rgba(255,255,255,0.06);"
                    "border-radius:6px;padding:5px 8px;"));
        }
    }
    if (event->type() == QEvent::HoverLeave) {
        auto* lbl = qobject_cast<QLabel*>(obj);
        if (lbl) {
            bool isActive = lbl->styleSheet().contains("rgba(0,200,180");
            if (!isActive)
                lbl->setStyleSheet(QString(
                    "color:%1;font-size:13px;font-weight:400;"
                    "background:transparent;padding:5px 8px;").arg(TEXT_DIM));
        }
    }
    return QWidget::eventFilter(obj, event);
}

void MediaInfoOverlay::setActiveTab(QLabel* activeTab) {
    QList<QLabel*> tabs = {tabPlaylist_, tabAudio_, tabVideo_, tabSubtitle_, tabPro_};
    for (auto* t : tabs) {
        if (t == activeTab) {
            t->setStyleSheet(QString(
                "color:%1;font-size:13px;font-weight:600;"
                "background:rgba(0,200,180,0.15);"
                "border-radius:6px;padding:5px 8px;").arg(TEAL));
        } else {
            t->setStyleSheet(QString(
                "color:%1;font-size:13px;font-weight:400;"
                "background:transparent;padding:5px 8px;").arg(TEXT_DIM));
        }
    }
}

QString MediaInfoOverlay::formatChannels(int ch) const {
    switch (ch) {
    case 1: return "1.0ch";
    case 2: return "2.0ch";
    case 6: return "5.1ch";
    case 7: return "6.1ch";
    case 8: return "7.1ch";
    default: return ch > 0 ? QString("%1ch").arg(ch) : "";
    }
}

QString MediaInfoOverlay::formatCodecBadge(const QString& codec) const {
    QString c = codec.toUpper();
    if (c.contains("TRUEHD"))  return "TrueHD";
    if (c.contains("DTS-HD"))  return "DTS-HD";
    if (c.contains("DTS"))     return "DTS";
    if (c.contains("EAC3"))    return "DD+";
    if (c.contains("AC3"))     return "DD";
    if (c.contains("AAC"))     return "AAC";
    if (c.contains("FLAC"))    return "FLAC";
    if (c.contains("MP3"))     return "MP3";
    if (c.contains("OPUS"))    return "Opus";
    if (c.contains("PCM"))     return "PCM";
    return codec.left(8).toUpper();
}
