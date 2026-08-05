#include "MediaInfoOverlay.h"
#include "MpvCore.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QPropertyAnimation>
#include <QSettings>

// ── 색상 상수 (랜딩페이지와 동일) ─────────────────────────────────
static const QString BG_MAIN   = "#0a0a12";
static const QString BG_CARD_A = "rgba(0,200,180,0.07)";
static const QString BG_CARD_V = "rgba(79,142,247,0.07)";
static const QString BORDER_A  = "rgba(0,200,180,0.18)";
static const QString BORDER_V  = "rgba(79,142,247,0.20)";
static const QString TEAL      = "#00c8b4";
static const QString BLUE      = "#4F8EF7";
static const QString TEXT_DIM  = "#8888a8";
static const QString TEXT_MAIN = "#eeeef8";

// ── 헬퍼: 구분선 ──────────────────────────────────────────────────
static QFrame* makeSep() {
    auto* f = new QFrame;
    f->setFrameShape(QFrame::HLine);
    f->setStyleSheet("background:rgba(255,255,255,0.06);border:none;max-height:1px;");
    return f;
}

// ── 헬퍼: 정보 행 (라벨 + 값) ─────────────────────────────────────
static QWidget* makeRow(const QString& label, QLabel*& valLabel,
                        const QString& valColor = TEXT_MAIN) {
    auto* w = new QWidget;
    auto* h = new QHBoxLayout(w);
    h->setContentsMargins(0,0,0,0); h->setSpacing(4);

    auto* lbl = new QLabel(label);
    lbl->setStyleSheet(QString("color:%1;font-size:11px;").arg(TEXT_DIM));

    valLabel = new QLabel("-");
    valLabel->setStyleSheet(QString("color:%1;font-size:11px;font-weight:600;").arg(valColor));
    valLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    h->addWidget(lbl);
    h->addStretch();
    h->addWidget(valLabel);
    return w;
}

// ── 헬퍼: 뱃지 라벨 ───────────────────────────────────────────────
static QLabel* makeBadge(const QString& text, const QString& bg,
                          const QString& fg = "#0a0a0f") {
    auto* b = new QLabel(text);
    b->setStyleSheet(QString(
        "background:%1;color:%2;padding:2px 9px;"
        "border-radius:4px;font-size:12px;font-weight:800;").arg(bg, fg));
    b->setFixedHeight(22);
    return b;
}

// ─────────────────────────────────────────────────────────────────
MediaInfoOverlay::MediaInfoOverlay(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(Qt::Widget);
    buildUi();
    hide();
}

void MediaInfoOverlay::buildUi() {
    // 전체 너비 178px (랜딩페이지와 동일)
    setFixedWidth(178);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0,0,0,0);
    root->setSpacing(0);

    // ── 패널 컨테이너 ──────────────────────────────────────────────
    auto* panel = new QWidget;
    panel->setObjectName("mediaInfoPanel");
    panel->setStyleSheet(QString(
        "#mediaInfoPanel {"
        "  background:%1;"
        "  border-right:1px solid rgba(255,255,255,0.07);"
        "}").arg(BG_MAIN));

    auto* vbox = new QVBoxLayout(panel);
    vbox->setContentsMargins(8,10,8,10);
    vbox->setSpacing(2);

    // ── 탭 목록 ────────────────────────────────────────────────────
    auto makeTab = [&](const QString& text, bool active) -> QLabel* {
        auto* t = new QLabel(text);
        if (active) {
            t->setStyleSheet(QString(
                "color:%1;font-size:13px;font-weight:500;"
                "background:rgba(0,200,180,0.13);"
                "border-radius:6px;padding:5px 8px;").arg(TEAL));
        } else {
            t->setStyleSheet(QString(
                "color:%1;font-size:13px;font-weight:400;"
                "padding:5px 8px;").arg(TEXT_DIM));
        }
        return t;
    };

    tabPlaylist_ = makeTab("재생목록", true);
    tabAudio_    = makeTab("오디오",   false);
    tabVideo_    = makeTab("화질",     false);
    tabSubtitle_ = makeTab("자막",     false);
    tabPro_      = makeTab("전문 기능",false);

    vbox->addWidget(tabPlaylist_);
    vbox->addWidget(tabAudio_);
    vbox->addWidget(tabVideo_);
    vbox->addWidget(tabSubtitle_);
    vbox->addWidget(tabPro_);
    vbox->addSpacing(6);

    // ── 현재 오디오 카드 ───────────────────────────────────────────
    auto* audioCard = new QWidget;
    audioCard->setStyleSheet(QString(
        "background:%1;"
        "border:1px solid %2;"
        "border-radius:8px;").arg(BG_CARD_A, BORDER_A));
    auto* av = new QVBoxLayout(audioCard);
    av->setContentsMargins(10,8,10,8); av->setSpacing(3);

    auto* audioTitle = new QLabel("현재 오디오");
    audioTitle->setStyleSheet(QString("color:%1;font-size:10px;").arg(TEXT_DIM));
    av->addWidget(audioTitle);

    audioBadge_ = makeBadge("—", TEAL);
    av->addWidget(audioBadge_);
    av->addSpacing(2);
    av->addWidget(makeRow("원본",     audioOriginal_));
    av->addWidget(makeRow("출력",     audioOutput_,  TEAL));
    av->addWidget(makeRow("WASAPI",   audioWasapi_,  TEAL));
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
    videoTitle->setStyleSheet(QString("color:%1;font-size:10px;").arg(TEXT_DIM));
    vv->addWidget(videoTitle);

    videoBadge_ = makeBadge("—", BLUE, "#fff");
    vv->addWidget(videoBadge_);
    vv->addSpacing(2);
    vv->addWidget(makeRow("원본",   videoOriginal_));
    vv->addWidget(makeRow("출력",   videoOutput_,  BLUE));
    vv->addWidget(makeRow("색공간", videoColor_));
    vv->addWidget(makeRow("HDR",    videoHdr_,     BLUE));

    vbox->addWidget(videoCard);
    vbox->addStretch();

    root->addWidget(panel);

    // ── 슬라이드 애니메이션 ────────────────────────────────────────
    anim_ = new QPropertyAnimation(this, "maximumWidth", this);
    anim_->setDuration(200);
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
    if (isVisible()) {
        // 닫기: 슬라이드 아웃
        anim_->stop();
        anim_->setStartValue(178);
        anim_->setEndValue(0);
        connect(anim_, &QPropertyAnimation::finished, this, [this](){
            hide();
            setMaximumWidth(178);
            disconnect(anim_, &QPropertyAnimation::finished, nullptr, nullptr);
        });
        anim_->start();
    } else {
        // 열기: 슬라이드 인
        setMaximumWidth(0);
        show();
        raise();
        anim_->stop();
        anim_->setStartValue(0);
        anim_->setEndValue(178);
        connect(anim_, &QPropertyAnimation::finished, this, [this](){
            setMaximumWidth(178);
            disconnect(anim_, &QPropertyAnimation::finished, nullptr, nullptr);
        });
        anim_->start();
    }
}

void MediaInfoOverlay::onFileLoaded(const QString&) {
    if (!core_) return;

    // 영상 정보 즉시 갱신
    QVariant w = core_->getProperty("video-params/w");
    QVariant h = core_->getProperty("video-params/h");
    QVariant fps = core_->getProperty("container-fps");
    QVariant codec = core_->getProperty("video-codec");
    if (w.isValid() && h.isValid())
        updateVideoInfo(w.toInt(), h.toInt(), fps.toDouble(),
                        codec.toString());

    // 오디오 정보 즉시 갱신
    QVariant acodec = core_->getProperty("audio-codec-name");
    QVariant ach    = core_->getProperty("audio-channels");
    QVariant asr    = core_->getProperty("audio-samplerate");
    QVariant aout   = core_->getProperty("audio-out-params/format");
    if (acodec.isValid())
        updateAudioInfo(acodec.toString(),
                        ach.toInt(), asr.toInt(),
                        aout.toString());
}

void MediaInfoOverlay::updateAudioInfo(const QString& codec, int channels,
                                        int sampleRate, const QString& output) {
    // 뱃지: 코덱명 정리
    QString badge = formatCodecBadge(codec);
    audioBadge_->setText(badge.isEmpty() ? "—" : badge);

    // 원본
    QString chStr = formatChannels(channels);
    QString orig = codec.toUpper();
    if (!chStr.isEmpty()) orig += " " + chStr;
    audioOriginal_->setText(orig.isEmpty() ? "—" : orig);

    // 출력 방식
    bool isPassthrough = output.contains("spdif", Qt::CaseInsensitive) ||
                         codec.contains("truehd", Qt::CaseInsensitive) ||
                         codec.contains("dts", Qt::CaseInsensitive) ||
                         codec.contains("eac3", Qt::CaseInsensitive) ||
                         codec.contains("ac3", Qt::CaseInsensitive);
    audioOutput_->setText(isPassthrough ? "패스스루" : "PCM 디코딩");
    audioOutput_->setStyleSheet(QString("color:%1;font-size:11px;font-weight:600;")
                                 .arg(isPassthrough ? TEAL : TEXT_MAIN));

    // WASAPI 모드
    QSettings s("가온미디어", "소리누리");
    bool exclusive = s.value("audio/exclusive", true).toBool();
    audioWasapi_->setText(exclusive ? "독점" : "공유");
    audioWasapi_->setStyleSheet(QString("color:%1;font-size:11px;font-weight:600;")
                                 .arg(exclusive ? TEAL : TEXT_DIM));

    // 비트퍼펙트
    bool bitperfect = isPassthrough || (output.contains("s32") || output.contains("s24") || output.contains("float"));
    audioBitperf_->setText(bitperfect ? "✓" : "—");
    audioBitperf_->setStyleSheet(QString("color:%1;font-size:11px;font-weight:600;")
                                  .arg(bitperfect ? TEAL : TEXT_DIM));
}

void MediaInfoOverlay::updateVideoInfo(int width, int height, double fps,
                                        const QString& codec) {
    // 뱃지
    QString badge = codec.toUpper();
    if (badge.contains("HEVC") || badge.contains("H265")) badge = "HEVC";
    else if (badge.contains("H264") || badge.contains("AVC")) badge = "H.264";
    else if (badge.contains("AV1"))  badge = "AV1";
    else if (badge.contains("VP9"))  badge = "VP9";
    videoBadge_->setText(badge.isEmpty() ? "—" : badge);

    // 원본 해상도
    QString res;
    if (width >= 3840)      res = "4K";
    else if (width >= 2560) res = "2K";
    else if (width >= 1920) res = "FHD";
    else if (width >= 1280) res = "HD";
    else if (width > 0)     res = QString("%1×%2").arg(width).arg(height);

    // HDR 여부
    QString hdrStr = "—";
    QString colorStr = "—";
    if (core_) {
        QVariant colorspace = core_->getProperty("video-params/colormatrix");
        QVariant primaries  = core_->getProperty("video-params/primaries");
        QVariant transfer   = core_->getProperty("video-params/gamma");

        QString cs = colorspace.toString();
        QString pr = primaries.toString();
        QString tr = transfer.toString();

        if (pr.contains("bt.2020") || pr.contains("2020"))
            colorStr = "BT.2020";
        else if (pr.contains("bt.709") || pr.contains("709"))
            colorStr = "BT.709";

        if (tr.contains("pq") || tr.contains("smpte2084"))
            hdrStr = "PQ (HDR10)";
        else if (tr.contains("hlg"))
            hdrStr = "HLG";
        else if (tr.contains("dovi") || tr.contains("dolby"))
            hdrStr = "Dolby Vision";
        else
            hdrStr = "SDR";
    }

    // FPS 포함 원본 문자열
    QString orig = res;
    if (!hdrStr.isEmpty() && hdrStr != "SDR" && hdrStr != "—")
        orig += " " + hdrStr.section(" ", 0, 0); // "HDR10" 등 앞부분만
    videoOriginal_->setText(orig.isEmpty() ? "—" : orig);

    // 출력 (hwdec)
    QString hwdec = "소프트웨어";
    if (core_) {
        QVariant hd = core_->getProperty("hwdec-current");
        QString hdStr = hd.toString();
        if (hdStr.contains("d3d11va"))     hwdec = "D3D11VA";
        else if (hdStr.contains("d3d12va")) hwdec = "D3D12VA";
        else if (hdStr.contains("nvdec"))   hwdec = "NVDEC";
        else if (hdStr.contains("cuda"))    hwdec = "CUDA";
        else if (hdStr.contains("no") || hdStr.isEmpty()) hwdec = "소프트웨어";
    }
    videoOutput_->setText(hwdec);
    videoOutput_->setStyleSheet(QString("color:%1;font-size:11px;font-weight:600;")
                                 .arg(hwdec == "소프트웨어" ? TEXT_DIM : BLUE));

    videoColor_->setText(colorStr);
    videoHdr_->setText(hdrStr);
    videoHdr_->setStyleSheet(QString("color:%1;font-size:11px;font-weight:600;")
                              .arg((hdrStr == "SDR" || hdrStr == "—") ? TEXT_DIM : BLUE));
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
