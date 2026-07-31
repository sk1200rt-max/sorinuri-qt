#include "SubtitleSearchDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QFileInfo>
#include <QSettings>

static const QString DARK_STYLE =
    "QDialog { background: #111; color: #ddd; }"
    "QLabel { color: #ccc; background: transparent; }"
    "QTableWidget { background: #1a1a1a; color: #ddd; border: 1px solid #2a2a2a;"
    "  gridline-color: #222; selection-background-color: #1565c0; }"
    "QTableWidget::item { padding: 4px 8px; }"
    "QHeaderView::section { background: #0d0d0d; color: #888; border: none;"
    "  padding: 4px 8px; font-size: 11px; }"
    "QComboBox { background: #1e1e1e; color: #ddd; border: 1px solid #2a2a2a;"
    "  border-radius: 3px; padding: 4px 8px; }"
    "QComboBox::drop-down { border: none; }"
    "QPushButton { background: #1e1e1e; color: #ccc; border: 1px solid #2a2a2a;"
    "  border-radius: 3px; padding: 6px 16px; font-size: 12px; }"
    "QPushButton:hover { background: #2a2a2a; border-color: #4fc3f7; }"
    "QPushButton:pressed { background: #1565c0; }"
    "QPushButton:disabled { color: #444; }"
    "QProgressBar { background: #1a1a1a; border: 1px solid #2a2a2a; border-radius: 3px;"
    "  height: 6px; text-align: center; }"
    "QProgressBar::chunk { background: #1565c0; border-radius: 3px; }";

SubtitleSearchDialog::SubtitleSearchDialog(const QString& videoPath, QWidget* parent)
    : QDialog(parent)
    , videoPath_(videoPath)
{
    setWindowTitle("자막 검색 — " + QFileInfo(videoPath).fileName());
    setMinimumSize(640, 400);
    setStyleSheet(DARK_STYLE);

    downloader_ = new SubtitleDownloader(this);
    connect(downloader_, &SubtitleDownloader::searchFinished,
            this, &SubtitleSearchDialog::onSearchFinished);
    connect(downloader_, &SubtitleDownloader::downloadFinished,
            this, &SubtitleSearchDialog::onDownloadFinished);
    connect(downloader_, &SubtitleDownloader::errorOccurred,
            this, &SubtitleSearchDialog::onError);

    setupUI();
    startSearch();
}

void SubtitleSearchDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(10);

    // 상단: 언어 선택 + 상태
    auto* topRow = new QHBoxLayout();
    topRow->addWidget(new QLabel("언어:", this));
    langCombo_ = new QComboBox(this);
    langCombo_->addItem("한국어 (ko)", "ko");
    langCombo_->addItem("영어 (en)", "en");
    langCombo_->addItem("한국어 + 영어", "ko,en");
    langCombo_->addItem("일본어 (ja)", "ja");
    langCombo_->addItem("중국어 간체 (zh-CN)", "zh-CN");
    langCombo_->setCurrentIndex(0);
    topRow->addWidget(langCombo_);

    auto* searchBtn = new QPushButton("다시 검색", this);
    connect(searchBtn, &QPushButton::clicked, this, [this]() {
        resultTable_->setRowCount(0);
        downloadBtn_->setEnabled(false);
        startSearch();
    });
    topRow->addWidget(searchBtn);
    topRow->addStretch();
    mainLayout->addLayout(topRow);

    // 진행 표시
    progressBar_ = new QProgressBar(this);
    progressBar_->setRange(0, 0);  // 인디케이터 모드
    progressBar_->setFixedHeight(6);
    mainLayout->addWidget(progressBar_);

    statusLabel_ = new QLabel("자막을 검색하는 중...", this);
    statusLabel_->setStyleSheet("color: #888; font-size: 11px; background: transparent;");
    mainLayout->addWidget(statusLabel_);

    // 결과 테이블
    resultTable_ = new QTableWidget(0, 5, this);
    resultTable_->setHorizontalHeaderLabels({"파일명", "언어", "다운로드 수", "평점", "청각장애"});
    resultTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    resultTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    resultTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    resultTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    resultTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    resultTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    resultTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    resultTable_->setAlternatingRowColors(true);
    resultTable_->setStyleSheet(resultTable_->styleSheet() +
        "QTableWidget { alternate-background-color: #141414; }");
    mainLayout->addWidget(resultTable_, 1);

    // 하단 버튼
    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();
    downloadBtn_ = new QPushButton("선택한 자막 다운로드", this);
    downloadBtn_->setEnabled(false);
    downloadBtn_->setStyleSheet(
        "QPushButton { background: #1565c0; color: #fff; border: none; "
        "border-radius: 4px; padding: 7px 20px; font-size: 12px; font-weight: 600; }"
        "QPushButton:hover { background: #1976d2; }"
        "QPushButton:disabled { background: #1a1a1a; color: #444; }");
    connect(downloadBtn_, &QPushButton::clicked, this, &SubtitleSearchDialog::onDownloadClicked);
    btnRow->addWidget(downloadBtn_);

    cancelBtn_ = new QPushButton("취소", this);
    connect(cancelBtn_, &QPushButton::clicked, this, &QDialog::reject);
    btnRow->addWidget(cancelBtn_);
    mainLayout->addLayout(btnRow);
}

void SubtitleSearchDialog::startSearch()
{
    progressBar_->setRange(0, 0);
    statusLabel_->setText("자막을 검색하는 중...");

    QString langStr = langCombo_->currentData().toString();
    QStringList langs = langStr.split(",");
    downloader_->search(videoPath_, langs);
}

void SubtitleSearchDialog::onSearchFinished(const QList<SubtitleResult>& results)
{
    progressBar_->setRange(0, 1);
    progressBar_->setValue(1);
    results_ = results;

    if (results.isEmpty()) {
        statusLabel_->setText("검색 결과가 없습니다. 다른 언어로 시도해 보세요.");
        return;
    }

    statusLabel_->setText(QString("%1개의 자막을 찾았습니다.").arg(results.size()));
    resultTable_->setRowCount(results.size());

    for (int i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        resultTable_->setItem(i, 0, new QTableWidgetItem(r.fileName));
        resultTable_->setItem(i, 1, new QTableWidgetItem(r.language.toUpper()));
        resultTable_->setItem(i, 2, new QTableWidgetItem(
            QString::number(r.downloadCount)));
        resultTable_->setItem(i, 3, new QTableWidgetItem(
            r.rating > 0 ? QString::number(r.rating, 'f', 1) : "-"));
        resultTable_->setItem(i, 4, new QTableWidgetItem(
            r.hearingImpaired ? "CC" : ""));

        // 한국어 자막 강조
        if (r.language == "ko") {
            for (int c = 0; c < 5; ++c) {
                auto* item = resultTable_->item(i, c);
                if (item) item->setForeground(QColor("#4fc3f7"));
            }
        }
    }

    // 첫 번째 항목 자동 선택
    resultTable_->selectRow(0);
    downloadBtn_->setEnabled(true);
}

void SubtitleSearchDialog::onDownloadClicked()
{
    int row = resultTable_->currentRow();
    if (row < 0 || row >= results_.size()) return;

    downloadBtn_->setEnabled(false);
    progressBar_->setRange(0, 0);
    statusLabel_->setText("자막을 다운로드하는 중...");

    // 영상 파일과 같은 폴더에 저장
    QString destDir = QFileInfo(videoPath_).absolutePath();
    downloader_->download(results_[row].id, destDir);
}

void SubtitleSearchDialog::onDownloadFinished(const QString& path)
{
    progressBar_->setRange(0, 1);
    progressBar_->setValue(1);
    downloadedPath_ = path;
    statusLabel_->setText("자막 다운로드 완료: " + QFileInfo(path).fileName());
    accept();
}

void SubtitleSearchDialog::onError(const QString& msg)
{
    progressBar_->setRange(0, 1);
    progressBar_->setValue(0);
    statusLabel_->setText("오류: " + msg);
    downloadBtn_->setEnabled(!results_.isEmpty());
    QMessageBox::warning(this, "자막 오류", msg);
}
