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
// 스트리밍 방식으로 디스크에 직접 저장 (메모리 사용 최소화)
// 90MB 설치파일을 readAll()로 메모리에 올리지 않음
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
    void onDownloadReadyRead();          // 스트리밍: 수신 즉시 디스크에 쓰기
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
    QNetworkReply*         currentReply_ = nullptr;  // 스트리밍용 현재 reply
    QFile*                 downloadFile_ = nullptr;  // 스트리밍 저장 파일
    bool downloading_ = false;
};
