#pragma once
#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QComboBox>
#include <QCheckBox>
#include "SubtitleDownloader.h"

/**
 * SubtitleSearchDialog - 자막 검색 결과 표시 및 다운로드 다이얼로그
 *
 * 사용법:
 *   SubtitleSearchDialog dlg(filePath, this);
 *   if (dlg.exec() == QDialog::Accepted)
 *       loadSubtitle(dlg.downloadedPath());
 */
class SubtitleSearchDialog : public QDialog {
    Q_OBJECT
public:
    explicit SubtitleSearchDialog(const QString& videoPath, QWidget* parent = nullptr);
    QString downloadedPath() const { return downloadedPath_; }

private slots:
    void onSearchFinished(const QList<SubtitleResult>& results);
    void onDownloadFinished(const QString& path);
    void onTranslateFinished(const QString& translatedPath);
    void onError(const QString& msg);
    void onDownloadClicked();

private:
    void setupUI();
    void startSearch();

    SubtitleDownloader* downloader_    = nullptr;
    QString             videoPath_;
    QString             downloadedPath_;

    QLabel*       statusLabel_  = nullptr;
    QProgressBar* progressBar_  = nullptr;
    QComboBox*    langCombo_    = nullptr;
    QTableWidget* resultTable_  = nullptr;
    QPushButton*  downloadBtn_   = nullptr;
    QPushButton*  cancelBtn_     = nullptr;
    QCheckBox*    translateCheck_ = nullptr;  // 자동 번역 옵션

    QList<SubtitleResult> results_;
};
