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

    QUrl dlUrl(installerUrl_);
    QNetworkRequest req;
    req.setUrl(dlUrl);
    QNetworkReply* reply = nam_->get(req);

    connect(reply, &QNetworkReply::downloadProgress,
            this, &UpdateDialog::onDownloadProgress);
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

    if (reply->error() != QNetworkReply::NoError) {
        // 다운로드 실패: 임시파일 정리 후 재시도 가능 상태로 복원
        QFile::remove(localInstallerPath_);  // 부분 다운로드 임시파일 정리
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

    // 파일 저장
    QByteArray data = reply->readAll();
    if (data.isEmpty()) {
        // 빈 응답: 다운로드 실패로 처리
        downloading_ = false;
        updateBtn_->setEnabled(true);
        skipBtn_->setEnabled(true);
        progressBar_->setVisible(false);
        statusLabel_->setText("⚠ 서버에서 빈 응답을 받았습니다. 다시 시도해 주세요.");
        return;
    }

    QFile file(localInstallerPath_);
    if (!file.open(QIODevice::WriteOnly)) {
        // 파일 쓰기 실패: 디스크 공간 부족 또는 권한 문제
        downloading_ = false;
        updateBtn_->setEnabled(true);
        skipBtn_->setEnabled(true);
        progressBar_->setVisible(false);
        statusLabel_->setText(
            QString("⚠ 파일 저장 실패: %1\n디스크 공간을 확인하세요.").arg(file.errorString()));
        qWarning() << "[UpdateDialog] 파일 저장 실패:" << file.errorString();
        return;
    }
    file.write(data);
    file.close();

    // 다운로드 완료 확인: 파일 크기 검증 (10KB 미만이면 손상된 파일)
    QFileInfo fi(localInstallerPath_);
    if (fi.size() < 10240) {
        QFile::remove(localInstallerPath_);
        downloading_ = false;
        updateBtn_->setEnabled(true);
        skipBtn_->setEnabled(true);
        progressBar_->setVisible(false);
        statusLabel_->setText("⚠ 다운로드된 파일이 손상된 것 같습니다. 다시 시도해 주세요.");
        qWarning() << "[UpdateDialog] 파일 크기 이상:" << fi.size() << "bytes";
        return;
    }

    statusLabel_->setText("설치 중...");
    progressBar_->setValue(100);

    // 설치파일 실행 후 앱 종료
    qDebug() << "[UpdateDialog] 설치 시작:" << localInstallerPath_;
    QProcess::startDetached(localInstallerPath_, QStringList());
    accept();
    QApplication::quit();
}
