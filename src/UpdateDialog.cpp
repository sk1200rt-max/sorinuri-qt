#include "UpdateDialog.h"
#include "UpdateChecker.h"
#include <QVBoxLayout>
#include <QTimer>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QDir>
#include <QStandardPaths>
#include <QProcess>
#include <QFileInfo>
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
    // Windows 사용자 로컬 앱 데이터 폴더에 절대 경로로 저장한다. 네트워크 드라이브·상대
    // 경로는 Inno Setup 및 ShellExecuteEx에서 코드 1203을 유발할 수 있으므로 사용하지 않는다.
    QString updateDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (updateDir.isEmpty())
        updateDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (updateDir.isEmpty())
        updateDir = QDir::tempPath();
    updateDir = QDir(updateDir).filePath("updates");

    QDir updateDirectory(updateDir);
    if (!updateDirectory.exists() && !QDir().mkpath(updateDir)) {
        statusLabel_->setText("⚠ 업데이트 폴더를 만들 수 없습니다. 업데이트를 다시 시도해 주세요.");
        downloading_ = false;
        updateBtn_->setEnabled(true);
        skipBtn_->setEnabled(true);
        progressBar_->setVisible(false);
        qWarning() << "[UpdateDialog] 업데이트 폴더 생성 실패:" << updateDir;
        return;
    }

    const QUrl installerUrl(installerUrl_);
    const QString installerFileName = QFileInfo(installerUrl.path()).fileName();
    if (installerFileName.isEmpty() || !installerFileName.endsWith(".exe", Qt::CaseInsensitive)) {
        statusLabel_->setText("⚠ 업데이트 주소가 올바르지 않습니다. 나중에 다시 시도해 주세요.");
        downloading_ = false;
        updateBtn_->setEnabled(true);
        skipBtn_->setEnabled(true);
        progressBar_->setVisible(false);
        qWarning() << "[UpdateDialog] 잘못된 설치 파일 URL:" << installerUrl_;
        return;
    }

    // App Control 호환 Inno Setup 패키지는 원본 EXE와 두 BIN 파일을 같은 폴더에 둔다.
    // 세 파일을 모두 받은 뒤에만 EXE를 실행해 누락된 데이터 파일로 인한 설치 중단을 막는다.
    const QString baseName = installerFileName.left(installerFileName.size() - 4);
    remoteBundleUrls_ = {installerUrl.toString()};
    localBundlePaths_ = {updateDirectory.absoluteFilePath(installerFileName)};
    for (int part = 0; part < 2; ++part) {
        QUrl partUrl(installerUrl);
        partUrl.setPath(installerUrl.path().left(installerUrl.path().size() - 4)
                        + QString("-%1.bin").arg(part));
        remoteBundleUrls_.append(partUrl.toString());
        localBundlePaths_.append(updateDirectory.absoluteFilePath(
            QString("%1-%2.bin").arg(baseName).arg(part)));
    }
    localInstallerPath_ = localBundlePaths_.front();
    currentDownloadIndex_ = 0;

    // 이전 미완료 번들을 통째로 정리한다.
    for (const QString& path : localBundlePaths_)
        QFile::remove(path);
    startCurrentAssetDownload();
}

void UpdateDialog::startCurrentAssetDownload() {
    if (currentDownloadIndex_ < 0 || currentDownloadIndex_ >= localBundlePaths_.size()) {
        downloading_ = false;
        updateBtn_->setEnabled(true);
        skipBtn_->setEnabled(true);
        progressBar_->setVisible(false);
        statusLabel_->setText("⚠ 업데이트 번들 순서가 올바르지 않습니다. 다시 시도해 주세요.");
        qWarning() << "[UpdateDialog] 잘못된 번들 인덱스:" << currentDownloadIndex_;
        return;
    }

    downloadFile_ = new QFile(localBundlePaths_.at(currentDownloadIndex_), this);
    if (!downloadFile_->open(QIODevice::WriteOnly)) {
        const QString error = downloadFile_->errorString();
        resetDownloadBundle();
        downloading_ = false;
        updateBtn_->setEnabled(true);
        skipBtn_->setEnabled(true);
        progressBar_->setVisible(false);
        statusLabel_->setText(QString("⚠ 업데이트 파일 생성 실패: %1").arg(error));
        return;
    }

    statusLabel_->setText(QString("업데이트 파일 다운로드 중... (%1/%2)")
                          .arg(currentDownloadIndex_ + 1)
                          .arg(localBundlePaths_.size()));
    QNetworkRequest req;
    req.setUrl(QUrl(remoteBundleUrls_.at(currentDownloadIndex_)));
    // Qt 측 전송 타임아웃 비활성화 (0 = 무제한)
    // Apache Timeout(600초)만 적용됨
    req.setTransferTimeout(0);
    req.setRawHeader("Connection", "keep-alive");
    currentReply_ = nam_->get(req);

    // 스트리밍: 수신 즉시 디스크에 쓰기 (readAll() 대신 readyRead 사용)
    // → 설치 파일을 메모리에 올리지 않고 청크 단위로 디스크에 저장
    connect(currentReply_, &QNetworkReply::readyRead,
            this, &UpdateDialog::onDownloadReadyRead);
    connect(currentReply_, &QNetworkReply::downloadProgress,
            this, &UpdateDialog::onDownloadProgress);
}

void UpdateDialog::resetDownloadBundle() {
    if (downloadFile_) {
        downloadFile_->close();
        delete downloadFile_;
        downloadFile_ = nullptr;
    }
    for (const QString& path : localBundlePaths_)
        QFile::remove(path);
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
    const QNetworkReply::NetworkError networkError = reply->error();
    const QString networkErrorText = reply->errorString();
    reply->deleteLater();
    currentReply_ = nullptr;

    // 혹시 남은 데이터를 먼저 쓴 뒤 현재 파일을 닫는다.
    if (downloadFile_) {
        const QByteArray remaining = reply->readAll();
        if (!remaining.isEmpty())
            downloadFile_->write(remaining);
        downloadFile_->close();
        delete downloadFile_;
        downloadFile_ = nullptr;
    }

    if (networkError != QNetworkReply::NoError) {
        resetDownloadBundle();
        downloading_ = false;
        updateBtn_->setEnabled(true);
        skipBtn_->setEnabled(true);
        progressBar_->setValue(0);
        progressBar_->setVisible(false);
        statusLabel_->setText(
            QString("⚠ 업데이트 파일 다운로드 실패: %1\n'지금 업데이트' 버튼을 다시 눌러 재시도하세요.")
                .arg(networkErrorText));
        qWarning() << "[UpdateDialog] 업데이트 번들 다운로드 실패:" << networkErrorText;
        return;
    }

    const QFileInfo downloadedFile(localBundlePaths_.at(currentDownloadIndex_));
    const qint64 minimumSize = currentDownloadIndex_ == 0 ? 10 * 1024 * 1024 : 1;
    if (!downloadedFile.exists() || downloadedFile.size() < minimumSize) {
        const qint64 actualSize = downloadedFile.size();
        resetDownloadBundle();
        downloading_ = false;
        updateBtn_->setEnabled(true);
        skipBtn_->setEnabled(true);
        progressBar_->setVisible(false);
        statusLabel_->setText("⚠ 다운로드된 설치 파일이 손상됐습니다. 다시 시도해 주세요.");
        qWarning() << "[UpdateDialog] 업데이트 번들 파일 크기 이상:" << actualSize
                   << "index:" << currentDownloadIndex_;
        return;
    }

    ++currentDownloadIndex_;
    if (currentDownloadIndex_ < localBundlePaths_.size()) {
        startCurrentAssetDownload();
        return;
    }

    statusLabel_->setText("설치 중...");
    progressBar_->setValue(100);

    // 설치 EXE와 BIN 파일은 같은 로컬 절대 경로에 완전히 준비됐고,
    // 작업 폴더도 지정해 실행한다. 실패하면 현재 앱을 종료하지 않는다.
    const QFileInfo installerInfo(localInstallerPath_);
    const QString installerPath = installerInfo.absoluteFilePath();
    const QString workingDirectory = installerInfo.absolutePath();
    qint64 installerPid = 0;
    const bool started = QProcess::startDetached(
        installerPath, QStringList(), workingDirectory, &installerPid);
    if (!started) {
        downloading_ = false;
        updateBtn_->setEnabled(true);
        skipBtn_->setEnabled(true);
        progressBar_->setVisible(false);
        statusLabel_->setText(
            "⚠ 설치 프로그램을 시작하지 못했습니다. '지금 업데이트'를 다시 눌러 재시도하세요.");
        qWarning() << "[UpdateDialog] 설치 프로그램 시작 실패:" << installerPath
                   << "작업 폴더:" << workingDirectory;
        return;
    }

    qDebug() << "[UpdateDialog] 설치 번들 시작:" << installerPath
             << "PID:" << installerPid
             << "파일 수:" << localBundlePaths_.size();
    accept();
    QApplication::quit();
}
