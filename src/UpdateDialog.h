#pragma once
#include <QDialog>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include <QFile>
#include <QFileInfo>

// 업데이트 다운로드 및 설치 다이얼로그
class UpdateDialog : public QDialog {
    Q_OBJECT
public:
    explicit UpdateDialog(const QString& newVersion,
                          const QString& releaseNotes,
                          const QString& installerUrl,
                          QWidget* parent = nullptr);

private slots:
    void onUpdateClicked();
    void onDownloadProgress(qint64 received, qint64 total);
    void onDownloadFinished(QNetworkReply* reply);

private:
    void setupUI(const QString& newVersion, const QString& releaseNotes);
    void startDownload();

    QString installerUrl_;
    QString localInstallerPath_;

    QLabel*       statusLabel_;
    QProgressBar* progressBar_;
    QPushButton*  updateBtn_;
    QPushButton*  skipBtn_;

    QNetworkAccessManager* nam_;
    bool downloading_ = false;
};
