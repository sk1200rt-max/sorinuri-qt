#include "InfoOverlayWidget.h"
#include <QFileInfo>
#include <QScrollArea>
#include <QSizePolicy>
#include <QApplication>
#include <QScreen>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
// 헬퍼 함수
// ─────────────────────────────────────────────────────────────────────────────
static QString formatTime(double sec) {
    if (sec < 0) sec = 0;
    int h = (int)sec / 3600;
    int m = ((int)sec % 3600) / 60;
    int s = (int)sec % 60;
    if (h > 0)
        return QString("%1:%2:%3").arg(h).arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
    return QString("%1:%2").arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
}

static QString channelName(int ch) {
    switch (ch) {
        case 1: return "1.0 모노";
        case 2: return "2.0 스테레오";
        case 4: return "4.0";
        case 6: return "5.1";
        case 7: return "6.1";
        case 8: return "7.1";
        default: return QString("%1ch").arg(ch);
    }
}

static QString shortCodec(const QString& c) {
    QString u = c.toUpper();
    if (u.contains("TRUEHD")) return "TrueHD";
    if (u.contains("DTS-HD") || u.contains("DTSHD")) return "DTS-HD";
    if (u.contains("DTS"))  return "DTS";
    if (u.contains("EAC3") || u.contains("E-AC-3")) return "E-AC3";
    if (u.contains("AC3")  || u.contains("AC-3"))   return "AC3";
    if (u.contains("FLAC")) return "FLAC";
    if (u.contains("AAC"))  return "AAC";
    if (u.contains("MP3"))  return "MP3";
    if (u.contains("OPUS")) return "Opus";
    if (u.contains("PCM"))  return "PCM";
    if (u.contains("HEVC") || u.contains("H265") || u.contains("H.265")) return "HEVC";
    if (u.contains("AVC")  || u.contains("H264") || u.contains("H.264")) return "H.264";
    if (u.contains("AV1"))  return "AV1";
    if (u.contains("VP9"))  return "VP9";
    if (u.contains("VP8"))  return "VP8";
    if (u.contains("MPEG2")) return "MPEG2";
    if (u.contains("MPEG4")) return "MPEG4";
    return c.isEmpty() ? "-" : c.left(8);
}

// ─────────────────────────────────────────────────────────────────────────────
// 생성자
// ─────────────────────────────────────────────────────────────────────────────
InfoOverlayWidget::InfoOverlayWidget(MpvCore* core, QWidget* parent)
    : QWidget(parent), core_(core)
{
    // 마우스 이벤트 통과 - 영상 클릭/드래그 방해 안 함
    setAttribute(Qt::WA_TransparentForMouseEvents);
    // 배경 투명
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
    setAutoFillBackground(false);

    // 1초 타이머 - 실시간 통계 갱신
    refreshTimer_ = new QTimer(this);
    refreshTimer_->setInterval(1000);
    connect(refreshTimer_, &QTimer::timeout, this, &InfoOverlayWidget::refresh);

    // 초기 레이아웃 (파일 없음 상태)
    buildLayout();
}

// ─────────────────────────────────────────────────────────────────────────────
// 표시/숨김
// ─────────────────────────────────────────────────────────────────────────────
void InfoOverlayWidget::setVisible(bool visible) {
    QWidget::setVisible(visible);
    if (visible && hasFile_) {
        refreshTimer_->start();
    } else {
        refreshTimer_->stop();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 파일 로드/정지 콜백
// ─────────────────────────────────────────────────────────────────────────────
void InfoOverlayWidget::onFileLoaded(const QString& path) {
    currentFile_ = path;
    hasFile_ = true;
    buildLayout();
    if (isVisible()) refreshTimer_->start();
}

void InfoOverlayWidget::onPlaybackStopped() {
    hasFile_ = false;
    videoCodec_.clear(); videoW_ = 0; videoH_ = 0; videoFps_ = 0;
    audioCodec_.clear(); audioChannels_ = 0; audioSampleRate_ = 0; audioOutput_.clear();
    refreshTimer_->stop();
    buildLayout();
}

void InfoOverlayWidget::onAudioFormatChanged(const QString& codec, int channels,
                                              int sampleRate, const QString& output) {
    audioCodec_      = codec;
    audioChannels_   = channels;
    audioSampleRate_ = sampleRate;
    audioOutput_     = output;
    if (lblAudioCodec_) lblAudioCodec_->setText(shortCodec(codec));
    if (lblChannels_)   lblChannels_->setText(channelName(channels));
    if (lblSampleRate_) lblSampleRate_->setText(
        sampleRate >= 1000 ? QString("%1 kHz").arg(sampleRate / 1000.0, 0, 'f', 1)
                           : QString("%1 Hz").arg(sampleRate));
    if (lblAudioOut_)   lblAudioOut_->setText(output.isEmpty() ? "-" : output);
}

void InfoOverlayWidget::onVideoInfoChanged(int width, int height, double fps,
                                            const QString& codec) {
    videoW_ = width; videoH_ = height; videoFps_ = fps; videoCodec_ = codec;
    if (lblResolution_) lblResolution_->setText(
        (width > 0 && height > 0) ? QString("%1×%2").arg(width).arg(height) : "-");
    if (lblFps_) lblFps_->setText(
        fps > 0 ? QString("%1 fps").arg(fps, 0, 'f', fps == (int)fps ? 0 : 2) : "-");
}

// ─────────────────────────────────────────────────────────────────────────────
// 실시간 통계 갱신 (1초마다)
// ─────────────────────────────────────────────────────────────────────────────
void InfoOverlayWidget::refresh() {
    if (!core_ || !hasFile_) return;

    // 재생 위치 / 길이
    double pos = core_->position();
    double dur = core_->duration();
    if (lblPosition_) lblPosition_->setText(formatTime(pos));
    if (lblDuration_) lblDuration_->setText(formatTime(dur));

    // 볼륨
    if (lblVolume_) {
        int vol = core_->volume();
        bool muted = core_->isMuted();
        lblVolume_->setText(muted ? "음소거" : QString("%1%").arg(vol));
    }

    // 드롭 프레임
    if (lblDropped_) {
        QVariant v = core_->getProperty("frame-drop-count");
        int dropped = v.isValid() ? v.toInt() : 0;
        lblDropped_->setText(QString::number(dropped));
        lblDropped_->setStyleSheet(
            dropped > 0
            ? "color: #ff6b6b; font-size: 16px; font-weight: bold;"
            : "color: #00c8b4; font-size: 16px; font-weight: bold;");
    }

    // A/V 동기화 오차
    if (lblAvSync_) {
        QVariant v = core_->getProperty("avsync");
        double avSync = v.isValid() ? v.toDouble() : 0.0;
        QString avStr = (avSync >= 0 ? "+" : "") + QString::number(avSync * 1000, 'f', 0) + " ms";
        lblAvSync_->setText(avStr);
        double absSync = std::abs(avSync * 1000);
        lblAvSync_->setStyleSheet(
            absSync > 50
            ? "color: #ff6b6b; font-size: 16px; font-weight: bold;"
            : absSync > 20
            ? "color: #ffd93d; font-size: 16px; font-weight: bold;"
            : "color: #00c8b4; font-size: 16px; font-weight: bold;");
    }

    // 비트레이트 (video + audio)
    if (lblBitrate_) {
        QVariant vv = core_->getProperty("video-bitrate");
        QVariant av = core_->getProperty("audio-bitrate");
        double vbr = vv.isValid() ? vv.toDouble() / 1000.0 : 0.0;  // kbps
        double abr = av.isValid() ? av.toDouble() / 1000.0 : 0.0;
        double total = vbr + abr;
        QString bStr;
        if (total >= 1000)
            bStr = QString("%1 Mbps").arg(total / 1000.0, 0, 'f', 1);
        else if (total > 0)
            bStr = QString("%1 kbps").arg((int)total);
        else
            bStr = "-";
        lblBitrate_->setText(bStr);
    }

    // 하드웨어 디코더
    if (lblHwdec_) {
        QVariant v = core_->getProperty("hwdec-current");
        QString hw = v.isValid() ? v.toString() : "";
        if (hw.isEmpty() || hw == "no" || hw == "none")
            lblHwdec_->setText("소프트웨어");
        else
            lblHwdec_->setText(hw.toUpper().replace("-COPY", " Copy").replace("D3D11VA", "D3D11VA"));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 레이아웃 빌드
// ─────────────────────────────────────────────────────────────────────────────
void InfoOverlayWidget::clearLayout() {
    if (container_) {
        delete container_;
        container_ = nullptr;
    }
    // 레이블 포인터 초기화
    lblDropped_ = lblAvSync_ = lblBitrate_ = nullptr;
    lblPosition_ = lblDuration_ = nullptr;
    lblHwdec_ = lblResolution_ = lblFps_ = nullptr;
    lblAudioCodec_ = lblChannels_ = lblSampleRate_ = lblAudioOut_ = nullptr;
    lblFileName_ = lblVolume_ = nullptr;
}

void InfoOverlayWidget::buildLayout() {
    clearLayout();
    if (hasFile_)
        buildPlayingLayout();
    else
        buildIdleLayout();
    if (container_) {
        container_->setParent(this);
        container_->setGeometry(rect());
        container_->show();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 재생 없음 레이아웃
// ─────────────────────────────────────────────────────────────────────────────
void InfoOverlayWidget::buildIdleLayout() {
    container_ = new QWidget(this);
    container_->setAttribute(Qt::WA_TransparentForMouseEvents);
    container_->setStyleSheet("background: transparent;");

    auto* vbox = new QVBoxLayout(container_);
    vbox->setContentsMargins(24, 24, 24, 24);
    vbox->setSpacing(16);
    vbox->addStretch(1);

    // 중앙 안내 텍스트
    auto* hint = new QLabel("파일을 드래그하거나\n우클릭 → 파일 열기", container_);
    hint->setAlignment(Qt::AlignCenter);
    hint->setStyleSheet(
        "color: rgba(255,255,255,0.25);"
        "font-size: 14px;"
        "background: transparent;"
        "border: none;");
    vbox->addWidget(hint, 0, Qt::AlignCenter);
    vbox->addStretch(1);
}

// ─────────────────────────────────────────────────────────────────────────────
// 재생 중 레이아웃 - 카드 대시보드
// ─────────────────────────────────────────────────────────────────────────────
void InfoOverlayWidget::buildPlayingLayout() {
    container_ = new QWidget(this);
    container_->setAttribute(Qt::WA_TransparentForMouseEvents);
    container_->setStyleSheet("background: transparent;");

    auto* mainLayout = new QVBoxLayout(container_);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(8);

    // ── 상단: 파일명 ──────────────────────────────────────────────
    lblFileName_ = new QLabel(container_);
    lblFileName_->setStyleSheet(
        "color: rgba(255,255,255,0.55);"
        "font-size: 11px;"
        "background: transparent;"
        "border: none;");
    lblFileName_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    if (!currentFile_.isEmpty()) {
        QString name = QFileInfo(currentFile_).fileName();
        lblFileName_->setText(name);
    }
    mainLayout->addWidget(lblFileName_);

    // ── 카드 그리드 (2열) ─────────────────────────────────────────
    auto* grid = new QGridLayout;
    grid->setSpacing(8);
    grid->setContentsMargins(0, 0, 0, 0);

    // ── 카드 1: 영상 정보 ─────────────────────────────────────────
    {
        auto* cardContent = new QWidget;
        cardContent->setStyleSheet("background: transparent;");
        auto* vb = new QVBoxLayout(cardContent);
        vb->setSpacing(4);
        vb->setContentsMargins(0, 0, 0, 0);

        // 코덱 + 해상도
        auto* row1 = new QHBoxLayout;
        row1->setSpacing(8);
        auto* codecLabel = new QLabel("코덱", cardContent);
        codecLabel->setStyleSheet("color: rgba(255,255,255,0.45); font-size: 10px; background: transparent; border: none;");
        lblResolution_ = new QLabel(
            (videoW_ > 0 && videoH_ > 0) ? QString("%1×%2").arg(videoW_).arg(videoH_) : "-",
            cardContent);
        lblResolution_->setStyleSheet("color: #fff; font-size: 13px; font-weight: bold; background: transparent; border: none;");
        row1->addWidget(codecLabel);
        row1->addStretch();
        row1->addWidget(lblResolution_);
        vb->addLayout(row1);

        // 코덱 값
        auto* codecVal = new QLabel(shortCodec(videoCodec_), cardContent);
        codecVal->setStyleSheet("color: #00c8b4; font-size: 20px; font-weight: bold; background: transparent; border: none;");
        vb->addWidget(codecVal);

        // FPS
        auto* row2 = new QHBoxLayout;
        row2->setSpacing(4);
        auto* fpsLabel = new QLabel("프레임레이트", cardContent);
        fpsLabel->setStyleSheet("color: rgba(255,255,255,0.45); font-size: 10px; background: transparent; border: none;");
        lblFps_ = new QLabel(
            videoFps_ > 0 ? QString("%1 fps").arg(videoFps_, 0, 'f', videoFps_ == (int)videoFps_ ? 0 : 2) : "-",
            cardContent);
        lblFps_->setStyleSheet("color: rgba(255,255,255,0.8); font-size: 12px; background: transparent; border: none;");
        row2->addWidget(fpsLabel);
        row2->addStretch();
        row2->addWidget(lblFps_);
        vb->addLayout(row2);

        // 하드웨어 디코더
        auto* row3 = new QHBoxLayout;
        row3->setSpacing(4);
        auto* hwLabel = new QLabel("디코더", cardContent);
        hwLabel->setStyleSheet("color: rgba(255,255,255,0.45); font-size: 10px; background: transparent; border: none;");
        lblHwdec_ = new QLabel("-", cardContent);
        lblHwdec_->setStyleSheet("color: rgba(255,255,255,0.8); font-size: 12px; background: transparent; border: none;");
        row3->addWidget(hwLabel);
        row3->addStretch();
        row3->addWidget(lblHwdec_);
        vb->addLayout(row3);

        grid->addWidget(makeCard("영상", cardContent, "#00c8b4"), 0, 0);
    }

    // ── 카드 2: 오디오 정보 ───────────────────────────────────────
    {
        auto* cardContent = new QWidget;
        cardContent->setStyleSheet("background: transparent;");
        auto* vb = new QVBoxLayout(cardContent);
        vb->setSpacing(4);
        vb->setContentsMargins(0, 0, 0, 0);

        // 코덱
        auto* row1 = new QHBoxLayout;
        auto* codecLabel = new QLabel("코덱", cardContent);
        codecLabel->setStyleSheet("color: rgba(255,255,255,0.45); font-size: 10px; background: transparent; border: none;");
        row1->addWidget(codecLabel);
        row1->addStretch();
        vb->addLayout(row1);

        lblAudioCodec_ = new QLabel(shortCodec(audioCodec_), cardContent);
        lblAudioCodec_->setStyleSheet("color: #4fc3f7; font-size: 20px; font-weight: bold; background: transparent; border: none;");
        vb->addWidget(lblAudioCodec_);

        // 채널
        auto* row2 = new QHBoxLayout;
        auto* chLabel = new QLabel("채널", cardContent);
        chLabel->setStyleSheet("color: rgba(255,255,255,0.45); font-size: 10px; background: transparent; border: none;");
        lblChannels_ = new QLabel(channelName(audioChannels_), cardContent);
        lblChannels_->setStyleSheet("color: rgba(255,255,255,0.8); font-size: 12px; background: transparent; border: none;");
        row2->addWidget(chLabel);
        row2->addStretch();
        row2->addWidget(lblChannels_);
        vb->addLayout(row2);

        // 샘플레이트
        auto* row3 = new QHBoxLayout;
        auto* srLabel = new QLabel("샘플레이트", cardContent);
        srLabel->setStyleSheet("color: rgba(255,255,255,0.45); font-size: 10px; background: transparent; border: none;");
        lblSampleRate_ = new QLabel(
            audioSampleRate_ >= 1000
            ? QString("%1 kHz").arg(audioSampleRate_ / 1000.0, 0, 'f', 1)
            : (audioSampleRate_ > 0 ? QString("%1 Hz").arg(audioSampleRate_) : "-"),
            cardContent);
        lblSampleRate_->setStyleSheet("color: rgba(255,255,255,0.8); font-size: 12px; background: transparent; border: none;");
        row3->addWidget(srLabel);
        row3->addStretch();
        row3->addWidget(lblSampleRate_);
        vb->addLayout(row3);

        // 출력 장치
        auto* row4 = new QHBoxLayout;
        auto* outLabel = new QLabel("출력", cardContent);
        outLabel->setStyleSheet("color: rgba(255,255,255,0.45); font-size: 10px; background: transparent; border: none;");
        lblAudioOut_ = new QLabel(audioOutput_.isEmpty() ? "-" : audioOutput_, cardContent);
        lblAudioOut_->setStyleSheet("color: rgba(255,255,255,0.6); font-size: 10px; background: transparent; border: none;");
        lblAudioOut_->setWordWrap(false);
        row4->addWidget(outLabel);
        row4->addStretch();
        row4->addWidget(lblAudioOut_);
        vb->addLayout(row4);

        grid->addWidget(makeCard("오디오", cardContent, "#4fc3f7"), 0, 1);
    }

    // ── 카드 3: 재생 통계 ─────────────────────────────────────────
    {
        auto* cardContent = new QWidget;
        cardContent->setStyleSheet("background: transparent;");
        auto* vb = new QVBoxLayout(cardContent);
        vb->setSpacing(4);
        vb->setContentsMargins(0, 0, 0, 0);

        // 드롭 프레임
        auto* row1 = new QHBoxLayout;
        auto* dropLabel = new QLabel("드롭 프레임", cardContent);
        dropLabel->setStyleSheet("color: rgba(255,255,255,0.45); font-size: 10px; background: transparent; border: none;");
        lblDropped_ = new QLabel("0", cardContent);
        lblDropped_->setStyleSheet("color: #00c8b4; font-size: 16px; font-weight: bold; background: transparent; border: none;");
        row1->addWidget(dropLabel);
        row1->addStretch();
        row1->addWidget(lblDropped_);
        vb->addLayout(row1);

        // A/V 동기화
        auto* row2 = new QHBoxLayout;
        auto* syncLabel = new QLabel("A/V 동기화", cardContent);
        syncLabel->setStyleSheet("color: rgba(255,255,255,0.45); font-size: 10px; background: transparent; border: none;");
        lblAvSync_ = new QLabel("+0 ms", cardContent);
        lblAvSync_->setStyleSheet("color: #00c8b4; font-size: 16px; font-weight: bold; background: transparent; border: none;");
        row2->addWidget(syncLabel);
        row2->addStretch();
        row2->addWidget(lblAvSync_);
        vb->addLayout(row2);

        // 비트레이트
        auto* row3 = new QHBoxLayout;
        auto* brLabel = new QLabel("비트레이트", cardContent);
        brLabel->setStyleSheet("color: rgba(255,255,255,0.45); font-size: 10px; background: transparent; border: none;");
        lblBitrate_ = new QLabel("-", cardContent);
        lblBitrate_->setStyleSheet("color: rgba(255,255,255,0.8); font-size: 12px; background: transparent; border: none;");
        row3->addWidget(brLabel);
        row3->addStretch();
        row3->addWidget(lblBitrate_);
        vb->addLayout(row3);

        // 볼륨
        auto* row4 = new QHBoxLayout;
        auto* volLabel = new QLabel("볼륨", cardContent);
        volLabel->setStyleSheet("color: rgba(255,255,255,0.45); font-size: 10px; background: transparent; border: none;");
        lblVolume_ = new QLabel(
            core_ ? (core_->isMuted() ? "음소거" : QString("%1%").arg(core_->volume())) : "-",
            cardContent);
        lblVolume_->setStyleSheet("color: rgba(255,255,255,0.8); font-size: 12px; background: transparent; border: none;");
        row4->addWidget(volLabel);
        row4->addStretch();
        row4->addWidget(lblVolume_);
        vb->addLayout(row4);

        grid->addWidget(makeCard("통계", cardContent, "#ffd93d"), 1, 0);
    }

    // ── 카드 4: 재생 시간 ─────────────────────────────────────────
    {
        auto* cardContent = new QWidget;
        cardContent->setStyleSheet("background: transparent;");
        auto* vb = new QVBoxLayout(cardContent);
        vb->setSpacing(4);
        vb->setContentsMargins(0, 0, 0, 0);

        // 현재 위치
        auto* row1 = new QHBoxLayout;
        auto* posLabel = new QLabel("현재", cardContent);
        posLabel->setStyleSheet("color: rgba(255,255,255,0.45); font-size: 10px; background: transparent; border: none;");
        lblPosition_ = new QLabel(
            core_ ? formatTime(core_->position()) : "0:00", cardContent);
        lblPosition_->setStyleSheet("color: #fff; font-size: 20px; font-weight: bold; background: transparent; border: none;");
        row1->addWidget(posLabel);
        row1->addStretch();
        row1->addWidget(lblPosition_);
        vb->addLayout(row1);

        // 전체 길이
        auto* row2 = new QHBoxLayout;
        auto* durLabel = new QLabel("전체", cardContent);
        durLabel->setStyleSheet("color: rgba(255,255,255,0.45); font-size: 10px; background: transparent; border: none;");
        lblDuration_ = new QLabel(
            core_ ? formatTime(core_->duration()) : "0:00", cardContent);
        lblDuration_->setStyleSheet("color: rgba(255,255,255,0.6); font-size: 14px; background: transparent; border: none;");
        row2->addWidget(durLabel);
        row2->addStretch();
        row2->addWidget(lblDuration_);
        vb->addLayout(row2);

        // 남은 시간
        auto* row3 = new QHBoxLayout;
        auto* remLabel = new QLabel("남은 시간", cardContent);
        remLabel->setStyleSheet("color: rgba(255,255,255,0.45); font-size: 10px; background: transparent; border: none;");
        // 남은 시간은 refresh()에서 갱신하지 않고 position/duration 레이블로 대체
        // (별도 레이블 추가 시 refresh()에서 계산)
        auto* remVal = new QLabel("-", cardContent);
        remVal->setStyleSheet("color: rgba(255,255,255,0.5); font-size: 11px; background: transparent; border: none;");
        row3->addWidget(remLabel);
        row3->addStretch();
        row3->addWidget(remVal);
        vb->addLayout(row3);

        grid->addWidget(makeCard("재생 시간", cardContent, "#a78bfa"), 1, 1);
    }

    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    mainLayout->addLayout(grid, 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// 카드 위젯 생성
// ─────────────────────────────────────────────────────────────────────────────
QFrame* InfoOverlayWidget::makeCard(const QString& title, QWidget* content,
                                     const QString& accentColor) {
    auto* card = new QFrame(container_);
    card->setStyleSheet(QString(
        "QFrame {"
        "  background: rgba(10, 14, 20, 0.72);"
        "  border: 1px solid rgba(255,255,255,0.07);"
        "  border-radius: 10px;"
        "}"
    ));

    auto* vb = new QVBoxLayout(card);
    vb->setContentsMargins(12, 10, 12, 10);
    vb->setSpacing(6);

    // 카드 제목
    auto* titleRow = new QHBoxLayout;
    titleRow->setSpacing(6);

    // 악센트 바
    auto* accent = new QFrame(card);
    accent->setFixedSize(3, 14);
    accent->setStyleSheet(QString("background: %1; border-radius: 2px; border: none;").arg(accentColor));
    titleRow->addWidget(accent);

    auto* titleLabel = new QLabel(title, card);
    titleLabel->setStyleSheet(
        "color: rgba(255,255,255,0.55);"
        "font-size: 10px;"
        "font-weight: bold;"
        "letter-spacing: 0.5px;"
        "background: transparent;"
        "border: none;");
    titleRow->addWidget(titleLabel);
    titleRow->addStretch();
    vb->addLayout(titleRow);

    // 구분선
    auto* sep = new QFrame(card);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("background: rgba(255,255,255,0.06); border: none; max-height: 1px;");
    vb->addWidget(sep);

    // 내용
    content->setParent(card);
    vb->addWidget(content, 1);

    return card;
}

// ─────────────────────────────────────────────────────────────────────────────
// 이벤트
// ─────────────────────────────────────────────────────────────────────────────
void InfoOverlayWidget::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    if (container_) {
        container_->setGeometry(rect());
    }
}

void InfoOverlayWidget::paintEvent(QPaintEvent*) {
    // 완전 투명 - 카드들이 자체 배경을 그림
}
