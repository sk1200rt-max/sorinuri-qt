#include "CloudDriveBrowserWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QListWidgetItem>
#include <QProgressBar>
#include <QGroupBox>
#include <QFileInfo>

static const QString CLOUD_STYLE =
    "QWidget { background: #111; color: #ccc; }"
    "QTabWidget::pane { border: 1px solid #2a2a2a; background: #111; }"
    "QTabBar::tab { background: #0d0d0d; color: #666; padding: 6px 16px;"
    "  border: 1px solid #1a1a1a; border-bottom: none; font-size: 11px; }"
    "QTabBar::tab:selected { background: #111; color: #00D4B4; border-color: #00D4B4; }"
    "QTabBar::tab:hover { color: #ccc; }"
    "QPushButton { background: #1e1e1e; color: #ccc; border: 1px solid #2a2a2a;"
    "  border-radius: 3px; padding: 5px 12px; font-size: 11px; }"
    "QPushButton:hover { background: #2a2a2a; border-color: #00D4B4; }"
    "QListWidget { background: #0d0d0d; border: 1px solid #1e1e1e; border-radius: 4px; }"
    "QListWidget::item { padding: 6px 8px; border-bottom: 1px solid #1a1a1a; }"
    "QListWidget::item:selected { background: #1a3a2a; color: #00D4B4; }"
    "QLabel { color: #aaa; font-size: 11px; }"
    "QProgressBar { background: #1a1a1a; border: none; border-radius: 1px; height: 3px; }"
    "QProgressBar::chunk { background: #00D4B4; }";

CloudDriveBrowserWidget::CloudDriveBrowserWidget(CloudDriveManager* manager, QWidget* parent)
    : QWidget(parent), manager_(manager)
{
    buildUI();

    if (manager_) {
        connect(manager_, &CloudDriveManager::filesListed,
                this, &CloudDriveBrowserWidget::onFilesListed);
        connect(manager_, &CloudDriveManager::authenticated,
                this, &CloudDriveBrowserWidget::onAuthenticated);
        connect(manager_, &CloudDriveManager::downloadUrlReady,
                this, &CloudDriveBrowserWidget::onDownloadUrlReady);
        connect(manager_, &CloudDriveManager::error,
                this, &CloudDriveBrowserWidget::onError);
    }
}

void CloudDriveBrowserWidget::buildUI() {
    setStyleSheet(CLOUD_STYLE);
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // 헤더
    auto* header = new QWidget;
    header->setFixedHeight(40);
    header->setStyleSheet("background: #0d0d0d; border-bottom: 1px solid #1e1e1e;");
    auto* hLayout = new QHBoxLayout(header);
    hLayout->setContentsMargins(12, 0, 12, 0);
    auto* lblHdr = new QLabel("☁  클라우드 드라이브");
    lblHdr->setStyleSheet("color: #00D4B4; font-size: 13px; font-weight: bold;");
    hLayout->addWidget(lblHdr);
    hLayout->addStretch();
    root->addWidget(header);

    // 탭
    tabs_ = new QTabWidget(this);
    tabs_->setDocumentMode(true);

    auto* oneDrivePage = new QWidget();
    auto* googlePage   = new QWidget();
    buildProviderTab(oneDrivePage, CloudDriveManager::Provider::OneDrive);
    buildProviderTab(googlePage,   CloudDriveManager::Provider::GoogleDrive);

    tabs_->addTab(oneDrivePage, "📁 OneDrive");
    tabs_->addTab(googlePage,   "📁 Google Drive");
    connect(tabs_, &QTabWidget::currentChanged, this, [this](int idx) {
        currentProvider_ = (idx == 0) ? CloudDriveManager::Provider::OneDrive
                                       : CloudDriveManager::Provider::GoogleDrive;
    });
    root->addWidget(tabs_, 1);

    // 하단 상태바
    auto* statusBar = new QWidget;
    statusBar->setFixedHeight(24);
    statusBar->setStyleSheet("background: #0d0d0d; border-top: 1px solid #1a1a1a;");
    auto* sbLayout = new QHBoxLayout(statusBar);
    sbLayout->setContentsMargins(8, 0, 8, 0);
    progressBar_ = new QProgressBar;
    progressBar_->setRange(0, 0);
    progressBar_->setFixedHeight(3);
    progressBar_->setTextVisible(false);
    progressBar_->hide();
    statusLabel_ = new QLabel("클라우드 드라이브에 연결하여 미디어 파일을 스트리밍하세요");
    statusLabel_->setStyleSheet("color: #555; font-size: 10px;");
    sbLayout->addWidget(progressBar_, 1);
    sbLayout->addWidget(statusLabel_);
    root->addWidget(statusBar);
}

void CloudDriveBrowserWidget::buildProviderTab(QWidget* page, CloudDriveManager::Provider provider) {
    bool isOneDrive = (provider == CloudDriveManager::Provider::OneDrive);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    // 인증 상태 영역
    auto* authGroup = new QGroupBox(isOneDrive ? "Microsoft OneDrive" : "Google Drive");
    authGroup->setStyleSheet(
        "QGroupBox { border: 1px solid #2a2a2a; border-radius: 4px; margin-top: 10px;"
        "  color: #888; font-size: 11px; padding-top: 6px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; }");
    auto* authLayout = new QHBoxLayout(authGroup);

    QLabel*      lblStatus  = nullptr;
    QPushButton* btnAuth    = nullptr;
    QPushButton* btnRefresh = nullptr;

    if (isOneDrive) {
        lblOneDriveStatus_ = new QLabel("연결되지 않음");
        lblStatus = lblOneDriveStatus_;
        btnOneDriveAuth_ = new QPushButton("🔗  로그인");
        btnAuth = btnOneDriveAuth_;
        btnOneDriveRefresh_ = new QPushButton("↺  새로고침");
        btnRefresh = btnOneDriveRefresh_;
        btnOneDriveRefresh_->setEnabled(false);
    } else {
        lblGoogleStatus_ = new QLabel("연결되지 않음");
        lblStatus = lblGoogleStatus_;
        btnGoogleAuth_ = new QPushButton("🔗  로그인");
        btnAuth = btnGoogleAuth_;
        btnGoogleRefresh_ = new QPushButton("↺  새로고침");
        btnRefresh = btnGoogleRefresh_;
        btnGoogleRefresh_->setEnabled(false);
    }

    lblStatus->setStyleSheet("color: #888; font-size: 11px;");
    btnAuth->setStyleSheet(
        "QPushButton { background: #1a3a2a; color: #00D4B4; border: 1px solid #00D4B4;"
        "  border-radius: 4px; padding: 5px 14px; }"
        "QPushButton:hover { background: #1e4a3a; }");

    authLayout->addWidget(lblStatus, 1);
    authLayout->addWidget(btnAuth);
    authLayout->addWidget(btnRefresh);
    layout->addWidget(authGroup);

    // 파일 목록
    QListWidget* list = nullptr;
    if (isOneDrive) {
        oneDriveList_ = new QListWidget;
        list = oneDriveList_;
    } else {
        googleList_ = new QListWidget;
        list = googleList_;
    }
    list->setIconSize(QSize(24, 24));
    layout->addWidget(list, 1);

    // 재생 버튼
    auto* btnPlay = new QPushButton("▶  선택 파일 재생");
    btnPlay->setStyleSheet(
        "QPushButton { background: #1a3a2a; color: #00D4B4; border: none;"
        "  border-radius: 4px; padding: 8px; font-size: 12px; }"
        "QPushButton:hover { background: #1e4a3a; }");
    layout->addWidget(btnPlay);

    // 인증 버튼 연결
    if (isOneDrive) {
        connect(btnAuth, &QPushButton::clicked, this, &CloudDriveBrowserWidget::onOneDriveAuth);
        connect(btnRefresh, &QPushButton::clicked, this, &CloudDriveBrowserWidget::onOneDriveRefresh);
        connect(list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
            if (!manager_) return;
            CloudFile f;
            f.id          = item->data(Qt::UserRole).toString();
            f.name        = item->text().mid(3);  // 이모지 제거
            f.downloadUrl = item->data(Qt::UserRole + 1).toString();
            manager_->getDownloadUrl(CloudDriveManager::Provider::OneDrive, f);
        });
        connect(btnPlay, &QPushButton::clicked, this, [this]() {
            auto* item = oneDriveList_->currentItem();
            if (item) emit oneDriveList_->itemDoubleClicked(item);
        });
    } else {
        connect(btnAuth, &QPushButton::clicked, this, &CloudDriveBrowserWidget::onGoogleAuth);
        connect(btnRefresh, &QPushButton::clicked, this, &CloudDriveBrowserWidget::onGoogleRefresh);
        connect(list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
            if (!manager_) return;
            CloudFile f;
            f.id          = item->data(Qt::UserRole).toString();
            f.name        = item->text().mid(3);
            f.downloadUrl = item->data(Qt::UserRole + 1).toString();
            manager_->getDownloadUrl(CloudDriveManager::Provider::GoogleDrive, f);
        });
        connect(btnPlay, &QPushButton::clicked, this, [this]() {
            auto* item = googleList_->currentItem();
            if (item) emit googleList_->itemDoubleClicked(item);
        });
    }

    // 이미 인증된 경우 상태 업데이트
    if (manager_ && manager_->isAuthenticated(provider)) {
        lblStatus->setText("✅ 연결됨");
        lblStatus->setStyleSheet("color: #00D4B4; font-size: 11px;");
        btnRefresh->setEnabled(true);
    }
}

void CloudDriveBrowserWidget::onOneDriveAuth() {
    if (!manager_) return;
    if (manager_->isAuthenticated(CloudDriveManager::Provider::OneDrive)) {
        manager_->logout(CloudDriveManager::Provider::OneDrive);
        if (lblOneDriveStatus_) {
            lblOneDriveStatus_->setText("연결되지 않음");
            lblOneDriveStatus_->setStyleSheet("color: #888; font-size: 11px;");
        }
        if (btnOneDriveAuth_) btnOneDriveAuth_->setText("🔗  로그인");
        if (btnOneDriveRefresh_) btnOneDriveRefresh_->setEnabled(false);
        if (oneDriveList_) oneDriveList_->clear();
    } else {
        statusLabel_->setText("브라우저에서 Microsoft 계정으로 로그인하세요...");
        manager_->authenticate(CloudDriveManager::Provider::OneDrive);
    }
}

void CloudDriveBrowserWidget::onGoogleAuth() {
    if (!manager_) return;
    if (manager_->isAuthenticated(CloudDriveManager::Provider::GoogleDrive)) {
        manager_->logout(CloudDriveManager::Provider::GoogleDrive);
        if (lblGoogleStatus_) {
            lblGoogleStatus_->setText("연결되지 않음");
            lblGoogleStatus_->setStyleSheet("color: #888; font-size: 11px;");
        }
        if (btnGoogleAuth_) btnGoogleAuth_->setText("🔗  로그인");
        if (btnGoogleRefresh_) btnGoogleRefresh_->setEnabled(false);
        if (googleList_) googleList_->clear();
    } else {
        statusLabel_->setText("브라우저에서 Google 계정으로 로그인하세요...");
        manager_->authenticate(CloudDriveManager::Provider::GoogleDrive);
    }
}

void CloudDriveBrowserWidget::onOneDriveRefresh() {
    if (!manager_) return;
    progressBar_->show();
    statusLabel_->setText("OneDrive 파일 목록 불러오는 중...");
    currentProvider_ = CloudDriveManager::Provider::OneDrive;
    manager_->listFiles(CloudDriveManager::Provider::OneDrive);
}

void CloudDriveBrowserWidget::onGoogleRefresh() {
    if (!manager_) return;
    progressBar_->show();
    statusLabel_->setText("Google Drive 파일 목록 불러오는 중...");
    currentProvider_ = CloudDriveManager::Provider::GoogleDrive;
    manager_->listFiles(CloudDriveManager::Provider::GoogleDrive);
}

void CloudDriveBrowserWidget::onFilesListed(const QList<CloudFile>& files) {
    progressBar_->hide();

    QListWidget* list = (currentProvider_ == CloudDriveManager::Provider::OneDrive)
                        ? oneDriveList_ : googleList_;
    if (!list) return;

    list->clear();
    for (const CloudFile& f : files) {
        QString ext  = QFileInfo(f.name).suffix().toLower();
        QString icon = f.mimeType.startsWith("video") ||
                       QStringList{"mp4","mkv","avi","mov","wmv","m4v","webm"}.contains(ext)
                       ? "🎬" : "🎵";
        QString sizeStr;
        if (f.size > 0) {
            if (f.size > 1024*1024*1024)
                sizeStr = QString(" (%1 GB)").arg(f.size/1024.0/1024.0/1024.0, 0, 'f', 1);
            else
                sizeStr = QString(" (%1 MB)").arg(f.size/1024.0/1024.0, 0, 'f', 0);
        }
        auto* item = new QListWidgetItem(icon + "  " + f.name + sizeStr);
        item->setData(Qt::UserRole,     f.id);
        item->setData(Qt::UserRole + 1, f.downloadUrl);
        list->addItem(item);
    }

    statusLabel_->setText(QString("파일 %1개").arg(files.size()));
}

void CloudDriveBrowserWidget::onAuthenticated(CloudDriveManager::Provider provider) {
    bool isOneDrive = (provider == CloudDriveManager::Provider::OneDrive);
    QLabel*      lbl     = isOneDrive ? lblOneDriveStatus_  : lblGoogleStatus_;
    QPushButton* btnAuth = isOneDrive ? btnOneDriveAuth_     : btnGoogleAuth_;
    QPushButton* btnRef  = isOneDrive ? btnOneDriveRefresh_  : btnGoogleRefresh_;

    if (lbl) {
        lbl->setText("✅ 연결됨");
        lbl->setStyleSheet("color: #00D4B4; font-size: 11px;");
    }
    if (btnAuth) btnAuth->setText("🔓  로그아웃");
    if (btnRef)  btnRef->setEnabled(true);

    statusLabel_->setText(isOneDrive ? "OneDrive 연결 완료" : "Google Drive 연결 완료");

    // 자동으로 파일 목록 불러오기
    currentProvider_ = provider;
    progressBar_->show();
    manager_->listFiles(provider);
}

void CloudDriveBrowserWidget::onDownloadUrlReady(const QString& url, const QString& name) {
    emit fileRequested(url, name);
    statusLabel_->setText("재생 중: " + name);
}

void CloudDriveBrowserWidget::onError(const QString& msg) {
    progressBar_->hide();
    statusLabel_->setText("오류: " + msg);
}

void CloudDriveBrowserWidget::updateAuthStatus() {
    if (!manager_) return;

    bool oneDriveOk = manager_->isAuthenticated(CloudDriveManager::Provider::OneDrive);
    bool googleOk   = manager_->isAuthenticated(CloudDriveManager::Provider::GoogleDrive);

    if (lblOneDriveStatus_) {
        lblOneDriveStatus_->setText(oneDriveOk ? "✅ 연결됨" : "연결되지 않음");
        lblOneDriveStatus_->setStyleSheet(
            oneDriveOk ? "color: #00D4B4; font-size: 11px;" : "color: #888; font-size: 11px;");
    }
    if (btnOneDriveAuth_) btnOneDriveAuth_->setText(oneDriveOk ? "🔓  로그아웃" : "🔗  로그인");
    if (btnOneDriveRefresh_) btnOneDriveRefresh_->setEnabled(oneDriveOk);

    if (lblGoogleStatus_) {
        lblGoogleStatus_->setText(googleOk ? "✅ 연결됨" : "연결되지 않음");
        lblGoogleStatus_->setStyleSheet(
            googleOk ? "color: #00D4B4; font-size: 11px;" : "color: #888; font-size: 11px;");
    }
    if (btnGoogleAuth_) btnGoogleAuth_->setText(googleOk ? "🔓  로그아웃" : "🔗  로그인");
    if (btnGoogleRefresh_) btnGoogleRefresh_->setEnabled(googleOk);
}
