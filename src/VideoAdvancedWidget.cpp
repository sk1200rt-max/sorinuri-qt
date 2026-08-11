#include "VideoAdvancedWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QTimer>

static const QString STYLE =
    "QWidget { background: #111; color: #ddd; }"
    "QGroupBox { border:1px solid #2a2a2a; border-radius:4px; margin-top:10px;"
    "  color:#888; font-size:11px; padding-top:6px; }"
    "QGroupBox::title { subcontrol-origin:margin; left:8px; padding:0 4px; }"
    "QCheckBox { color:#ddd; spacing:6px; }"
    "QCheckBox::indicator { width:14px; height:14px; border:1px solid #444;"
    "  border-radius:3px; background:#1e1e1e; }"
    "QCheckBox::indicator:checked { background:#4fc3f7; border-color:#4fc3f7; }"
    "QPushButton { background:#1e1e1e; color:#ccc; border:1px solid #2a2a2a;"
    "  border-radius:3px; padding:5px 12px; font-size:11px; }"
    "QPushButton:hover { background:#2a2a2a; border-color:#4fc3f7; }"
    "QComboBox { background:#1e1e1e; color:#ddd; border:1px solid #2a2a2a;"
    "  border-radius:3px; padding:4px 8px; }"
    "QComboBox::drop-down { border:none; }"
    "QComboBox QAbstractItemView { background:#1e1e1e; color:#ddd;"
    "  selection-background-color:#1565c0; }";

VideoAdvancedWidget::VideoAdvancedWidget(MpvCore* core, QWidget* parent)
    : QWidget(parent)
    , core_(core)
    , settings_("Sorinuri", "SorinuriPlayer")
{
    setStyleSheet(STYLE);
    setupUI();
    loadSettings();
}

void VideoAdvancedWidget::setupUI()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    // 설명
    auto* descLabel = new QLabel(
        "디스플레이 캘리브레이션 장비(i1Display, Spyder 등)로 생성한 3D LUT 파일(.cube)을\n"
        "로드하여 전문가 수준의 정확한 색상 재현을 구현합니다.",
        this);
    descLabel->setStyleSheet("color:#888; font-size:11px; background:transparent;");
    descLabel->setWordWrap(true);
    layout->addWidget(descLabel);

    // 3D LUT 파일 선택
    auto* lutGroup = new QGroupBox("3D LUT 디스플레이 캘리브레이션", this);
    auto* lutLay = new QVBoxLayout(lutGroup);
    lutLay->setSpacing(6);

    auto* fileRow = new QHBoxLayout();
    lutPathLabel_ = new QLabel("LUT 파일을 선택하세요...", this);
    lutPathLabel_->setStyleSheet(
        "color:#666; font-size:11px; background:#1a1a1a; border:1px solid #2a2a2a;"
        "border-radius:3px; padding:4px 8px;");
    browseLutBtn_ = new QPushButton("찾아보기...", this);
    browseLutBtn_->setFixedWidth(80);
    connect(browseLutBtn_, &QPushButton::clicked, this, &VideoAdvancedWidget::onBrowseLut);
    fileRow->addWidget(lutPathLabel_, 1);
    fileRow->addWidget(browseLutBtn_);
    lutLay->addLayout(fileRow);

    lutEnableCheck_ = new QCheckBox("3D LUT 캘리브레이션 적용", this);
    lutEnableCheck_->setEnabled(false);
    connect(lutEnableCheck_, &QCheckBox::toggled,
            this, &VideoAdvancedWidget::onLutToggled);
    lutLay->addWidget(lutEnableCheck_);

    layout->addWidget(lutGroup);

    // 색공간 프리셋
    auto* csGroup = new QGroupBox("색공간 프리셋", this);
    auto* csLay = new QHBoxLayout(csGroup);
    csLay->setSpacing(8);

    auto* csLabel = new QLabel("타겟 색공간:", this);
    csLabel->setFixedWidth(80);
    colorspaceCombo_ = new QComboBox(this);
    colorspaceCombo_->addItems({
        "자동 (권장)",
        "BT.709 SDR (일반 모니터)",
        "BT.2020 HDR (HDR 모니터)",
        "DCI-P3 (영화관/전문가)",
        "sRGB (웹/일반)"
    });
    connect(colorspaceCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &VideoAdvancedWidget::onColorspaceChanged);
    csLay->addWidget(csLabel);
    csLay->addWidget(colorspaceCombo_, 1);
    layout->addWidget(csGroup);

    // 상태
    statusLabel_ = new QLabel("3D LUT 비활성", this);
    statusLabel_->setStyleSheet("color:#666; font-size:11px; background:transparent;");
    layout->addWidget(statusLabel_);
    layout->addStretch();
}

void VideoAdvancedWidget::onBrowseLut()
{
    QString path = QFileDialog::getOpenFileName(
        this, "3D LUT 파일 선택", {},
        "3D LUT 파일 (*.cube *.3dl *.lut);;모든 파일 (*.*)");
    if (path.isEmpty()) return;

    activeLutPath_ = path;
    lutPathLabel_->setText(QFileInfo(path).fileName());
    lutPathLabel_->setStyleSheet(
        "color:#ddd; font-size:11px; background:#1a1a1a; border:1px solid #2a2a2a;"
        "border-radius:3px; padding:4px 8px;");
    lutPathLabel_->setToolTip(path);
    lutEnableCheck_->setEnabled(true);
    saveSettings();
}

void VideoAdvancedWidget::onLutToggled(bool on)
{
    if (on) applyLut();
    else    disableLut();
}

void VideoAdvancedWidget::onColorspaceChanged(int idx)
{
    if (!core_) return;
    // 색공간 프리셋 적용 (기존 tone-mapping 설정과 독립적)
    switch (idx) {
    case 0: // 자동
        core_->setProperty("target-colorspace-hint", true);
        break;
    case 1: // BT.709 SDR
        core_->setProperty("target-prim", QString("bt.709"));
        core_->setProperty("target-trc",  QString("bt.1886"));
        break;
    case 2: // BT.2020 HDR
        core_->setProperty("target-prim", QString("bt.2020"));
        core_->setProperty("target-trc",  QString("pq"));
        break;
    case 3: // DCI-P3
        core_->setProperty("target-prim", QString("dci-p3"));
        core_->setProperty("target-trc",  QString("gamma2.6"));
        break;
    case 4: // sRGB
        core_->setProperty("target-prim", QString("bt.709"));
        core_->setProperty("target-trc",  QString("srgb"));
        break;
    }
    saveSettings();
}

void VideoAdvancedWidget::applyLut()
{
    if (!core_ || activeLutPath_.isEmpty()) {
        statusLabel_->setText("오류: LUT 파일이 선택되지 않았습니다.");
        if (lutEnableCheck_) lutEnableCheck_->setChecked(false);
        return;
    }

    // MPV lut3d 옵션으로 3D LUT 적용
    QString lutPath = activeLutPath_;
    lutPath.replace("\\", "/");
    core_->setProperty("lut3d", lutPath);

    statusLabel_->setText("3D LUT 적용됨: " + QFileInfo(activeLutPath_).fileName());
    statusLabel_->setStyleSheet("color:#4caf50; font-size:11px; background:transparent;");
    emit lutChanged(true, activeLutPath_);
    saveSettings();
}

void VideoAdvancedWidget::disableLut()
{
    if (!core_) return;
    core_->setProperty("lut3d", QString(""));
    statusLabel_->setText("3D LUT 비활성화됨");
    statusLabel_->setStyleSheet("color:#666; font-size:11px; background:transparent;");
    emit lutChanged(false, QString());
}

bool VideoAdvancedWidget::isLutEnabled() const
{
    return lutEnableCheck_ && lutEnableCheck_->isChecked();
}

void VideoAdvancedWidget::loadSettings()
{
    activeLutPath_ = settings_.value("video_advanced/lut_path").toString();
    if (!activeLutPath_.isEmpty() && QFileInfo::exists(activeLutPath_)) {
        lutPathLabel_->setText(QFileInfo(activeLutPath_).fileName());
        lutPathLabel_->setToolTip(activeLutPath_);
        lutPathLabel_->setStyleSheet(
            "color:#ddd; font-size:11px; background:#1a1a1a; border:1px solid #2a2a2a;"
            "border-radius:3px; padding:4px 8px;");
        lutEnableCheck_->setEnabled(true);
    }

    int csIdx = settings_.value("video_advanced/colorspace_idx", 0).toInt();
    if (colorspaceCombo_ && csIdx >= 0 && csIdx < colorspaceCombo_->count()) {
        // blockSignals: 생성자에서 setCurrentIndex 시 currentIndexChanged 시그널 차단
        // onColorspaceChanged가 호출되면 target-colorspace-hint 등 MPV 속성이 변경되어
        // vo=libmpv 렌더러가 재초기화되면서 영상 창 분리 발생
        colorspaceCombo_->blockSignals(true);
        colorspaceCombo_->setCurrentIndex(csIdx);
        colorspaceCombo_->blockSignals(false);
    }

    bool wasEnabled = settings_.value("video_advanced/lut_enabled", false).toBool();
    if (wasEnabled && !activeLutPath_.isEmpty() && QFileInfo::exists(activeLutPath_)) {
        lutEnableCheck_->setChecked(true);
        QTimer::singleShot(1000, this, [this]() {
            if (lutEnableCheck_ && lutEnableCheck_->isChecked())
                applyLut();
        });
    }
}

void VideoAdvancedWidget::saveSettings()
{
    settings_.setValue("video_advanced/lut_path", activeLutPath_);
    settings_.setValue("video_advanced/lut_enabled",
        lutEnableCheck_ ? lutEnableCheck_->isChecked() : false);
    settings_.setValue("video_advanced/colorspace_idx",
        colorspaceCombo_ ? colorspaceCombo_->currentIndex() : 0);
}
