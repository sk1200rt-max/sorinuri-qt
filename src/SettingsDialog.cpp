#include "SettingsDialog.h"
#include "ScrobbleManager.h"
#include <QFileDialog>
#include <QTimer>
#include <QDesktopServices>
#include <QDateTime>
#include <QUrl>
#include <QStandardPaths>
#include <QDialogButtonBox>
#include <QScrollArea>
#include <QGuiApplication>
#include <QScreen>
#include <QDebug>

static const QString DIALOG_STYLE = R"(
QDialog {
    background: #1a1a1a;
    color: #e0e0e0;
    font-family: 'Segoe UI', 'Malgun Gothic', sans-serif;
    font-size: 13px;
}
QTabWidget::pane {
    border: 1px solid #2a2a2a;
    background: #1a1a1a;
}
QTabBar::tab {
    background: #141414;
    color: #888;
    padding: 8px 20px;
    border: none;
    border-bottom: 2px solid transparent;
}
QTabBar::tab:selected {
    color: #4fc3f7;
    border-bottom: 2px solid #4fc3f7;
}
QTabBar::tab:hover { color: #ccc; }
QGroupBox {
    border: 1px solid #2a2a2a;
    border-radius: 4px;
    margin-top: 12px;
    padding-top: 8px;
    color: #888;
    font-size: 11px;
}
QGroupBox::title {
    subcontrol-origin: margin;
    left: 8px;
    padding: 0 4px;
}
QComboBox {
    background: #252525;
    border: 1px solid #333;
    border-radius: 3px;
    padding: 4px 8px;
    color: #e0e0e0;
    min-width: 200px;
}
QComboBox::drop-down { border: none; }
QComboBox QAbstractItemView {
    background: #252525;
    color: #e0e0e0;
    selection-background-color: #1a3a5c;
}
QCheckBox { color: #e0e0e0; spacing: 6px; }
QCheckBox::indicator {
    width: 16px; height: 16px;
    border: 1px solid #444;
    border-radius: 3px;
    background: #252525;
}
QCheckBox::indicator:checked {
    background: #4fc3f7;
    border-color: #4fc3f7;
}
QSlider::groove:horizontal {
    height: 4px; background: #333; border-radius: 2px;
}
QSlider::sub-page:horizontal {
    background: #4fc3f7; border-radius: 2px;
}
QSlider::handle:horizontal {
    width: 14px; height: 14px; margin: -5px 0;
    background: #fff; border-radius: 7px;
}
QPushButton {
    background: #252525;
    border: 1px solid #333;
    border-radius: 3px;
    padding: 6px 16px;
    color: #e0e0e0;
}
QPushButton:hover { background: #2a2a2a; }
QPushButton#btnApply, QPushButton#btnOk {
    background: #1a3a5c;
    border-color: #4fc3f7;
    color: #4fc3f7;
}
QPushButton#btnApply:hover, QPushButton#btnOk:hover {
    background: #1e4a6e;
}
QLabel { color: #e0e0e0; }
QLabel.hint { color: #666; font-size: 11px; }
)";

SettingsDialog::SettingsDialog(MpvCore* mpv, QWidget* parent)
    : QDialog(parent), mpv_(mpv), settings_("Sorinuri", "SorinuriPlayer")
{
    setWindowTitle("소리누리 설정");
    setSizeGripEnabled(true);
    setStyleSheet(DIALOG_STYLE);

    // Per-Monitor DPI 환경에서는 현재 화면의 논리 해상도를 기준으로 계산한다.
    // 창 전체가 화면을 점유해 하단 확인·취소 버튼이 가려지는 것을 방지한다.
    const QScreen* screen = parent ? parent->screen() : QGuiApplication::primaryScreen();
    const QRect available = screen ? screen->availableGeometry() : QRect(0, 0, 1280, 720);
    const int margin = 20;
    const QSize availableSize(qMax(1, available.width() - margin * 2),
                              qMax(1, available.height() - margin * 2));
    const QSize preferred(760, 640);
    const QSize minimum(qMin(640, availableSize.width()),
                        qMin(420, availableSize.height()));
    setMinimumSize(minimum);
    setMaximumSize(availableSize);
    resize(qMin(preferred.width(), availableSize.width()),
           qMin(preferred.height(), availableSize.height()));

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    QTabWidget* tabs = new QTabWidget(this);
    tabs->setDocumentMode(true);

    setupAudioTab(tabs);
    setupVideoTab(tabs);
    setupSubtitleTab(tabs);
    setupGeneralTab(tabs);
    setupLastfmTab(tabs);

    // 각 탭의 본문은 독립 스크롤 영역으로 감싼다. 하단 버튼 영역은
    // 스크롤되지 않는 mainLayout의 마지막 항목이라 어떤 DPI에서도 보인다.
    for (int i = 0; i < tabs->count(); ++i) {
        QWidget* page = tabs->widget(i);
        const QString title = tabs->tabText(i);
        const QIcon icon = tabs->tabIcon(i);
        tabs->removeTab(i);
        auto* scroll = new QScrollArea(tabs);
        scroll->setObjectName("settingsTabScroll");
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        scroll->setWidget(page);
        tabs->insertTab(i, scroll, icon, title);
    }

    mainLayout->addWidget(tabs, 1);

    // 버튼 영역
    QWidget* btnWidget = new QWidget(this);
    // 버튼 영역은 콘텐츠 스크롤과 분리한다. 고배율에서 눌림 영역이
    // 줄어들지 않도록 높이와 레이아웃 크기를 고정한다.
    btnWidget->setMinimumHeight(60);
    btnWidget->setMaximumHeight(60);
    btnWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    btnWidget->setStyleSheet(
        "QWidget { background: #141414; border-top: 1px solid #2a2a2a; }"
        // 버튼은 btnWidget 자식으로 설정하여 스타일시트 상속이 정상 적용됨
        "QPushButton { background: #252525; border: 1px solid #333; border-radius: 3px;"
        "  padding: 6px 16px; color: #e0e0e0; min-height: 28px; }"
        "QPushButton:hover { background: #2a2a2a; }"
        "QPushButton#btnApply, QPushButton#btnOk {"
        "  background: #1a3a5c; border-color: #4fc3f7; color: #4fc3f7; }"
        "QPushButton#btnApply:hover, QPushButton#btnOk:hover { background: #1e4a6e; }");
    QHBoxLayout* btnLayout = new QHBoxLayout(btnWidget);
    // 위아래 여백 10px으로 버튼 외경선이 잠히지 않도록
    btnLayout->setContentsMargins(12, 10, 12, 10);
    btnLayout->setSpacing(8);

    // 버튼 parent를 btnWidget으로 설정하여 스타일시트 상속 정상 동작
    QPushButton* btnApply = new QPushButton("적용", btnWidget);
    QPushButton* btnOk    = new QPushButton("확인", btnWidget);
    QPushButton* btnCancel= new QPushButton("취소", btnWidget);
    btnApply->setObjectName("btnApply");
    btnOk->setObjectName("btnOk");

    btnLayout->addStretch();
    btnLayout->addWidget(btnApply);
    btnLayout->addWidget(btnOk);
    btnLayout->addWidget(btnCancel);

    // Esc/창 닫기와 동일하게 취소 동작을 제공하고, 확인 버튼을 기본 동작으로 둔다.
    btnOk->setDefault(true);
    btnCancel->setAutoDefault(false);
    mainLayout->addWidget(btnWidget);

    connect(btnApply,  &QPushButton::clicked, this, &SettingsDialog::onApply);
    connect(btnOk,     &QPushButton::clicked, this, &SettingsDialog::onOk);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    loadSettings();
}

void SettingsDialog::setupAudioTab(QTabWidget* tabs) {
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    // ── 출력 장치 ────────────────────────────────────────────────
    QGroupBox* deviceGroup = new QGroupBox("오디오 출력 장치", page);
    QFormLayout* deviceForm = new QFormLayout(deviceGroup);
    deviceForm->setSpacing(8);

    audioDeviceCombo_ = new QComboBox(page);
    audioDeviceCombo_->addItem("기본 장치 (auto)");

    QPushButton* refreshBtn = new QPushButton("새로고침", page);
    refreshBtn->setFixedWidth(80);
    connect(refreshBtn, &QPushButton::clicked, this, &SettingsDialog::refreshAudioDevices);

    QHBoxLayout* deviceRow = new QHBoxLayout();
    deviceRow->addWidget(audioDeviceCombo_, 1);
    deviceRow->addWidget(refreshBtn);

    deviceForm->addRow("출력 장치:", deviceRow);

    exclusiveModeCheck_ = new QCheckBox("WASAPI Exclusive Mode (독점 모드)", page);
    deviceForm->addRow("", exclusiveModeCheck_);

    // 독점 모드 활성화 시 안내 (노란 배경)
    exclusiveHintLabel_ = new QLabel(page);
    exclusiveHintLabel_->setWordWrap(true);
    exclusiveHintLabel_->setStyleSheet(
        "background:#1a1200;border:1px solid #4a3800;border-radius:6px;"
        "color:#e8c84a;font-size:11px;padding:8px 12px;margin-top:2px;");
    exclusiveHintLabel_->setText(
        "⚠️  독점 모드 활성화 시 다른 앱의 소리가 일시적으로 차단될 수 있습니다.\n"
        "Dolby Atmos / DTS:X 패스스루 및 Bit-Perfect 재생에는 이 모드가 필요합니다.");
    exclusiveHintLabel_->setVisible(false);
    deviceForm->addRow("", exclusiveHintLabel_);

    // 공유 모드 안내 (녹색 배경)
    sharedHintLabel_ = new QLabel(page);
    sharedHintLabel_->setWordWrap(true);
    sharedHintLabel_->setStyleSheet(
        "background:#001a0e;border:1px solid #004d22;border-radius:6px;"
        "color:#4ade80;font-size:11px;padding:8px 12px;margin-top:2px;");
    sharedHintLabel_->setText(
        "✅  공유 모드: 다른 앱과 동시에 소리를 재생할 수 있습니다.\n"
        "5.1 / 7.1 서라운드는 Windows 사운드 설정에서 치널 수를 설정하면 지원됩니다.\n"
        "⚠️  Dolby Atmos / DTS:X 패스스루는 지원되지 않습니다. (독점 모드 필요)");
    sharedHintLabel_->setVisible(true);
    deviceForm->addRow("", sharedHintLabel_);

    // 체크박스 토글 시 안내 전환
    connect(exclusiveModeCheck_, &QCheckBox::toggled, this, [this](bool on) {
        exclusiveHintLabel_->setVisible(on);
        sharedHintLabel_->setVisible(!on);
    });

    layout->addWidget(deviceGroup);

    // ── 패스스루 ─────────────────────────────────────────────────
    QGroupBox* ptGroup = new QGroupBox("오디오 패스스루 (비트스트림 전송)", page);
    QVBoxLayout* ptLayout = new QVBoxLayout(ptGroup);
    ptLayout->setSpacing(6);

    passthroughCheck_ = new QCheckBox("패스스루 활성화 (AV 리시버 디코딩)", page);
    ptLayout->addWidget(passthroughCheck_);

    QLabel* ptHint = new QLabel("활성화 시 아래 선택한 포맷을 디코딩 없이 AV 리시버로 전송합니다.", page);
    ptHint->setStyleSheet("color: #666; font-size: 11px;");
    ptLayout->addWidget(ptHint);

    QWidget* codecWidget = new QWidget(page);
    QHBoxLayout* codecLayout = new QHBoxLayout(codecWidget);
    codecLayout->setContentsMargins(16, 0, 0, 0);
    codecLayout->setSpacing(16);

    ptAC3_    = new QCheckBox("Dolby Digital (AC3)", page);
    ptEAC3_   = new QCheckBox("Dolby Digital+ (E-AC3)", page);
    ptDTS_    = new QCheckBox("DTS Core", page);
    ptDTSHD_  = new QCheckBox("DTS-HD MA", page);
    ptTrueHD_ = new QCheckBox("TrueHD / Atmos", page);

    codecLayout->addWidget(ptAC3_);
    codecLayout->addWidget(ptEAC3_);
    codecLayout->addWidget(ptDTS_);
    codecLayout->addWidget(ptDTSHD_);
    codecLayout->addWidget(ptTrueHD_);
    codecLayout->addStretch();

    ptLayout->addWidget(codecWidget);
    layout->addWidget(ptGroup);

    // ── 볼륨 ─────────────────────────────────────────────────────
    QGroupBox* volGroup = new QGroupBox("볼륨", page);
    QFormLayout* volForm = new QFormLayout(volGroup);

    volumeSlider_ = new QSlider(Qt::Horizontal, page);
    volumeSlider_->setRange(0, 200);
    volumeSlider_->setValue(100);
    volumeLabel_  = new QLabel("100%", page);
    volumeLabel_->setFixedWidth(40);

    connect(volumeSlider_, &QSlider::valueChanged, [this](int v) {
        volumeLabel_->setText(QString("%1%").arg(v));
    });

    QHBoxLayout* volRow = new QHBoxLayout();
    volRow->addWidget(volumeSlider_, 1);
    volRow->addWidget(volumeLabel_);
    volForm->addRow("볼륨:", volRow);

        layout->addWidget(volGroup);

    // ── DSD 네이티브 재생 ──────────────────────────────────────────────────────────
    QGroupBox* dsdGroup = new QGroupBox("DSD 네이티브 재생 (.dsf / .dff)", page);
    QFormLayout* dsdForm = new QFormLayout(dsdGroup);
    dsdForm->setSpacing(8);

    dsdModeCombo_ = new QComboBox(page);
    dsdModeCombo_->addItems({
        "PCM 변환 (호환성 우선)",
        "DoP (DSD over PCM - WASAPI 독점모드 필요)",
        "DSD Direct (ASIO 전용 DAC 필요)"
    });
    dsdForm->addRow("DSD 출력 모드:", dsdModeCombo_);

    QLabel* dsdHint = new QLabel(
        "DoP: WASAPI 독점 모드 + DSD 지원 DAC 필요.\n"
        "DSD Direct: ASIO 드라이버 설치 필요 (전문가용).", page);
    dsdHint->setStyleSheet("color: #666; font-size: 11px;");
    dsdHint->setWordWrap(true);
    dsdForm->addRow("", dsdHint);

    layout->addWidget(dsdGroup);

    // ── 스마트폰 리모컨 ──────────────────────────────────────────────────────────
    QGroupBox* remoteGroup = new QGroupBox("스마트폰 리모컨 (Wi-Fi)", page);
    QFormLayout* remoteForm = new QFormLayout(remoteGroup);
    remoteForm->setSpacing(8);

    remoteEnabledCheck_ = new QCheckBox("리모컨 서버 활성화 (포트 7373)", page);
    remoteForm->addRow("", remoteEnabledCheck_);

    QLabel* remoteHint = new QLabel(
        "활성화 후 같은 Wi-Fi의 스마트폰 브라우저에서\n"
        "http://[PC IP]:7373 접속 → 재생/일시정지/볼륨 제어 가능.", page);
    remoteHint->setStyleSheet("color: #666; font-size: 11px;");
    remoteHint->setWordWrap(true);
    remoteForm->addRow("", remoteHint);

    layout->addWidget(remoteGroup);

    layout->addStretch();
    tabs->addTab(page, "오디오");

    // 기본값 설정: 독점 모드 (true) - 멀티채널 자동 인식을 위해 기본 독점 모드
    exclusiveModeCheck_->setChecked(true);
    passthroughCheck_->setChecked(true);
    ptAC3_->setChecked(true);
    ptEAC3_->setChecked(true);
    ptDTS_->setChecked(true);
    ptDTSHD_->setChecked(true);
    ptTrueHD_->setChecked(true);

    refreshAudioDevices();
}

void SettingsDialog::setupVideoTab(QTabWidget* tabs) {
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    // ── 디코딩 ───────────────────────────────────────────────────
    QGroupBox* decodeGroup = new QGroupBox("비디오 디코딩", page);
    QFormLayout* decodeForm = new QFormLayout(decodeGroup);
    decodeForm->setSpacing(8);

    hwdecCombo_ = new QComboBox(page);
    hwdecCombo_->addItems({"d3d11va (DirectX 11)", "d3d11va-copy", "dxva2", "dxva2-copy",
                            "nvdec (NVIDIA)", "nvdec-copy", "auto", "no (소프트웨어)"});
    decodeForm->addRow("하드웨어 디코딩:", hwdecCombo_);

    voCombo_ = new QComboBox(page);
    voCombo_->addItems({
        "D3D11 (기본, 안정적)",
        "D3D12 / libplacebo (차세대, 4K GPU 부하 20~30% 감소)",
        "OpenGL (호환성)",
        "Vulkan (실험적)"
    });
    decodeForm->addRow("GPU API:", voCombo_);

    // 재시작 필요 항목 안내 (주황색 배너)
    QLabel* restartHint = new QLabel(
        "⚠️  하드웨어 디코딩 및 비디오 출력 변경은 <b>다음 실행 시 적용</b>됩니다.", page);
    restartHint->setStyleSheet(
        "background:#1a0e00; border:1px solid #4a2800; border-radius:4px;"
        "color:#ff9800; font-size:11px; padding:6px 10px;");
    restartHint->setWordWrap(true);
    restartHint->setTextFormat(Qt::RichText);
    decodeForm->addRow("", restartHint);

    // gpu-api 안내 레이블
    QLabel* gpuNextHint = new QLabel(
        "D3D12/libplacebo: 4K 환경에서 GPU 부하 20~30% 감소. NVIDIA RTX/AMD RDNA2+ 권장.\n"
        "변경은 다음 실행 시 적용됩니다.", page);
    gpuNextHint->setStyleSheet("color: #888; font-size: 11px;");
    gpuNextHint->setWordWrap(true);
    decodeForm->addRow("", gpuNextHint);

    layout->addWidget(decodeGroup);

    // ── GPU 렌더링 프로파일 ─────────────────────────────────────────────
    QGroupBox* profileGroup = new QGroupBox("GPU 렌더링 프로파일", page);
    QFormLayout* profileForm = new QFormLayout(profileGroup);
    profileForm->setSpacing(8);

    renderProfileCombo_ = new QComboBox(page);
    renderProfileCombo_->addItems({
        "Eco (절전 - 노트북 통합 GPU)",
        "Balanced (균형 - 중급 GPU)",
        "Quality (화질 - 고급 GPU) [기본값]",
        "HiEnd (최고화질 - 전문가용)"
    });
    renderProfileCombo_->setCurrentIndex(2);  // Quality 기본값
    profileForm->addRow("렌더링 프로파일:", renderProfileCombo_);

    QLabel* profileHint = new QLabel(
        "Eco: bilinear | Balanced: spline36 | Quality: ewa_lanczossharp | HiEnd: ewa_lanczossharp4sharpest", page);
    profileHint->setStyleSheet("color: #666; font-size: 10px;");
    profileHint->setWordWrap(true);
    profileForm->addRow("", profileHint);

    layout->addWidget(profileGroup);

    // ── 화질 ─────────────────────────────────────────────────────────
    QGroupBox* qualityGroup = new QGroupBox("화질 세부 설정 (Advanced)", page);
    QFormLayout* qualityForm = new QFormLayout(qualityGroup);
    qualityForm->setSpacing(8);

    scalingCombo_ = new QComboBox(page);
    scalingCombo_->addItems({"bilinear (빠름)", "bicubic", "lanczos", "spline36",
                              "ewa_lanczos (최고품질)", "ewa_lanczossharp"});
    scalingCombo_->setCurrentIndex(3);
    qualityForm->addRow("업스케일링:", scalingCombo_);

    hdrCombo_ = new QComboBox(page);
    hdrCombo_->addItems({"auto", "bt.2390 (권장)", "hable", "reinhard", "mobius", "clip (끄기)"});
    qualityForm->addRow("HDR 톤매핑:", hdrCombo_);

    debandCheck_ = new QCheckBox("디밴딩 (밴딩 노이즈 제거)", page);
    qualityForm->addRow("", debandCheck_);

    debandStrengthCombo_ = new QComboBox(page);
    debandStrengthCombo_->addItems({"약하게", "보통", "강하게"});
    debandStrengthCombo_->setCurrentIndex(1);
    qualityForm->addRow("디밴딩 강도:", debandStrengthCombo_);

    // 모션 스무딩 (프레임 보간) - 기존 코드에 영향 없는 신규 추가
    motionSmoothingCheck_ = new QCheckBox("모션 스무딩 (프레임 보간, 24fps→매끄러운 재생)", page);
    motionSmoothingCheck_->setChecked(false);
    QLabel* smoothHint = new QLabel("주의: 일부 영상에서 아티팩트가 생길 수 있습니다.", page);
    smoothHint->setStyleSheet("color: #666; font-size: 11px;");
    qualityForm->addRow("", motionSmoothingCheck_);
    qualityForm->addRow("", smoothHint);

    layout->addWidget(qualityGroup);
    layout->addStretch();

    tabs->addTab(page, "비디오");
}

void SettingsDialog::setupSubtitleTab(QTabWidget* tabs) {
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    QGroupBox* subGroup = new QGroupBox("자막 스타일", page);
    QFormLayout* subForm = new QFormLayout(subGroup);
    subForm->setSpacing(8);

    subFontCombo_ = new QComboBox(page);
    subFontCombo_->addItems({"맑은 고딕", "나눔고딕", "Segoe UI", "Arial", "Noto Sans KR"});
    subForm->addRow("폰트:", subFontCombo_);

    subSizeSlider_ = new QSlider(Qt::Horizontal, page);
    subSizeSlider_->setRange(12, 72);
    subSizeSlider_->setValue(36);
    subSizeLabel_ = new QLabel("36", page);
    subSizeLabel_->setFixedWidth(30);
    connect(subSizeSlider_, &QSlider::valueChanged, [this](int v) {
        subSizeLabel_->setText(QString::number(v));
    });

    QHBoxLayout* sizeRow = new QHBoxLayout();
    sizeRow->addWidget(subSizeSlider_, 1);
    sizeRow->addWidget(subSizeLabel_);
    subForm->addRow("크기:", sizeRow);

    subBoldCheck_ = new QCheckBox("굵게", page);
    subForm->addRow("", subBoldCheck_);

    subColorCombo_ = new QComboBox(page);
    subColorCombo_->addItems({"흰색", "노란색", "하늘색", "연두색"});
    subForm->addRow("색상:", subColorCombo_);

    subShadowCheck_ = new QCheckBox("그림자 효과", page);
    subShadowCheck_->setChecked(true);
    subForm->addRow("", subShadowCheck_);

    // ── 자막 실시간 미리보기 ────────────────────────────────────────────
    // 폰트/크기/볼드/색상/그림자 변경 시 즉시 반영되는 미리보기 라벨
    subPreviewLabel_ = new QLabel("자막 미리보기 — 소리누리 Sorinuri", page);
    subPreviewLabel_->setAlignment(Qt::AlignCenter);
    subPreviewLabel_->setMinimumHeight(60);
    subPreviewLabel_->setStyleSheet(
        "background:#000; color:#fff; padding:10px 16px;"
        "border:1px solid #2a2a2a; border-radius:4px;"
        "font-family:'맑은 고딕'; font-size:24px;");
    subPreviewLabel_->setWordWrap(true);
    subForm->addRow("미리보기:", subPreviewLabel_);

    // 미리보기 업데이트 람다
    auto updatePreview = [this]() {
        if (!subPreviewLabel_) return;
        // 폰트
        QString fontFamily = subFontCombo_ ? subFontCombo_->currentText() : "맑은 고딕";
        // 크기 (MPV sub-font-size 36 ≈ UI 24px 기준으로 비례 환산)
        int mpvSize = subSizeSlider_ ? subSizeSlider_->value() : 36;
        int previewPx = qBound(12, mpvSize * 2 / 3, 48);
        // 볼드
        bool bold = subBoldCheck_ && subBoldCheck_->isChecked();
        // 색상
        QString colorStr = "#ffffff";
        if (subColorCombo_) {
            switch (subColorCombo_->currentIndex()) {
            case 1: colorStr = "#ffff00"; break;  // 노란색
            case 2: colorStr = "#87ceeb"; break;  // 하늘색
            case 3: colorStr = "#90ee90"; break;  // 연두색
            default: colorStr = "#ffffff"; break; // 흰색
            }
        }
        // 그림자
        QString shadow = (subShadowCheck_ && subShadowCheck_->isChecked())
            ? "text-shadow: 1px 1px 3px #000000;"
            : "";
        subPreviewLabel_->setStyleSheet(QString(
            "background:#000; color:%1; padding:10px 16px;"
            "border:1px solid #2a2a2a; border-radius:4px;"
            "font-family:'%2'; font-size:%3px; %4 %5")
            .arg(colorStr)
            .arg(fontFamily)
            .arg(previewPx)
            .arg(bold ? "font-weight:bold;" : "font-weight:normal;")
            .arg(shadow));
    };

    // 각 컨트롤 변경 시 미리보기 즉시 업데이트
    connect(subFontCombo_,  QOverload<int>::of(&QComboBox::currentIndexChanged), page, [updatePreview](int){ updatePreview(); });
    connect(subSizeSlider_, &QSlider::valueChanged, page, [updatePreview](int){ updatePreview(); });
    connect(subBoldCheck_,  &QCheckBox::toggled, page, [updatePreview](bool){ updatePreview(); });
    connect(subColorCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), page, [updatePreview](int){ updatePreview(); });
    connect(subShadowCheck_,&QCheckBox::toggled, page, [updatePreview](bool){ updatePreview(); });
    // 초기 미리보기 렌더링
    QTimer::singleShot(0, page, [updatePreview]() { updatePreview(); });

    layout->addWidget(subGroup);

    QGroupBox* autoSubGroup = new QGroupBox("자막 자동 로드", page);
    QFormLayout* autoSubForm = new QFormLayout(autoSubGroup);

    autoLoadSubCheck_ = new QCheckBox("동영상과 같은 폴더에서 자막 자동 로드", page);
    autoLoadSubCheck_->setChecked(true);
    autoSubForm->addRow("", autoLoadSubCheck_);

    layout->addWidget(autoSubGroup);

    // OpenSubtitles API 키 입력 섹션
    QGroupBox* apiGroup = new QGroupBox("OpenSubtitles 자막 자동 다운로드", page);
    QVBoxLayout* apiLay = new QVBoxLayout(apiGroup);
    apiLay->setSpacing(6);

    auto* apiDesc = new QLabel(
        "<a href='https://www.opensubtitles.com/consumers' style='color:#4fc3f7;'>"
        "OpenSubtitles.com</a>에서 API 키를 발급받아 입력하세요."
        "<br><small style='color:#666;'>로그인 → 프로필 → API 섹션 → Consumer Key</small>",
        page);
    apiDesc->setOpenExternalLinks(true);
    apiDesc->setWordWrap(true);
    apiDesc->setStyleSheet("color:#aaa; font-size:11px; background:transparent;");
    apiLay->addWidget(apiDesc);

    auto* apiRow = new QHBoxLayout();
    auto* apiLabel = new QLabel("API Key:", page);
    apiLabel->setFixedWidth(60);
    subApiKeyEdit_ = new QLineEdit(page);
    subApiKeyEdit_->setPlaceholderText("여기에 API 키를 입력하세요...");
    subApiKeyEdit_->setEchoMode(QLineEdit::Password);
    subApiKeyEdit_->setStyleSheet(
        "QLineEdit { background:#252525; color:#ddd; border:1px solid #333;"
        "border-radius:3px; padding:4px 8px; }"
        "QLineEdit:focus { border-color:#4fc3f7; }");
    auto* showKeyBtn = new QPushButton("표시", page);
    showKeyBtn->setFixedWidth(44);
    showKeyBtn->setCheckable(true);
    connect(showKeyBtn, &QPushButton::toggled, [this](bool on) {
        subApiKeyEdit_->setEchoMode(on ? QLineEdit::Normal : QLineEdit::Password);
    });
    apiRow->addWidget(apiLabel);
    apiRow->addWidget(subApiKeyEdit_, 1);
    apiRow->addWidget(showKeyBtn);
    apiLay->addLayout(apiRow);

    layout->addWidget(apiGroup);

    // ── 자동 번역 API 설정 ───────────────────────────────────────────────
    QGroupBox* transGroup = new QGroupBox("자동 번역 API", page);
    QFormLayout* transForm = new QFormLayout(transGroup);
    transForm->setSpacing(8);

    QLabel* transHint = new QLabel(
        "자막 다운로드 후 한국어로 자동 번역 시 사용합니다.\n"
        "DeepL 또는 파파고 중 하나를 입력하세요.", page);
    transHint->setStyleSheet("color: #666; font-size: 11px;");
    transHint->setWordWrap(true);
    transForm->addRow("", transHint);

    deeplApiKeyEdit_ = new QLineEdit(page);
    deeplApiKeyEdit_->setPlaceholderText("DeepL API Key (api-free.deepl.com)");
    deeplApiKeyEdit_->setEchoMode(QLineEdit::Password);
    deeplApiKeyEdit_->setStyleSheet(
        "QLineEdit { background:#1a1a1a; border:1px solid #2a2a2a; border-radius:3px;"
        "  padding:4px 8px; color:#e0e0e0; }"
        "QLineEdit:focus { border-color:#4fc3f7; }");
    transForm->addRow("DeepL API Key:", deeplApiKeyEdit_);

    papagoClientIdEdit_ = new QLineEdit(page);
    papagoClientIdEdit_->setPlaceholderText("파파고 Client ID (developers.naver.com)");
    papagoClientIdEdit_->setStyleSheet(deeplApiKeyEdit_->styleSheet());
    transForm->addRow("파파고 Client ID:", papagoClientIdEdit_);

    papagoClientSecretEdit_ = new QLineEdit(page);
    papagoClientSecretEdit_->setPlaceholderText("파파고 Client Secret");
    papagoClientSecretEdit_->setEchoMode(QLineEdit::Password);
    papagoClientSecretEdit_->setStyleSheet(deeplApiKeyEdit_->styleSheet());
    transForm->addRow("파파고 Secret:", papagoClientSecretEdit_);

    layout->addWidget(transGroup);
    layout->addStretch();

    tabs->addTab(page, "자막");
}

void SettingsDialog::setupGeneralTab(QTabWidget* tabs) {
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    QGroupBox* playGroup = new QGroupBox("재생 설정", page);
    QFormLayout* playForm = new QFormLayout(playGroup);
    playForm->setSpacing(8);

    rememberPosCheck_ = new QCheckBox("재생 위치 기억 (이어보기)", page);
    rememberPosCheck_->setChecked(true);
    playForm->addRow("", rememberPosCheck_);

    resumeCheck_ = new QCheckBox("마지막 재생 파일 자동 재개", page);
    playForm->addRow("", resumeCheck_);

    layout->addWidget(playGroup);

    QGroupBox* langGroup = new QGroupBox("언어 설정", page);
    QFormLayout* langForm = new QFormLayout(langGroup);

    langCombo_ = new QComboBox(page);
    langCombo_->addItems({"한국어", "English", "日本語", "中文"});
    langForm->addRow("인터페이스 언어:", langCombo_);

    QLabel* langHint = new QLabel("* 언어 변경은 재시작 후 적용됩니다.", page);
    langHint->setStyleSheet("color: #666; font-size: 11px;");
    langForm->addRow("", langHint);

        layout->addWidget(langGroup);

    // ── 스크린샷 설정 ──────────────────────────────────────────────────────────
    QGroupBox* shotGroup = new QGroupBox("스크린샷 설정", page);
    QFormLayout* shotForm = new QFormLayout(shotGroup);
    shotForm->setSpacing(8);

    QHBoxLayout* dirLay = new QHBoxLayout();
    screenshotDirEdit_ = new QLineEdit(page);
    screenshotDirEdit_->setReadOnly(true);
    screenshotDirEdit_->setStyleSheet(
        "QLineEdit { background:#1a1a1a; border:1px solid #2a2a2a; border-radius:3px;"
        "  padding:4px 8px; color:#e0e0e0; }");
    QPushButton* btnBrowseDir = new QPushButton("찾아보기...", page);
    connect(btnBrowseDir, &QPushButton::clicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, "스크린샷 저장 폴더 선택",
            screenshotDirEdit_->text().isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) : screenshotDirEdit_->text());
        if (!dir.isEmpty()) screenshotDirEdit_->setText(dir);
    });
    dirLay->addWidget(screenshotDirEdit_, 1);
    dirLay->addWidget(btnBrowseDir);
    shotForm->addRow("저장 폴더:", dirLay);

    screenshotFmtCombo_ = new QComboBox(page);
    screenshotFmtCombo_->addItems({"PNG", "JPG", "WEBP"});
    shotForm->addRow("파일 형식:", screenshotFmtCombo_);

        layout->addWidget(shotGroup);

    // ── SORINURI ORIGINALS API ─────────────────────────────────────────────────────────
    QGroupBox* origGroup = new QGroupBox("SORINURI ORIGINALS", page);
    QFormLayout* origForm = new QFormLayout(origGroup);
    origForm->setSpacing(8);

    originalsApiUrlEdit_ = new QLineEdit(page);
    originalsApiUrlEdit_->setPlaceholderText("https://sorinuri.com/api/songs.json");
    originalsApiUrlEdit_->setStyleSheet(
        "QLineEdit { background:#1a1a1a; border:1px solid #2a2a2a; border-radius:3px;"
        "  padding:4px 8px; color:#e0e0e0; }"
        "QLineEdit:focus { border-color: #00D4B4; }");
    origForm->addRow("API URL:", originalsApiUrlEdit_);

    QLabel* origHint = new QLabel(
        "기본값: https://sorinuri.com/api/songs.json\n"
        "변경 시 적용 버튼을 누르면 즉시 갱신됩니다.", page);
    origHint->setStyleSheet("color: #666; font-size: 11px;");
    origHint->setWordWrap(true);
    origForm->addRow("", origHint);

    layout->addWidget(origGroup);

    layout->addStretch();
    tabs->addTab(page, "일반");
}

void SettingsDialog::refreshAudioDevices() {
    if (!audioDeviceCombo_) return;
    audioDeviceCombo_->clear();
    audioDeviceCombo_->addItem("기본 장치 (auto)", "auto");

    if (!mpv_) return;

    // MPV audio-device-list 실제 파싱 (MPV_FORMAT_NODE → QVariantList)
    const QVariantList devices = mpv_->audioDeviceList();
    for (const QVariant& item : devices) {
        const QVariantMap m = item.toMap();
        const QString name = m.value("name").toString();
        const QString desc = m.value("description").toString();
        if (name.isEmpty() || name == "auto") continue;
        if (!name.startsWith("wasapi")) continue;  // WASAPI 장치만 (Exclusive/패스스루 대상)
        audioDeviceCombo_->addItem(
            QString("WASAPI: %1").arg(desc.isEmpty() ? name : desc), name);
    }
    // 저장된 장치 선택 복원
    const QString saved = settings_.value("audio/device", "auto").toString();
    int idx = audioDeviceCombo_->findData(saved);
    if (idx >= 0) audioDeviceCombo_->setCurrentIndex(idx);
}

void SettingsDialog::loadSettings() {
    // 기본값: 노트북=false(공유 모드), 데스크톱=true(독점 모드)
    // mpv_에서 isLaptop()을 읽어 적절한 기본값 설정
    {
        const bool defaultExclusive = mpv_ ? !mpv_->isLaptop() : true;
        exclusiveModeCheck_->setChecked(settings_.value("audio/exclusive", defaultExclusive).toBool());
    }
    passthroughCheck_->setChecked(settings_.value("audio/passthrough", true).toBool());
    ptAC3_->setChecked(settings_.value("audio/pt_ac3", true).toBool());
    ptEAC3_->setChecked(settings_.value("audio/pt_eac3", true).toBool());
    ptDTS_->setChecked(settings_.value("audio/pt_dts", true).toBool());
    ptDTSHD_->setChecked(settings_.value("audio/pt_dtshd", true).toBool());
    ptTrueHD_->setChecked(settings_.value("audio/pt_truehd", true).toBool());
    volumeSlider_->setValue(settings_.value("audio/volume", 100).toInt());

    // GPU API 로드 (다음 실행 시 적용)
    if (voCombo_) {
        static const QStringList gpuApis = {"d3d11", "d3d12", "opengl", "vulkan"};
        QString savedApi = settings_.value("video/gpu_api", "d3d11").toString();
        int voIdx = gpuApis.indexOf(savedApi);
        if (voIdx < 0) voIdx = settings_.value("video/vo_index", 0).toInt();
        if (voIdx >= 0 && voIdx < voCombo_->count())
            voCombo_->setCurrentIndex(voIdx);
    }
    int hwdecIdx = hwdecCombo_->findText(settings_.value("video/hwdec", "d3d11va (DirectX 11)").toString());
    if (hwdecIdx >= 0) hwdecCombo_->setCurrentIndex(hwdecIdx);

    int scalingIdx = scalingCombo_->findText(settings_.value("video/scaling", "spline36").toString());
    if (scalingIdx >= 0) scalingCombo_->setCurrentIndex(scalingIdx);

    debandCheck_->setChecked(settings_.value("video/deband", false).toBool());
    if (motionSmoothingCheck_)
        motionSmoothingCheck_->setChecked(settings_.value("video/motion_smoothing", false).toBool());
    // GPU 렌더링 프로파일 로드 (0=Eco, 1=Balanced, 2=Quality, 3=HiEnd)
    if (renderProfileCombo_)
        renderProfileCombo_->setCurrentIndex(settings_.value("video/render_profile", 2).toInt());
    rememberPosCheck_->setChecked(settings_.value("general/remember_pos", true).toBool());
    resumeCheck_->setChecked(settings_.value("general/resume", false).toBool());
    // 자막 스타일 복원 (폰트/크기/볼드/색상/그림자)
    if (subFontCombo_) {
        int fi = subFontCombo_->findText(settings_.value("subtitle/font", "맑은 고딕").toString());
        if (fi >= 0) subFontCombo_->setCurrentIndex(fi);
    }
    if (subSizeSlider_) subSizeSlider_->setValue(settings_.value("subtitle/size", 36).toInt());
    if (subBoldCheck_)  subBoldCheck_->setChecked(settings_.value("subtitle/bold", false).toBool());
    if (subColorCombo_) subColorCombo_->setCurrentIndex(settings_.value("subtitle/color", 0).toInt());
    if (subShadowCheck_)subShadowCheck_->setChecked(settings_.value("subtitle/shadow", true).toBool());
    autoLoadSubCheck_->setChecked(settings_.value("subtitle/auto_load", true).toBool());
    if (subApiKeyEdit_)
        subApiKeyEdit_->setText(settings_.value("subtitle/opensubtitles_apikey").toString());
    if (deeplApiKeyEdit_)
        deeplApiKeyEdit_->setText(settings_.value("subtitle/deepl_api_key").toString());
    if (papagoClientIdEdit_)
        papagoClientIdEdit_->setText(settings_.value("subtitle/papago_client_id").toString());
    if (papagoClientSecretEdit_)
        papagoClientSecretEdit_->setText(settings_.value("subtitle/papago_client_secret").toString());
        
    QString defPicDir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (screenshotDirEdit_) screenshotDirEdit_->setText(settings_.value("general/screenshot_dir", defPicDir).toString());
    if (screenshotFmtCombo_) screenshotFmtCombo_->setCurrentText(settings_.value("general/screenshot_format", "PNG").toString());
    if (dsdModeCombo_) dsdModeCombo_->setCurrentIndex(settings_.value("audio/dsd_mode", 0).toInt());
    if (remoteEnabledCheck_) remoteEnabledCheck_->setChecked(settings_.value("remote/enabled", false).toBool());
    if (originalsApiUrlEdit_) originalsApiUrlEdit_->setText(settings_.value("originals/api_url",
        "https://sorinuri.com/api/songs.json").toString());
}

void SettingsDialog::applyToMpv() {
    if (!mpv_) return;

    // 오디오 설정
    // 출력 장치 적용
    const QString devName = audioDeviceCombo_->currentData().toString();
    mpv_->setAudioDevice(devName.isEmpty() ? QStringLiteral("auto") : devName);
    mpv_->setAudioExclusive(exclusiveModeCheck_->isChecked());

    if (passthroughCheck_->isChecked()) {
        QStringList codecs;
        if (ptAC3_->isChecked())    codecs << "ac3";
        if (ptEAC3_->isChecked())   codecs << "eac3";
        if (ptDTS_->isChecked())    codecs << "dts";
        if (ptDTSHD_->isChecked())  codecs << "dts-hd";
        if (ptTrueHD_->isChecked()) codecs << "truehd";
        mpv_->setSpdifCodecs(codecs);
        mpv_->setAudioPassthrough(true);
    } else {
        mpv_->setAudioPassthrough(false);
    }

    mpv_->setVolume(volumeSlider_->value());

    // 비디오 설정
    QString hwdecText = hwdecCombo_->currentText();
    QString hwdec = hwdecText.contains("d3d11va") ? "d3d11va" :
                    hwdecText.contains("dxva2")   ? "dxva2"   :
                    hwdecText.contains("nvdec")   ? "nvdec"   :
                    hwdecText.contains("auto")    ? "auto"    : "no";
    mpv_->setHwdec(hwdec);

    // 업스케일링
    QString scalingText = scalingCombo_->currentText().split(" ").first();
    mpv_->setProperty("scale", scalingText);
    mpv_->setProperty("dscale", scalingText);

    // HDR
    QString hdrText = hdrCombo_->currentText().split(" ").first();
    if (hdrText != "clip") {
        mpv_->setProperty("tone-mapping", hdrText);
    }

    // 디밴딩
    mpv_->setProperty("deband", debandCheck_->isChecked() ? 1 : 0);
    // 모션 스무딩 (프레임 보간) - 기존 코드에 영향 없는 신규 적용
    if (motionSmoothingCheck_)
        mpv_->setMotionSmoothing(motionSmoothingCheck_->isChecked());

    // GPU 렌더링 프로파일 적용
    if (renderProfileCombo_) {
        int profileIdx = renderProfileCombo_->currentIndex();
        RenderProfile profile = static_cast<RenderProfile>(profileIdx);
        mpv_->applyRenderProfile(profile);
    }

    // NOTE: vo(Video Output) 변경은 libmpv 렌더 컨텍스트와 충돌하여
    // 영상이 별도 창으로 분리되는 문제가 발생함.
    // vo=libmpv는 초기화 시 한 번만 설정하며 이후 절대 변경하지 않음.
    // voCombo_ 선택값은 QSettings에 저장하고 다음 실행 시 initialize()에서 적용됨.

    // 자막
    if (subFontCombo_)  mpv_->setProperty("sub-font", subFontCombo_->currentText());
    if (subSizeSlider_) mpv_->setProperty("sub-font-size", subSizeSlider_->value());
    if (subBoldCheck_)  mpv_->setProperty("sub-bold", subBoldCheck_->isChecked() ? 1 : 0);
    if (subShadowCheck_) mpv_->setProperty("sub-shadow-offset", subShadowCheck_->isChecked() ? 2 : 0);
    // 자막 색상 적용 (MPV sub-color: AARRGGBB 포맷 0xAARRGGBB)
    if (subColorCombo_) {
        static const QStringList colors = {
            "#FFFFFFFF",  // 흰색
            "#FFFFFF00",  // 노란색
            "#FF87CEEB",  // 하늘색
            "#FF90EE90",  // 연두색
        };
        int ci = subColorCombo_->currentIndex();
        if (ci >= 0 && ci < colors.size())
            mpv_->setProperty("sub-color", colors[ci]);
    }
    mpv_->setProperty("sub-auto", autoLoadSubCheck_->isChecked() ? "fuzzy" : "no");
}

void SettingsDialog::onApply() {
    // 설정 저장
    settings_.setValue("audio/device",      audioDeviceCombo_->currentData().toString());
    settings_.setValue("audio/exclusive",   exclusiveModeCheck_->isChecked());
    settings_.setValue("audio/passthrough", passthroughCheck_->isChecked());
    settings_.setValue("audio/pt_ac3",      ptAC3_->isChecked());
    settings_.setValue("audio/pt_eac3",     ptEAC3_->isChecked());
    settings_.setValue("audio/pt_dts",      ptDTS_->isChecked());
    settings_.setValue("audio/pt_dtshd",    ptDTSHD_->isChecked());
    settings_.setValue("audio/pt_truehd",   ptTrueHD_->isChecked());
    settings_.setValue("audio/volume",      volumeSlider_->value());
    // GPU API 저장 (다음 실행 시 initialize()에서 적용)
    if (voCombo_) {
        static const QStringList gpuApis = {"d3d11", "d3d12", "opengl", "vulkan"};
        int idx = voCombo_->currentIndex();
        if (idx >= 0 && idx < gpuApis.size())
            settings_.setValue("video/gpu_api", gpuApis[idx]);
        settings_.setValue("video/vo_index", idx);
    }
    settings_.setValue("video/hwdec",       hwdecCombo_->currentText());
    settings_.setValue("video/scaling",     scalingCombo_->currentText());
    settings_.setValue("video/deband",      debandCheck_->isChecked());
    if (motionSmoothingCheck_)
        settings_.setValue("video/motion_smoothing", motionSmoothingCheck_->isChecked());
    if (renderProfileCombo_)
        settings_.setValue("video/render_profile", renderProfileCombo_->currentIndex());
    settings_.setValue("general/remember_pos", rememberPosCheck_->isChecked());
    settings_.setValue("general/resume",    resumeCheck_->isChecked());
    if (screenshotDirEdit_)
        settings_.setValue("general/screenshot_dir", screenshotDirEdit_->text());
    if (screenshotFmtCombo_)
        settings_.setValue("general/screenshot_format", screenshotFmtCombo_->currentText());
    if (dsdModeCombo_)
        settings_.setValue("audio/dsd_mode", dsdModeCombo_->currentIndex());
    if (remoteEnabledCheck_)
        settings_.setValue("remote/enabled", remoteEnabledCheck_->isChecked());
    if (originalsApiUrlEdit_ && !originalsApiUrlEdit_->text().isEmpty())
        settings_.setValue("originals/api_url", originalsApiUrlEdit_->text().trimmed());
    // 자막 스타일 저장 (폰트/크기/볼드/색상/그림자)
    if (subFontCombo_)  settings_.setValue("subtitle/font",   subFontCombo_->currentText());
    if (subSizeSlider_) settings_.setValue("subtitle/size",   subSizeSlider_->value());
    if (subBoldCheck_)  settings_.setValue("subtitle/bold",   subBoldCheck_->isChecked());
    if (subColorCombo_) settings_.setValue("subtitle/color",  subColorCombo_->currentIndex());
    if (subShadowCheck_)settings_.setValue("subtitle/shadow", subShadowCheck_->isChecked());
    settings_.setValue("subtitle/auto_load",autoLoadSubCheck_->isChecked());
    if (subApiKeyEdit_)
        settings_.setValue("subtitle/opensubtitles_apikey", subApiKeyEdit_->text().trimmed());
    if (deeplApiKeyEdit_)
        settings_.setValue("subtitle/deepl_api_key", deeplApiKeyEdit_->text().trimmed());
    if (papagoClientIdEdit_)
        settings_.setValue("subtitle/papago_client_id", papagoClientIdEdit_->text().trimmed());
    if (papagoClientSecretEdit_)
        settings_.setValue("subtitle/papago_client_secret", papagoClientSecretEdit_->text().trimmed());

    applyToMpv();
    emit settingsApplied();
}

void SettingsDialog::onOk() {
    onApply();
    accept();
}

// ─── Last.fm 설정 탭 (v6.18.0 신규) ─────────────────────────────────────────
void SettingsDialog::setupLastfmTab(QTabWidget* tabs) {
    auto* page   = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    // 헤더
    auto* lblHdr = new QLabel("📻  Last.fm 스크로블링");
    lblHdr->setStyleSheet("font-size: 13px; font-weight: bold; color: #e0e0e0;");
    layout->addWidget(lblHdr);

    auto* lblDesc = new QLabel(
        "재생한 음악을 Last.fm에 자동으로 기록합니다.\n"
        "재생 시간이 50% 이상이거나 4분 이상이면 스크로블됩니다.");
    lblDesc->setStyleSheet("color: #888; font-size: 11px;");
    lblDesc->setWordWrap(true);
    layout->addWidget(lblDesc);

    // 활성화 토글
    lastfmEnabledCheck_ = new QCheckBox("Last.fm 스크로블링 활성화");
    lastfmEnabledCheck_->setChecked(settings_.value("lastfm/enabled", false).toBool());
    layout->addWidget(lastfmEnabledCheck_);

    // 연결 상태 그룹
    auto* grpAuth = new QGroupBox("계정 연결");
    auto* authLayout = new QVBoxLayout(grpAuth);

    lastfmStatusLabel_ = new QLabel("연결되지 않음");
    lastfmStatusLabel_->setStyleSheet("color: #888; font-size: 11px;");
    authLayout->addWidget(lastfmStatusLabel_);

    auto* rowBtns = new QHBoxLayout;
    btnLastfmAuth_ = new QPushButton("🔗  Last.fm 계정 연결");
    btnLastfmAuth_->setStyleSheet(
        "QPushButton { background: #c3000d; color: #fff; border: none;"
        "  border-radius: 4px; padding: 7px 16px; font-size: 12px; }"
        "QPushButton:hover { background: #e0001a; }");
    btnLastfmLogout_ = new QPushButton("로그아웃");
    btnLastfmLogout_->setEnabled(false);
    rowBtns->addWidget(btnLastfmAuth_);
    rowBtns->addWidget(btnLastfmLogout_);
    rowBtns->addStretch();
    authLayout->addLayout(rowBtns);

    auto* lblHelp = new QLabel(
        "연결 버튼을 누르면 브라우저에서 Last.fm 인증 페이지가 열립니다.\n"
        "로그인 후 소리누리로 돌아오면 자동으로 연결됩니다.");
    lblHelp->setStyleSheet("color: #555; font-size: 10px;");
    lblHelp->setWordWrap(true);
    authLayout->addWidget(lblHelp);
    layout->addWidget(grpAuth);

    // API 키 설정 (고급)
    auto* grpApi = new QGroupBox("고급 설정 (선택 사항)");
    auto* apiLayout = new QFormLayout(grpApi);
    lastfmApiKeyEdit_ = new QLineEdit;
    lastfmApiKeyEdit_->setPlaceholderText("기본값 사용 (비워두면 소리누리 앱 키 사용)");
    lastfmApiKeyEdit_->setText(settings_.value("lastfm/api_key").toString());
    apiLayout->addRow("API 키:", lastfmApiKeyEdit_);
    layout->addWidget(grpApi);

    // 최근 스크로블 이력
    layout->addWidget(new QLabel("최근 스크로블 이력:"));
    lastfmHistoryList_ = new QListWidget;
    lastfmHistoryList_->setMaximumHeight(150);
    lastfmHistoryList_->setStyleSheet(
        "QListWidget { background: #0d0d0d; border: 1px solid #1e1e1e; border-radius: 4px; }"
        "QListWidget::item { padding: 4px 8px; border-bottom: 1px solid #1a1a1a; font-size: 11px; }");
    layout->addWidget(lastfmHistoryList_);
    layout->addStretch();

    // 시그널 연결
    connect(btnLastfmAuth_,   &QPushButton::clicked, this, &SettingsDialog::onLastfmAuth);
    connect(btnLastfmLogout_, &QPushButton::clicked, this, &SettingsDialog::onLastfmLogout);

    if (scrobbleMgr_) {
        connect(scrobbleMgr_, &ScrobbleManager::scrobbled,
                this, [this](const QString& title, const QString& artist) {
            if (lastfmHistoryList_) {
                QString timeStr = QDateTime::currentDateTime().toString("hh:mm");
                lastfmHistoryList_->insertItem(0,
                    QString("[%1] %2 — %3").arg(timeStr, title, artist));
                if (lastfmHistoryList_->count() > 30)
                    lastfmHistoryList_->takeItem(lastfmHistoryList_->count()-1);
            }
        });
    }

    updateLastfmStatus();
    tabs->addTab(page, "📻 Last.fm");
}

void SettingsDialog::onLastfmAuth() {
    if (!scrobbleMgr_) return;
    QDesktopServices::openUrl(QUrl(scrobbleMgr_->authUrl()));
    if (lastfmStatusLabel_) {
        lastfmStatusLabel_->setText("브라우저에서 Last.fm 로그인 후 돌아오세요...");
        lastfmStatusLabel_->setStyleSheet("color: #f0c040; font-size: 11px;");
    }
}

void SettingsDialog::onLastfmLogout() {
    if (!scrobbleMgr_) return;
    scrobbleMgr_->setSessionKey({});
    QSettings s("GaonCommunication", "Sorinuri");
    s.remove("lastfm/session_key");
    updateLastfmStatus();
}

void SettingsDialog::updateLastfmStatus() {
    if (!scrobbleMgr_) return;
    bool connected = !scrobbleMgr_->sessionKey().isEmpty();
    if (lastfmStatusLabel_) {
        lastfmStatusLabel_->setText(connected ? "✅ Last.fm 계정 연결됨" : "연결되지 않음");
        lastfmStatusLabel_->setStyleSheet(
            connected ? "color: #00D4B4; font-size: 11px;" : "color: #888; font-size: 11px;");
    }
    if (btnLastfmAuth_)   btnLastfmAuth_->setText(connected ? "재인증" : "🔗  Last.fm 계정 연결");
    if (btnLastfmLogout_) btnLastfmLogout_->setEnabled(connected);
}
