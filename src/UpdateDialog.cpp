#include "UpdateDialog.h"
#include "UpdateChecker.h"
#include <QVBoxLayout>
#include <QTimer>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QDir>
#include <QStandardPaths>
#include <QProcess>
#include <QMessageBox>
#include <QApplication>
#include <QDebug>

UpdateDialog::UpdateDialog(const QString& newVersion,
                           const QString& releaseNotes,
                           const QString& installerUrl,
                           QWidget* parent)
    : QDialog(parent)
    , installerUrl_(installerUrl)
    , nam_(new QNetworkAccessManager(this))
{
    setWindowTitle("소리누리 업데이트");
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setFixedWidth(480);
    setupUI(newVersion, releaseNotes);

    // finished 시그널: 다운로드 완료 또는 오류 처리
    connect(nam_, &QNetworkAccessManager::finished,
            this, &UpdateDialog::onDownloadFinished);
}

void UpdateDialog::setupUI(const QString& newVersion, const QString& releaseNotes) {
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);
    layout->setContentsMargins(20, 20, 20, 20);

    // 제목
    auto* titleLabel = new QLabel(
        QString("<b>새 버전 %1이 출시됐습니다</b>").arg(newVersion), this);
    titleLabel->setStyleSheet("font-size: 14px; color: #4fc3f7;");
    layout->addWidget(titleLabel);

    // 현재 버전 표시
    auto* versionLabel = new QLabel(
        QString("현재 버전: %1  →  새 버전: %2")
            .arg(UpdateChecker::currentVersion(), newVersion), this);
    versionLabel->setStyleSheet("color: #aaa; font-size: 11px;");
    layout->addWidget(versionLabel);

    // 릴리스 노트
    if (!releaseNotes.isEmpty()) {
        auto* notesLabel = new QLabel("업데이트 내용:", this);
        notesLabel->setStyleSheet("font-weight: bold; margin-top: 8px;");
        layout->addWidget(notesLabel);

        auto* notes = new QTextEdit(this);
        notes->setPlainText(releaseNotes);
        notes->setReadOnly(true);
        notes->setMaximumHeight(120);
        notes->setStyleSheet(
            "background: #1e1e1e; color: #ccc; border: 1px solid #444; border-radius: 4px;");
        layout->addWidget(notes);
    }

    // 진행 상태
    statusLabel_ = new QLabel("업데이트를 다운로드하고 설치합니다.", this);
    statusLabel_->setStyleSheet("color: #aaa; font-size: 11px;");
    layout->addWidget(statusLabel_);

    progressBar_ = new QProgressBar(this);
    progressBar_->setRange(0, 100);
    progressBar_->setValue(0);
    progressBar_->setVisible(false);
    progressBar_->setStyleSheet(
        "QProgressBar { border: 1px solid #444; border-radius: 4px; "
        "background: #1e1e1e; height: 16px; text-align: center; }"
        "QProgressBar::chunk { background: #4fc3f7; border-radius: 3px; }");
    layout->addWidget(progressBar_);

    // 버튼
    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    skipBtn_ = new QPushButton("나중에", this);
    skipBtn_->setStyleSheet(
        "QPushButton { background: #333; color: #aaa; border: 1px solid #555; "
        "border-radius: 4px; padding: 6px 16px; }"
        "QPushButton:hover { background: #444; }");
    connect(skipBtn_, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(skipBtn_);

    updateBtn_ = new QPushButton("지금 업데이트", this);
    updateBtn_->setStyleSheet(
        "QPushButton { background: #4fc3f7; color: #000; border: none; "
        "border-radius: 4px; padding: 6px 20px; font-weight: bold; }"
        "QPushButton:hover { background: #81d4fa; }"
        "QPushButton:disabled { background: #555; color: #888; }");
    connect(updateBtn_, &QPushButton::clicked, this, &UpdateDialog::onUpdateClicked);
    btnLayout->addWidget(updateBtn_);

    layout->addLayout(btnLayout);
}

void UpdateDialog::onUpdateClicked() {
    if (downloading_) return;
    downloading_ = true;
    updateBtn_->setEnabled(false);
    skipBtn_->setEnabled(false);
    progressBar_->setVisible(true);
    statusLabel_->setText("다운로드 중...");
    startDownload();
}

void UpdateDialog::startDownload() {
    // 임시 폴더에 설치파일 저장
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    localInstallerPath_ = tempDir + "/Sorinuri-Setup-latest.exe";

    // 이전 임시파일 정리
    QFile::remove(localInstallerPath_);

    // 스트리밍 저장 파일 열기
    downloadFile_ = new QFile(localInstallerPath_, this);
    if (!downloadFile_->open(QIODevice::WriteOnly)) {
        statusLabel_->setText(
            QString("⚠ 임시 파일 생성 실패: %1").arg(downloadFile_->errorString()));
        downloading_ = false;
        updateBtn_->setEnabled(true);
        skipBtn_->setEnabled(true);
        progressBar_->setVisible(false);
        delete downloadFile_;
        downloadFile_ = nullptr;
        return;
    }

    QNetworkRequest req;
    req.setUrl(QUrl(installerUrl_));
    // Qt 측 전송 타임아웃 비활성화 (0 = 무제한)
    // Apache Timeout(600초)만 적용됨
    req.setTransferTimeout(0);
    // HTTP Keep-Alive 유지
    req.setRawHeader("Connection", "keep-alive");

    currentReply_ = nam_->get(req);

    // 스트리밍: 수신 즉시 디스크에 쓰기 (readAll() 대신 readyRead 사용)
    // → 90MB를 메모리에 올리지 않고 청크 단위로 디스크에 저장
    connect(currentReply_, &QNetworkReply::readyRead,
            this, &UpdateDialog::onDownloadReadyRead);
    connect(currentReply_, &QNetworkReply::downloadProgress,
            this, &UpdateDialog::onDownloadProgress);
}

void UpdateDialog::onDownloadReadyRead() {
    // 수신된 데이터를 즉시 디스크에 쓰기 (메모리 버퍼 최소화)
    if (downloadFile_ && currentReply_) {
        downloadFile_->write(currentReply_->readAll());
    }
}

void UpdateDialog::onDownloadProgress(qint64 received, qint64 total) {
    if (total > 0) {
        int pct = static_cast<int>(received * 100 / total);
        progressBar_->setValue(pct);
        statusLabel_->setText(QString("다운로드 중... %1 MB / %2 MB")
            .arg(received / 1024 / 1024)
            .arg(total / 1024 / 1024));
    }
}

void UpdateDialog::onDownloadFinished(QNetworkReply* reply) {
    reply->deleteLater();
    currentReply_ = nullptr;

    // 파일 닫기
    if (downloadFile_) {
        // 혹시 남은 데이터 마저 쓰기
        QByteArray remaining = reply->readAll();
        if (!remaining.isEmpty())
            downloadFile_->write(remaining);
        downloadFile_->close();
        delete downloadFile_;
        downloadFile_ = nullptr;
    }

    if (reply->error() != QNetworkReply::NoError) {
        // 다운로드 실패: 임시파일 정리 후 재시도 가능 상태로 복원
        QFile::remove(localInstallerPath_);
        downloading_ = false;
        updateBtn_->setEnabled(true);
        skipBtn_->setEnabled(true);
        progressBar_->setValue(0);
        progressBar_->setVisible(false);
        statusLabel_->setText(
            QString("⚠ 다운로드 실패: %1\n'지금 업데이트' 버튼을 다시 눌러 재시도하세요.")
                .arg(reply->errorString()));
        qWarning() << "[UpdateDialog] 다운로드 실패:" << reply->errorString();
        return;
    }

    // 다운로드 완료 확인: 파일 크기 검증 (10MB 미만이면 손상된 파일)
    QFileInfo fi(localInstallerPath_);
    if (fi.size() < 10 * 1024 * 1024) {
        QFile::remove(localInstallerPath_);
        downloading_ = false;
        updateBtn_->setEnabled(true);
        skipBtn_->setEnabled(true);
        progressBar_->setVisible(false);
        statusLabel_->setText(
            QString("⚠ 다운로드된 파일이 손상됐습니다 (%1 MB). 다시 시도해 주세요.")
                .arg(fi.size() / 1024 / 1024));
        qWarning() << "[UpdateDialog] 파일 크기 이상:" << fi.size() << "bytes";
        return;
    }

    statusLabel_->setText("설치 중...");
    progressBar_->setValue(100);

    // 설치파일 실행 후 앱 종료
    qDebug() << "[UpdateDialog] 설치 시작:" << localInstallerPath_
             << "크기:" << fi.size() / 1024 / 1024 << "MB";
    QProcess::startDetached(localInstallerPath_, QStringList());
    accept();
    QApplication::quit();
}
