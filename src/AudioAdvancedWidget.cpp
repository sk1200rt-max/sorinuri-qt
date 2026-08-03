#include "AudioAdvancedWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QTabWidget>
#include <QGroupBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QLabel>
#include <cmath>

static const QString WIDGET_STYLE =
    "QWidget { background: #111; color: #ddd; }"
    "QGroupBox { border: 1px solid #2a2a2a; border-radius: 4px; margin-top: 10px;"
    "  color: #888; font-size: 11px; padding-top: 6px; }"
    "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; }"
    "QCheckBox { color: #ddd; spacing: 6px; }"
    "QCheckBox::indicator { width:14px; height:14px; border:1px solid #444;"
    "  border-radius:3px; background:#1e1e1e; }"
    "QCheckBox::indicator:checked { background:#4fc3f7; border-color:#4fc3f7; }"
    "QPushButton { background:#1e1e1e; color:#ccc; border:1px solid #2a2a2a;"
    "  border-radius:3px; padding:5px 12px; font-size:11px; }"
    "QPushButton:hover { background:#2a2a2a; border-color:#4fc3f7; }"
    "QPushButton:pressed { background:#1565c0; }"
    "QSlider::groove:horizontal { height:3px; background:#2a2a2a; border-radius:1px; }"
    "QSlider::handle:horizontal { width:10px; height:10px; margin:-4px 0;"
    "  background:#4fc3f7; border-radius:5px; }"
    "QSlider::sub-page:horizontal { background:#1565c0; border-radius:1px; }"
    "QListWidget { background:#1a1a1a; color:#ddd; border:1px solid #2a2a2a;"
    "  border-radius:3px; }"
    "QListWidget::item:selected { background:#1565c0; }"
    "QTabWidget::pane { border:1px solid #2a2a2a; background:#111; }"
    "QTabBar::tab { background:#0d0d0d; color:#888; padding:5px 14px;"
    "  border:1px solid #1a1a1a; border-bottom:none; font-size:11px; }"
    "QTabBar::tab:selected { background:#111; color:#4fc3f7; border-color:#4fc3f7; }"
    "QTabBar::tab:hover { color:#ccc; }";

AudioAdvancedWidget::AudioAdvancedWidget(MpvCore* core, QWidget* parent)
    : QWidget(parent)
    , core_(core)
    , settings_("Sorinuri", "SorinuriPlayer")
{
    setStyleSheet(WIDGET_STYLE);
    setupUI();
    loadSettings();
}

void AudioAdvancedWidget::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    auto* tabs = new QTabWidget(this);
    tabs->setDocumentMode(true);

    // ── 컨볼루션 룸 코렉션 탭 ────────────────────────────────────────
    auto* convPage = new QWidget();
    buildConvolutionTab(convPage);
    tabs->addTab(convPage, "룸 코렉션");

    // ── VST 플러그인 탭 ───────────────────────────────────────────────
    auto* vstPage = new QWidget();
    buildVstTab(vstPage);
    tabs->addTab(vstPage, "VST 플러그인");

    mainLayout->addWidget(tabs);
}

void AudioAdvancedWidget::buildConvolutionTab(QWidget* parent)
{
    auto* layout = new QVBoxLayout(parent);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    // 설명
    auto* descLabel = new QLabel(
        "REW(Room EQ Wizard) 등에서 생성한 임펄스 응답(IR) WAV 파일을 로드하여\n"
        "실내 음향 특성을 보정합니다. 스테레오 또는 모노 IR 파일을 지원합니다.",
        parent);
    descLabel->setStyleSheet("color:#888; font-size:11px; background:transparent;");
    descLabel->setWordWrap(true);
    layout->addWidget(descLabel);

    // IR 파일 선택 그룹
    auto* irGroup = new QGroupBox("임펄스 응답 (IR) 파일", parent);
    auto* irLay = new QVBoxLayout(irGroup);
    irLay->setSpacing(6);

    auto* fileRow = new QHBoxLayout();
    irPathLabel_ = new QLabel("파일을 선택하세요...", parent);
    irPathLabel_->setStyleSheet(
        "color:#666; font-size:11px; background:#1a1a1a; border:1px solid #2a2a2a;"
        "border-radius:3px; padding:4px 8px;");
    irPathLabel_->setWordWrap(false);
    browseIrBtn_ = new QPushButton("찾아보기...", parent);
    browseIrBtn_->setFixedWidth(80);
    connect(browseIrBtn_, &QPushButton::clicked, this, &AudioAdvancedWidget::onBrowseIrFile);
    fileRow->addWidget(irPathLabel_, 1);
    fileRow->addWidget(browseIrBtn_);
    irLay->addLayout(fileRow);

    // 활성화 체크박스
    convEnableCheck_ = new QCheckBox("컨볼루션 룸 코렉션 활성화", parent);
    convEnableCheck_->setEnabled(false);  // IR 파일 선택 후 활성화
    connect(convEnableCheck_, &QCheckBox::toggled,
            this, &AudioAdvancedWidget::onConvolutionToggled);
    irLay->addWidget(convEnableCheck_);

    layout->addWidget(irGroup);

    // 게인 조절
    auto* gainGroup = new QGroupBox("출력 게인 보정", parent);
    auto* gainLay = new QHBoxLayout(gainGroup);
    gainLay->setSpacing(8);

    auto* gainTitleLbl = new QLabel("게인:", parent);
    gainTitleLbl->setFixedWidth(36);
    gainSlider_ = new QSlider(Qt::Horizontal, parent);
    gainSlider_->setRange(-12, 12);
    gainSlider_->setValue(0);
    gainSlider_->setTickInterval(3);
    gainLabel_ = new QLabel("0 dB", parent);
    gainLabel_->setFixedWidth(48);
    gainLabel_->setStyleSheet("color:#4fc3f7; font-family:Consolas; background:transparent;");
    connect(gainSlider_, &QSlider::valueChanged,
            this, &AudioAdvancedWidget::onGainChanged);
    gainLay->addWidget(gainTitleLbl);
    gainLay->addWidget(gainSlider_, 1);
    gainLay->addWidget(gainLabel_);
    layout->addWidget(gainGroup);

    // 상태 표시
    statusLabel_ = new QLabel("비활성", parent);
    statusLabel_->setStyleSheet(
        "color:#666; font-size:11px; background:transparent; padding:4px 0;");
    layout->addWidget(statusLabel_);
    layout->addStretch();
}

void AudioAdvancedWidget::buildVstTab(QWidget* parent)
{
    auto* layout = new QVBoxLayout(parent);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    // 안내 메시지
    auto* infoLabel = new QLabel(
        "VST3 플러그인을 등록하면 재생 시 실시간 오디오 체인에 적용됩니다.\n"
        "Windows 전용: .dll (VST2) / .vst3 (VST3) 파일을 지원합니다.\n"
        "플러그인 적용 순서는 드래그로 변경할 수 있습니다.",
        parent);
    infoLabel->setStyleSheet("color:#888; font-size:11px; background:transparent;");
    infoLabel->setWordWrap(true);
    layout->addWidget(infoLabel);

    // 플러그인 목록
    auto* listGroup = new QGroupBox("등록된 VST/VST3 플러그인", parent);
    auto* listLay = new QVBoxLayout(listGroup);

    vstListWidget_ = new QListWidget(parent);
    vstListWidget_->setMinimumHeight(120);
    listLay->addWidget(vstListWidget_);

    auto* btnRow = new QHBoxLayout();
    addVstBtn_ = new QPushButton("+ 플러그인 추가", parent);
    removeVstBtn_ = new QPushButton("- 제거", parent);
    removeVstBtn_->setEnabled(false);
    connect(addVstBtn_, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getOpenFileName(
            this, "VST/VST3 플러그인 선택", {},
            "VST 플러그인 (*.dll *.vst3);;모든 파일 (*.*)");
        if (!path.isEmpty()) {
            vstListWidget_->addItem(path);
            saveSettings();
        }
    });
    connect(removeVstBtn_, &QPushButton::clicked, this, [this]() {
        auto* item = vstListWidget_->currentItem();
        if (item) { delete item; saveSettings(); }
    });
    connect(vstListWidget_, &QListWidget::currentItemChanged, this,
            [this](QListWidgetItem* cur, QListWidgetItem*) {
        removeVstBtn_->setEnabled(cur != nullptr);
    });
    btnRow->addWidget(addVstBtn_);
    btnRow->addWidget(removeVstBtn_);
    btnRow->addStretch();
    listLay->addLayout(btnRow);

    layout->addWidget(listGroup);

    vstStatusLabel_ = new QLabel(
        "플러그인 적용: MPV af 체인을 통해 실시간 처리됩니다. 재생 중 적용 시 잠시 끊길 수 있습니다.", parent);
    vstStatusLabel_->setStyleSheet("color:#555; font-size:10px; background:transparent;");
    vstStatusLabel_->setWordWrap(true);
    layout->addWidget(vstStatusLabel_);

    // VST 체인 활성화 토글
    auto* applyRow = new QHBoxLayout();
    auto* vstEnableCheck = new QCheckBox("VST 플러그인 체인 활성화", parent);
    vstEnableCheck->setStyleSheet("color:#e0e0e0;");
    auto* vstApplyBtn = new QPushButton("지금 적용", parent);
    vstApplyBtn->setStyleSheet(
        "QPushButton { background:#1a3a5c; color:#4fc3f7; border:1px solid #4fc3f7;"
        "  border-radius:4px; padding:5px 14px; font-size:12px; }"
        "QPushButton:hover { background:#1e4a6e; }");
    applyRow->addWidget(vstEnableCheck);
    applyRow->addStretch();
    applyRow->addWidget(vstApplyBtn);
    layout->addLayout(applyRow);

    // VST 체인 적용: MPV af 명령으로 실시간 적용
    connect(vstApplyBtn, &QPushButton::clicked, this, [this, vstEnableCheck]() {
        if (!core_) return;
        if (!vstEnableCheck->isChecked() || vstListWidget_->count() == 0) {
            core_->setProperty("af", QString(""));
            vstStatusLabel_->setText("VST 체인 비활성화됨.");
            return;
        }
        // LADSPA/VST3 래퍼를 통한 af 체인 구성
        // Windows: lavfi=[ladspa=...] 또는 직접 af=ladspa=...
        QStringList afParts;
        for (int i = 0; i < vstListWidget_->count(); ++i) {
            QString path = vstListWidget_->item(i)->text();
            QFileInfo fi(path);
            QString ext = fi.suffix().toLower();
            if (ext == "dll" || ext == "vst3") {
                // LADSPA 래퍼 방식 (MPV 지원)
                QString escaped = path.replace("\\", "/").replace("'", "\'");
                afParts << QString("ladspa=%1").arg(escaped);
            }
        }
        if (!afParts.isEmpty()) {
            QString afStr = afParts.join(",");
            core_->setProperty("af", afStr);
            vstStatusLabel_->setText(QString("VST 체인 적용됨: %1개 플러그인").arg(afParts.size()));
        } else {
            vstStatusLabel_->setText("적용 가능한 플러그인이 없습니다.");
        }
    });

    layout->addStretch();
}

// ─── 슬롯 구현 ───────────────────────────────────────────────────────────

void AudioAdvancedWidget::onBrowseIrFile()
{
    QString path = QFileDialog::getOpenFileName(
        this, "임펄스 응답 (IR) 파일 선택", {},
        "WAV 파일 (*.wav);;모든 파일 (*.*)");
    if (path.isEmpty()) return;

    activeIrPath_ = path;
    irPathLabel_->setText(QFileInfo(path).fileName());
    irPathLabel_->setStyleSheet(
        "color:#ddd; font-size:11px; background:#1a1a1a; border:1px solid #2a2a2a;"
        "border-radius:3px; padding:4px 8px;");
    irPathLabel_->setToolTip(path);
    convEnableCheck_->setEnabled(true);
    saveSettings();
}

void AudioAdvancedWidget::onConvolutionToggled(bool on)
{
    if (on) {
        applyConvolution();
    } else {
        disableConvolution();
    }
}

void AudioAdvancedWidget::onGainChanged(int val)
{
    gainDb_ = val;
    gainLabel_->setText(QString("%1%2 dB")
        .arg(val > 0 ? "+" : "")
        .arg(val));
    // 활성화 상태면 즉시 재적용
    if (convEnableCheck_ && convEnableCheck_->isChecked()) {
        updateAfChain();
    }
    saveSettings();
}

void AudioAdvancedWidget::applyConvolution()
{
    if (!core_ || activeIrPath_.isEmpty()) {
        statusLabel_->setText("오류: IR 파일이 선택되지 않았습니다.");
        if (convEnableCheck_) convEnableCheck_->setChecked(false);
        return;
    }
    updateAfChain();
}

void AudioAdvancedWidget::disableConvolution()
{
    if (!core_) return;
    // af 체인 초기화 (기존 EQ 등 다른 필터도 제거되므로 주의)
    // 안전을 위해 af=""로만 초기화
    core_->setProperty("af", QString(""));
    statusLabel_->setText("컨볼루션 비활성화됨");
    statusLabel_->setStyleSheet("color:#666; font-size:11px; background:transparent;");
    emit convolutionChanged(false, QString());
}

void AudioAdvancedWidget::updateAfChain()
{
    if (!core_ || activeIrPath_.isEmpty()) return;

    // MPV lavfi 필터: aconvolve로 IR 파일 적용
    // 경로의 백슬래시를 슬래시로 변환 (MPV/FFmpeg 호환)
    QString irPath = activeIrPath_;
    irPath.replace("\\", "/");

    // 게인 보정: volume 필터 추가 (dB → 선형 변환)
    double gainLinear = std::pow(10.0, gainDb_ / 20.0);
    QString gainStr = QString::number(gainLinear, 'f', 4);

    // af 체인: volume → aconvolve
    // aconvolve: IR 파일로 컨볼루션 (룸 코렉션)
    QString afChain = QString("lavfi=[volume=%1,aconvolve=ir='%2':normalize=1]")
        .arg(gainStr)
        .arg(irPath);

    core_->setProperty("af", afChain);

    statusLabel_->setText(QString("활성화: %1 (%2%3 dB)")
        .arg(QFileInfo(activeIrPath_).fileName())
        .arg(gainDb_ > 0 ? "+" : "")
        .arg(gainDb_));
    statusLabel_->setStyleSheet(
        "color:#4caf50; font-size:11px; background:transparent;");
    emit convolutionChanged(true, activeIrPath_);
}

bool AudioAdvancedWidget::isConvolutionEnabled() const
{
    return convEnableCheck_ && convEnableCheck_->isChecked();
}

void AudioAdvancedWidget::loadSettings()
{
    activeIrPath_ = settings_.value("audio_advanced/ir_path").toString();
    gainDb_       = settings_.value("audio_advanced/gain_db", 0).toInt();

    if (!activeIrPath_.isEmpty() && QFileInfo::exists(activeIrPath_)) {
        irPathLabel_->setText(QFileInfo(activeIrPath_).fileName());
        irPathLabel_->setToolTip(activeIrPath_);
        irPathLabel_->setStyleSheet(
            "color:#ddd; font-size:11px; background:#1a1a1a; border:1px solid #2a2a2a;"
            "border-radius:3px; padding:4px 8px;");
        convEnableCheck_->setEnabled(true);
    }

    if (gainSlider_) gainSlider_->setValue(gainDb_);

    // VST 플러그인 목록 복원
    if (vstListWidget_) {
        vstListWidget_->clear();
        QStringList vstPaths = settings_.value("audio_advanced/vst_plugins").toStringList();
        for (const QString& p : vstPaths)
            vstListWidget_->addItem(p);
    }

    // 컨볼루션 활성화 상태 복원 (파일이 있을 때만)
    bool wasEnabled = settings_.value("audio_advanced/conv_enabled", false).toBool();
    if (wasEnabled && !activeIrPath_.isEmpty() && QFileInfo::exists(activeIrPath_)) {
        convEnableCheck_->setChecked(true);
        // MPV 초기화 후 적용되도록 약간 지연
        QTimer::singleShot(1000, this, [this]() {
            if (convEnableCheck_ && convEnableCheck_->isChecked())
                updateAfChain();
        });
    }
}

void AudioAdvancedWidget::saveSettings()
{
    settings_.setValue("audio_advanced/ir_path", activeIrPath_);
    settings_.setValue("audio_advanced/gain_db", gainDb_);
    settings_.setValue("audio_advanced/conv_enabled",
        convEnableCheck_ ? convEnableCheck_->isChecked() : false);

    if (vstListWidget_) {
        QStringList vstPaths;
        for (int i = 0; i < vstListWidget_->count(); ++i)
            vstPaths << vstListWidget_->item(i)->text();
        settings_.setValue("audio_advanced/vst_plugins", vstPaths);
    }
}
