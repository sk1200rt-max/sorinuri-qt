#include "SubtitleEditorWidget.h"
#include "MpvCore.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QPlainTextEdit>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDir>
#include <QMessageBox>
#include <QFontDatabase>
#include <QFileDialog>
#include <QTimer>

SubtitleEditorWidget::SubtitleEditorWidget(MpvCore* core, QWidget* parent)
    : QWidget(parent), core_(core)
{
    buildUI();
}

void SubtitleEditorWidget::buildUI() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 8, 12, 8);
    root->setSpacing(6);

    // ── 파일 경로 표시 ──────────────────────────────────────
    auto* rowFile = new QHBoxLayout;
    auto* lblTitle = new QLabel("자막 편집기");
    lblTitle->setStyleSheet("color:#00c8b4; font-weight:bold; font-size:12px;");
    lblFile_ = new QLabel("자막 파일 없음");
    lblFile_->setStyleSheet("color:#666; font-size:10px;");
    lblFile_->setWordWrap(false);
    lblFile_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    rowFile->addWidget(lblTitle);
    rowFile->addSpacing(12);
    rowFile->addWidget(lblFile_, 1);
    root->addLayout(rowFile);

    // ── 싱크 조절 행 ────────────────────────────────────────
    auto* rowSync = new QHBoxLayout;
    auto* lblSyncTitle = new QLabel("싱크 오프셋:");
    lblSyncTitle->setStyleSheet("color:#aaa; font-size:11px;");
    lblSyncTitle->setFixedWidth(80);

    spinSync_ = new QSpinBox;
    spinSync_->setRange(-30000, 30000);
    spinSync_->setValue(0);
    spinSync_->setSuffix(" ms");
    spinSync_->setSingleStep(100);
    spinSync_->setToolTip("양수: 자막 늦춤 / 음수: 자막 앞당김");
    spinSync_->setStyleSheet(
        "QSpinBox { background:#1e1e1e; color:#ccc; border:1px solid #333;"
        "border-radius:3px; padding:2px 6px; font-size:11px; }"
        "QSpinBox:hover { border-color:#00c8b4; }");
    spinSync_->setFixedWidth(110);

    auto* btnMinus500 = new QPushButton("-500ms");
    auto* btnMinus100 = new QPushButton("-100ms");
    auto* btnPlus100  = new QPushButton("+100ms");
    auto* btnPlus500  = new QPushButton("+500ms");
    auto* btnReset    = new QPushButton("리셋");

    QString btnStyle =
        "QPushButton { background:#1e1e1e; color:#aaa; border:1px solid #2a2a2a;"
        "border-radius:3px; padding:2px 8px; font-size:10px; }"
        "QPushButton:hover { background:#2a2a2a; color:#fff; border-color:#00c8b4; }";
    for (auto* b : {btnMinus500, btnMinus100, btnPlus100, btnPlus500, btnReset})
        b->setStyleSheet(btnStyle);

    connect(btnMinus500, &QPushButton::clicked, this, [this]{ spinSync_->setValue(spinSync_->value()-500); });
    connect(btnMinus100, &QPushButton::clicked, this, [this]{ spinSync_->setValue(spinSync_->value()-100); });
    connect(btnPlus100,  &QPushButton::clicked, this, [this]{ spinSync_->setValue(spinSync_->value()+100); });
    connect(btnPlus500,  &QPushButton::clicked, this, [this]{ spinSync_->setValue(spinSync_->value()+500); });
    connect(btnReset,    &QPushButton::clicked, this, [this]{ spinSync_->setValue(0); });
    connect(spinSync_, QOverload<int>::of(&QSpinBox::valueChanged), this, &SubtitleEditorWidget::onSyncOffsetChanged);

    rowSync->addWidget(lblSyncTitle);
    rowSync->addWidget(spinSync_);
    rowSync->addSpacing(8);
    rowSync->addWidget(btnMinus500);
    rowSync->addWidget(btnMinus100);
    rowSync->addWidget(btnPlus100);
    rowSync->addWidget(btnPlus500);
    rowSync->addWidget(btnReset);
    rowSync->addStretch();
    root->addLayout(rowSync);

    // ── 자막 내용 편집기 ────────────────────────────────────
    editor_ = new QPlainTextEdit;
    editor_->setPlaceholderText("자막 파일을 열면 여기에 내용이 표시됩니다.\n직접 편집 후 '적용'을 누르면 즉시 반영됩니다.");
    editor_->setStyleSheet(
        "QPlainTextEdit { background:#0e0e0e; color:#ccc; border:1px solid #2a2a2a;"
        "border-radius:4px; font-family:'Consolas','D2Coding',monospace; font-size:11px;"
        "selection-background-color:#1565c0; }");
    // 고정폭 폰트 설정
    QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    mono.setPointSize(10);
    editor_->setFont(mono);
    root->addWidget(editor_, 1);

    // ── 하단 버튼 행 ────────────────────────────────────────
    auto* rowBtn = new QHBoxLayout;

    btnReload_ = new QPushButton("다시 불러오기");
    btnApply_  = new QPushButton("적용 (MPV 재로드)");
    btnSave_   = new QPushButton("파일로 저장");

    btnApply_->setStyleSheet(
        "QPushButton { background:#00c8b4; color:#0e0e0e; border:none;"
        "border-radius:4px; padding:4px 16px; font-weight:bold; font-size:11px; }"
        "QPushButton:hover { background:#00e5cf; }");
    QString btnStyle2 =
        "QPushButton { background:#1e1e1e; color:#aaa; border:1px solid #333;"
        "border-radius:4px; padding:4px 12px; font-size:11px; }"
        "QPushButton:hover { background:#2a2a2a; color:#fff; }";
    btnReload_->setStyleSheet(btnStyle2);
    btnSave_->setStyleSheet(btnStyle2);

    connect(btnApply_,  &QPushButton::clicked, this, &SubtitleEditorWidget::onApplyEdit);
    connect(btnReload_, &QPushButton::clicked, this, &SubtitleEditorWidget::onReloadSubtitle);
    connect(btnSave_,   &QPushButton::clicked, this, [this]() {
        if (filePath_.isEmpty()) {
            // 원본 파일 없으면 다른 이름으로 저장
            QString savePath = QFileDialog::getSaveFileName(
                this, "자막 저장", "",
                "자막 파일 (*.srt *.ass *.ssa *.vtt);;SRT (*.srt);;ASS/SSA (*.ass *.ssa);;VTT (*.vtt)"
            );
            if (savePath.isEmpty()) return;
            filePath_ = savePath;
        }
        QFile f(filePath_);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&f);
            out.setEncoding(QStringConverter::Utf8);
            out << editor_->toPlainText();
            f.close();
            lblFile_->setText(QFileInfo(filePath_).fileName() + " [저장됨]");
            QTimer::singleShot(2000, this, [this](){
                if (!filePath_.isEmpty())
                    lblFile_->setText(QFileInfo(filePath_).fileName());
            });
        } else {
            QMessageBox::warning(this, "저장 실패",
                "파일을 저장할 수 없습니다:\n" + filePath_);
        }
    });

    rowBtn->addWidget(btnReload_);
    rowBtn->addStretch();
    rowBtn->addWidget(btnSave_);
    rowBtn->addWidget(btnApply_);
    root->addLayout(rowBtn);
}

void SubtitleEditorWidget::setSubtitleFile(const QString& path) {
    filePath_ = path;
    loadFile(path);
}

void SubtitleEditorWidget::loadFile(const QString& path) {
    if (path.isEmpty()) {
        lblFile_->setText("자막 파일 없음");
        editor_->clear();
        return;
    }
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        lblFile_->setText("파일 열기 실패: " + path);
        return;
    }
    QTextStream in(&f);
    in.setAutoDetectUnicode(true);
    editor_->setPlainText(in.readAll());
    f.close();
    lblFile_->setText(QFileInfo(path).fileName());
}

void SubtitleEditorWidget::onSyncOffsetChanged(int ms) {
    syncOffset_ = ms;
    if (core_) {
        // MPV sub-delay는 초 단위
        core_->setProperty("sub-delay", QVariant(ms / 1000.0));
    }
}

void SubtitleEditorWidget::onApplyEdit() {
    if (!core_) return;
    if (editor_->toPlainText().isEmpty()) return;

    // 임시 파일에 편집 내용 저장
    QString tmpPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                      + "/sorinuri_sub_edit.srt";
    QFile f(tmpPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "오류", "임시 파일 저장 실패");
        return;
    }
    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);
    out << editor_->toPlainText();
    f.close();

    // MPV에 자막 재로드
    core_->command({"sub-remove"});
    core_->command({"sub-add", tmpPath, "select"});
    // 싱크 오프셋 재적용
    core_->setProperty("sub-delay", QVariant(syncOffset_ / 1000.0));

    emit subtitleReloaded(tmpPath);
}

void SubtitleEditorWidget::onReloadSubtitle() {
    if (!filePath_.isEmpty()) {
        loadFile(filePath_);
    }
}
