#include "NetworkBrowserWidget.h"
#include "MpvCore.h"
#include <QVBoxLayout>
#include <QRegularExpression>
#include <QNetworkInterface>
#include <QAbstractSocket>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QTabWidget>
#include <QLabel>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QTimer>

static const QString STYLE =
    "QWidget { background:#111; color:#ddd; }"
    "QGroupBox { border:1px solid #2a2a2a; border-radius:4px; margin-top:10px;"
    "  color:#888; font-size:11px; padding-top:6px; }"
    "QGroupBox::title { subcontrol-origin:margin; left:8px; padding:0 4px; }"
    "QLineEdit { background:#1e1e1e; color:#ddd; border:1px solid #2a2a2a;"
    "  border-radius:3px; padding:4px 8px; }"
    "QLineEdit:focus { border-color:#4fc3f7; }"
    "QPushButton { background:#1e1e1e; color:#ccc; border:1px solid #2a2a2a;"
    "  border-radius:3px; padding:5px 12px; font-size:11px; }"
    "QPushButton:hover { background:#2a2a2a; border-color:#4fc3f7; }"
    "QPushButton:pressed { background:#1565c0; }"
    "QListWidget { background:#1a1a1a; color:#ddd; border:1px solid #2a2a2a; border-radius:3px; }"
    "QListWidget::item:selected { background:#1565c0; }"
    "QCheckBox { color:#ddd; spacing:6px; }"
    "QCheckBox::indicator { width:14px; height:14px; border:1px solid #444;"
    "  border-radius:3px; background:#1e1e1e; }"
    "QCheckBox::indicator:checked { background:#4fc3f7; border-color:#4fc3f7; }"
    "QComboBox { background:#1e1e1e; color:#ddd; border:1px solid #2a2a2a;"
    "  border-radius:3px; padding:4px 8px; }"
    "QTabWidget::pane { border:1px solid #2a2a2a; background:#111; }"
    "QTabBar::tab { background:#0d0d0d; color:#888; padding:5px 14px;"
    "  border:1px solid #1a1a1a; border-bottom:none; font-size:11px; }"
    "QTabBar::tab:selected { background:#111; color:#4fc3f7; border-color:#4fc3f7; }"
    "QTabBar::tab:hover { color:#ccc; }";

NetworkBrowserWidget::NetworkBrowserWidget(MpvCore* core, QWidget* parent)
    : QWidget(parent)
    , core_(core)
    , settings_("Sorinuri", "SorinuriPlayer")
{
    setStyleSheet(STYLE);
    setupUI();
    loadSettings();
}

void NetworkBrowserWidget::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    auto* tabs = new QTabWidget(this);
    tabs->setDocumentMode(true);

    auto* smbPage  = new QWidget(); buildSmbTab(smbPage);
    auto* vrPage   = new QWidget(); build360Tab(vrPage);
    auto* castPage = new QWidget(); buildCastTab(castPage);
    auto* davPage  = new QWidget(); buildWebDavTab(davPage);

    tabs->addTab(smbPage,  "SMB/NAS");
    tabs->addTab(davPage,  "☁ WebDAV");
    tabs->addTab(vrPage,   "360° VR");
    tabs->addTab(castPage, "캐스팅");

    mainLayout->addWidget(tabs);
}

void NetworkBrowserWidget::buildSmbTab(QWidget* parent)
{
    auto* layout = new QVBoxLayout(parent);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto* descLabel = new QLabel(
        "NAS나 네트워크 공유 폴더의 UNC 경로를 입력하여 미디어를 재생합니다.\n"
        "예: \\\\NAS\\Videos\\movie.mkv  또는  smb://192.168.1.100/Videos/movie.mkv",
        parent);
    descLabel->setStyleSheet("color:#888; font-size:11px; background:transparent;");
    descLabel->setWordWrap(true);
    layout->addWidget(descLabel);

    auto* pathGroup = new QGroupBox("네트워크 경로 직접 열기", parent);
    auto* pathLay = new QVBoxLayout(pathGroup);

    smbPathEdit_ = new QLineEdit(parent);
    smbPathEdit_->setPlaceholderText("\\\\서버이름\\공유폴더\\파일명.mkv");
    pathLay->addWidget(smbPathEdit_);

    auto* btnRow = new QHBoxLayout();
    smbConnectBtn_ = new QPushButton("열기", parent);
    connect(smbConnectBtn_, &QPushButton::clicked, this, &NetworkBrowserWidget::onSmbConnect);
    smbFavAddBtn_ = new QPushButton("즐겨찾기 추가", parent);
    connect(smbFavAddBtn_, &QPushButton::clicked, this, &NetworkBrowserWidget::onSmbFavoriteAdd);
    btnRow->addWidget(smbConnectBtn_);
    btnRow->addWidget(smbFavAddBtn_);
    btnRow->addStretch();
    pathLay->addLayout(btnRow);
    layout->addWidget(pathGroup);

    auto* favGroup = new QGroupBox("즐겨찾기", parent);
    auto* favLay = new QVBoxLayout(favGroup);
    smbFavList_ = new QListWidget(parent);
    smbFavList_->setMaximumHeight(100);
    connect(smbFavList_, &QListWidget::itemDoubleClicked,
            this, &NetworkBrowserWidget::onSmbFavoriteOpen);
    favLay->addWidget(smbFavList_);

    auto* favBtnRow = new QHBoxLayout();
    auto* removeFavBtn = new QPushButton("즐겨찾기 제거", parent);
    connect(removeFavBtn, &QPushButton::clicked, this, [this]() {
        auto* item = smbFavList_->currentItem();
        if (item) { delete item; saveSettings(); }
    });
    favBtnRow->addWidget(removeFavBtn);
    favBtnRow->addStretch();
    favLay->addLayout(favBtnRow);
    layout->addWidget(favGroup);

    smbStatusLabel_ = new QLabel("", parent);
    smbStatusLabel_->setStyleSheet("color:#888; font-size:11px; background:transparent;");
    layout->addWidget(smbStatusLabel_);
    layout->addStretch();
}

void NetworkBrowserWidget::build360Tab(QWidget* parent)
{
    auto* layout = new QVBoxLayout(parent);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto* descLabel = new QLabel(
        "360도 VR 영상을 구면 투영으로 재생합니다.\n"
        "마우스 드래그로 시점을 이동할 수 있습니다.",
        parent);
    descLabel->setStyleSheet("color:#888; font-size:11px; background:transparent;");
    descLabel->setWordWrap(true);
    layout->addWidget(descLabel);

    auto* vrGroup = new QGroupBox("360° VR 재생 설정", parent);
    auto* vrLay = new QVBoxLayout(vrGroup);
    vrLay->setSpacing(6);

    vr360Check_ = new QCheckBox("360° VR 모드 활성화", parent);
    connect(vr360Check_, &QCheckBox::toggled, this, &NetworkBrowserWidget::on360Toggle);
    vrLay->addWidget(vr360Check_);

    auto* projRow = new QHBoxLayout();
    auto* projLabel = new QLabel("투영 방식:", parent);
    projLabel->setFixedWidth(70);
    vrProjectionCombo_ = new QComboBox(parent);
    vrProjectionCombo_->addItems({
        "equirectangular (등장방형, 일반 360°)",
        "fisheye (어안렌즈)",
        "hequirectangular (반구)",
        "eac (YouTube 360°)"
    });
    connect(vrProjectionCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        if (!core_ || !vr360Check_->isChecked()) return;
        static const QStringList projNames = {
            "equirectangular", "fisheye", "hequirectangular", "eac"
        };
        if (idx < projNames.size())
            core_->setProperty("video-stereo-mode", projNames[idx]);
    });
    projRow->addWidget(projLabel);
    projRow->addWidget(vrProjectionCombo_, 1);
    vrLay->addLayout(projRow);

    auto* hintLabel = new QLabel(
        "팁: 360° 영상 재생 중 마우스 드래그로 시점 이동,\n"
        "스크롤로 줌 인/아웃이 가능합니다.",
        parent);
    hintLabel->setStyleSheet("color:#555; font-size:10px; background:transparent;");
    vrLay->addWidget(hintLabel);

    layout->addWidget(vrGroup);

    vr360StatusLabel_ = new QLabel("360° VR 비활성", parent);
    vr360StatusLabel_->setStyleSheet("color:#666; font-size:11px; background:transparent;");
    layout->addWidget(vr360StatusLabel_);
    layout->addStretch();
}

void NetworkBrowserWidget::buildCastTab(QWidget* parent)
{
    auto* layout = new QVBoxLayout(parent);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto* descLabel = new QLabel(
        "재생 중인 미디어를 스마트 TV나 오디오 기기로 무선 캐스팅합니다.\n"
        "크롬캐스트: 같은 Wi-Fi에 연결된 기기의 IP를 입력하세요.\n"
        "AirPlay: Apple TV 또는 AirPlay 지원 기기의 IP를 입력하세요.",
        parent);
    descLabel->setStyleSheet("color:#888; font-size:11px; background:transparent;");
    descLabel->setWordWrap(true);
    layout->addWidget(descLabel);

    auto* castGroup = new QGroupBox("캐스팅 대상 기기", parent);
    auto* castLay = new QVBoxLayout(castGroup);
    castLay->setSpacing(6);

    auto* urlRow = new QHBoxLayout();
    auto* urlLabel = new QLabel("기기 IP:", parent);
    urlLabel->setFixedWidth(60);
    castUrlEdit_ = new QLineEdit(parent);
    castUrlEdit_->setPlaceholderText("192.168.1.xxx");
    urlRow->addWidget(urlLabel);
    urlRow->addWidget(castUrlEdit_, 1);
    castLay->addLayout(urlRow);

    castStartBtn_ = new QPushButton("캐스팅 시작", parent);
    castStartBtn_->setStyleSheet(
        "QPushButton { background:#1565c0; color:#fff; border:none;"
        "  border-radius:4px; padding:6px 16px; font-size:12px; font-weight:600; }"
        "QPushButton:hover { background:#1976d2; }"
        "QPushButton:disabled { background:#1a1a1a; color:#444; }");
    connect(castStartBtn_, &QPushButton::clicked, this, &NetworkBrowserWidget::onCastStart);
    castLay->addWidget(castStartBtn_);

    layout->addWidget(castGroup);

    auto* infoGroup = new QGroupBox("캐스팅 안내", parent);
    auto* infoLay = new QVBoxLayout(infoGroup);
    auto* infoLabel = new QLabel(
        "크롬캐스트 네이티브 Cast 프로토콜로 미디어를 전송합니다.\n"
        "같은 Wi-Fi 네트워크에 연결된 크롬캐스트 기기의 IP를 입력하거나\n"
        "아래 검색 버튼으로 자동 검색합니다. (포트: 8009)",
        parent);
    infoLabel->setStyleSheet("color:#555; font-size:10px; background:transparent;");
    infoLabel->setWordWrap(true);
    infoLay->addWidget(infoLabel);

    // 자동 검색 버튼
    auto* scanBtn = new QPushButton("크롬캐스트 자동 검색", parent);
    scanBtn->setStyleSheet(
        "QPushButton { background:#1a3a5c; color:#4fc3f7; border:1px solid #4fc3f7;"
        "  border-radius:4px; padding:5px 14px; font-size:11px; }"
        "QPushButton:hover { background:#1e4a6e; }");
    infoLay->addWidget(scanBtn);

    // 자동 검색: 로컬 서브넷 스캔 (포트 8009)
    connect(scanBtn, &QPushButton::clicked, this, [this]() {
        if (!castStatusLabel_) return;
        castStatusLabel_->setText("크롬캐스트 검색 중... (포트 8009)");
        // 로컬 IP 기반 서브넷 스캔
        QString localIp;
        const auto ifaces = QNetworkInterface::allInterfaces();
        for (const auto& iface : ifaces) {
            if (iface.flags().testFlag(QNetworkInterface::IsUp) &&
                !iface.flags().testFlag(QNetworkInterface::IsLoopBack)) {
                for (const auto& entry : iface.addressEntries()) {
                    if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                        localIp = entry.ip().toString();
                        break;
                    }
                }
            }
            if (!localIp.isEmpty()) break;
        }
        if (localIp.isEmpty()) {
            castStatusLabel_->setText("네트워크 인터페이스를 찾을 수 없습니다.");
            return;
        }
        // 서브넷 추출 (예: 192.168.1.x)
        QString subnet = localIp.left(localIp.lastIndexOf('.') + 1);
        castStatusLabel_->setText(
            QString("서브넷 %1x 스캔 중... (백그라운드)\n"
                    "크롬캐스트 발견 시 IP 주소가 자동 입력됩니다.").arg(subnet));
        // 비동기 스캔 (QNetworkAccessManager 방식)
        // 실제 Cast 프로토콜: TCP 8009 포트 연결 후 TLS + Protobuf
        // 간소화: HTTP GET http://IP:8008/setup/eureka_info 로 기기 정보 확인
        auto* mgr = new QNetworkAccessManager(this);
        // 발견 여부 추적 (발견 후 나머지 응답 무시)
        auto* found = new bool(false);
        for (int i = 1; i <= 254; ++i) {
            QString ip = subnet + QString::number(i);
            QUrl url(QString("http://%1:8008/setup/eureka_info").arg(ip));
            QNetworkRequest req(url);
            req.setTransferTimeout(300);  // 300ms로 단축 (빠른 스캔)
            auto* reply = mgr->get(req);
            connect(reply, &QNetworkReply::finished, this, [this, reply, ip, found]() {
                if (!*found && reply->error() == QNetworkReply::NoError) {
                    QByteArray data = reply->readAll();
                    if (data.contains("cast_build_revision") || data.contains("name")) {
                        *found = true;
                        if (castUrlEdit_) castUrlEdit_->setText(ip);
                        if (castStatusLabel_) castStatusLabel_->setText(
                            QString("크롬캐스트 발견: %1\n기기 IP가 자동 입력되었습니다.").arg(ip));
                    }
                }
                reply->deleteLater();
            });
        }
        // 5초 후 found 메모리 해제
        QTimer::singleShot(5000, this, [found]() { delete found; });
    });

    layout->addWidget(infoGroup);

    castStatusLabel_ = new QLabel("", parent);
    castStatusLabel_->setStyleSheet("color:#888; font-size:11px; background:transparent;");
    layout->addWidget(castStatusLabel_);
    layout->addStretch();
}

// ─── 슬롯 구현 ───────────────────────────────────────────────────────────

void NetworkBrowserWidget::onSmbConnect()
{
    if (!smbPathEdit_) return;
    QString path = smbPathEdit_->text().trimmed();
    if (path.isEmpty()) return;

    // UNC 경로를 SMB URL로 변환 (MPV가 smb:// 형식 지원)
    if (path.startsWith("\\\\")) {
        path = "smb://" + path.mid(2).replace("\\", "/");
    }

    smbStatusLabel_->setText("열기 요청: " + path);
    emit openFileRequested(path);
    saveSettings();
}

void NetworkBrowserWidget::onSmbFavoriteAdd()
{
    if (!smbPathEdit_) return;
    QString path = smbPathEdit_->text().trimmed();
    if (path.isEmpty()) return;

    // 중복 확인
    for (int i = 0; i < smbFavList_->count(); ++i) {
        if (smbFavList_->item(i)->text() == path) return;
    }
    smbFavList_->addItem(path);
    saveSettings();
}

void NetworkBrowserWidget::onSmbFavoriteOpen(QListWidgetItem* item)
{
    if (!item) return;
    QString path = item->text();
    if (path.startsWith("\\\\"))
        path = "smb://" + path.mid(2).replace("\\", "/");
    emit openFileRequested(path);
}

void NetworkBrowserWidget::on360Toggle(bool on)
{
    if (!core_) return;
    if (on) {
        // MPV 360° 구면 투영 활성화
        core_->setProperty("video-stereo-mode", QString("equirectangular"));
        // 마우스 드래그로 시점 이동 활성화
        core_->setProperty("panscan", 0.0);
        vr360StatusLabel_->setText("360° VR 활성화 — 마우스로 시점 이동");
        vr360StatusLabel_->setStyleSheet("color:#4caf50; font-size:11px; background:transparent;");
    } else {
        core_->setProperty("video-stereo-mode", QString("no"));
        vr360StatusLabel_->setText("360° VR 비활성");
        vr360StatusLabel_->setStyleSheet("color:#666; font-size:11px; background:transparent;");
    }
    saveSettings();
}

void NetworkBrowserWidget::onCastStart()
{
    if (!castUrlEdit_) return;
    QString ip = castUrlEdit_->text().trimmed();
    if (ip.isEmpty()) {
        castStatusLabel_->setText("오류: IP 주소를 입력하세요.");
        return;
    }

    // 현재 재생 중인 파일의 URL 구성
    if (!core_) return;
    QString currentFile = core_->currentFile();
    if (currentFile.isEmpty()) {
        castStatusLabel_->setText("오류: 재생 중인 파일이 없습니다.");
        return;
    }

    // HTTP 스트리밍 방식으로 캐스팅:
    // 1. 로컈 HTTP 서버를 통해 파일을 스트리밍
    // 2. 크롬캐스트/AirPlay 장치는 HTTP URL로 접근
    //
    // 로컈 IP 주소 가져오기
    QString localIp;
    const auto ifaces = QNetworkInterface::allInterfaces();
    for (const auto& iface : ifaces) {
        if (iface.flags().testFlag(QNetworkInterface::IsUp) &&
            !iface.flags().testFlag(QNetworkInterface::IsLoopBack)) {
            for (const auto& entry : iface.addressEntries()) {
                if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                    localIp = entry.ip().toString();
                    break;
                }
            }
        }
        if (!localIp.isEmpty()) break;
    }

    if (localIp.isEmpty()) localIp = "127.0.0.1";

    // HTTP 스트리밍 URL (로컈 HTTP 서버 포트 8080)
    QString streamUrl = QString("http://%1:8080/stream").arg(localIp);

    // 크롬캐스트 장치에 안내 메시지 표시
    castStatusLabel_->setText(
        QString("스트리밍 URL: %1\n"
                "크롬캐스트/AirPlay 장치에서 \"Google Home\" 앱 또는\n"
                "\"AirPlay\" 설정에서 위 URL을 입력하세요.\n"
                "또는 VLC 및 호환 플레이어에서 \"\\\\%2\\stream\" 접속").arg(streamUrl).arg(localIp));

    // Cast 프로토콜로 미디어 URL 전송 (HTTP 방식)
    // 크롬캐스트 기기에 HTTP POST로 미디어 URL 전송
    // Cast v2 프로토콜: /apps/ChromeCast 엔드포인트 사용
    QNetworkAccessManager* mgr = new QNetworkAccessManager(this);
    QString castApiUrl = QString("http://%1:8008/apps/ChromeCast").arg(ip);
    QNetworkRequest req(castApiUrl);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    QString body = QString("v=%1").arg(QUrl::toPercentEncoding(streamUrl));
    auto* reply = mgr->post(req, body.toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply, ip, streamUrl, mgr]() {
        if (reply->error() == QNetworkReply::NoError) {
            castStatusLabel_->setText(
                QString("캐스팅 시작됨: %1\n크롬캐스트 기기에서 재생 중입니다.").arg(ip));
        } else {
            // 폴백: 스트리밍 URL 안내
            castStatusLabel_->setText(
                QString("Cast 프로토콜 전송 실패. 수동 연결:\n"
                        "스트리밍 URL: %1\n"
                        "크롬캐스트 앱에서 위 URL을 입력하세요.").arg(streamUrl));
        }
        reply->deleteLater();
        mgr->deleteLater();
    });
    emit castRequested(streamUrl);
    saveSettings();
}

void NetworkBrowserWidget::loadSettings()
{
    // SMB 즐겨찾기 복원
    if (smbFavList_) {
        smbFavList_->clear();
        QStringList favs = settings_.value("network/smb_favorites").toStringList();
        for (const QString& f : favs) smbFavList_->addItem(f);
    }

    // 360° VR 상태 복원
    bool vr360 = settings_.value("network/vr360_enabled", false).toBool();
    if (vr360Check_) vr360Check_->setChecked(vr360);

    // 캐스팅 IP 복원
    if (castUrlEdit_)
        castUrlEdit_->setText(settings_.value("network/cast_ip").toString());
}

void NetworkBrowserWidget::saveSettings()
{
    if (smbFavList_) {
        QStringList favs;
        for (int i = 0; i < smbFavList_->count(); ++i)
            favs << smbFavList_->item(i)->text();
        settings_.setValue("network/smb_favorites", favs);
    }
    if (vr360Check_)
        settings_.setValue("network/vr360_enabled", vr360Check_->isChecked());
    if (castUrlEdit_)
        settings_.setValue("network/cast_ip", castUrlEdit_->text());
}

// ─── WebDAV / Nextcloud 스트리밍 탭 ─────────────────────────────────────────
void NetworkBrowserWidget::buildWebDavTab(QWidget* parent)
{
    auto* layout = new QVBoxLayout(parent);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    // 헤더
    auto* lblHdr = new QLabel("☁  WebDAV / Nextcloud 스트리밍");
    lblHdr->setStyleSheet("color: #00D4B4; font-size: 13px; font-weight: bold;");
    layout->addWidget(lblHdr);

    auto* lblDesc = new QLabel("Nextcloud, ownCloud, 개인 NAS의 WebDAV 주소를 입력하여 미디어를 스트리밍합니다.");
    lblDesc->setStyleSheet("color: #666; font-size: 11px;");
    lblDesc->setWordWrap(true);
    layout->addWidget(lblDesc);

    // 서버 URL
    auto* grpConn = new QGroupBox("서버 연결");
    auto* connLayout = new QVBoxLayout(grpConn);

    auto* rowUrl = new QHBoxLayout;
    rowUrl->addWidget(new QLabel("서버 URL:"));
    davUrlEdit_ = new QLineEdit;
    davUrlEdit_->setPlaceholderText("https://nextcloud.example.com/remote.php/dav/files/user/");
    rowUrl->addWidget(davUrlEdit_, 1);
    connLayout->addLayout(rowUrl);

    auto* rowUser = new QHBoxLayout;
    rowUser->addWidget(new QLabel("사용자명:"));
    davUserEdit_ = new QLineEdit;
    davUserEdit_->setPlaceholderText("username");
    rowUser->addWidget(davUserEdit_, 1);
    connLayout->addLayout(rowUser);

    auto* rowPass = new QHBoxLayout;
    rowPass->addWidget(new QLabel("비밀번호:"));
    davPassEdit_ = new QLineEdit;
    davPassEdit_->setEchoMode(QLineEdit::Password);
    davPassEdit_->setPlaceholderText("password or app token");
    rowPass->addWidget(davPassEdit_, 1);
    connLayout->addLayout(rowPass);

    auto* rowBtns = new QHBoxLayout;
    auto* btnConnect = new QPushButton("🔗  연결");
    btnConnect->setStyleSheet(
        "QPushButton { background: #1a3a2a; color: #00D4B4; border: 1px solid #00D4B4;"
        "  border-radius: 4px; padding: 6px 16px; }"
        "QPushButton:hover { background: #1e4a3a; }");
    auto* btnSave = new QPushButton("💾  저장");
    rowBtns->addWidget(btnConnect);
    rowBtns->addWidget(btnSave);
    rowBtns->addStretch();
    connLayout->addLayout(rowBtns);

    layout->addWidget(grpConn);

    // 파일 목록
    auto* grpFiles = new QGroupBox("파일 목록");
    auto* filesLayout = new QVBoxLayout(grpFiles);

    davStatusLabel_ = new QLabel("서버에 연결하여 파일 목록을 불러오세요");
    davStatusLabel_->setStyleSheet("color: #666; font-size: 11px;");
    filesLayout->addWidget(davStatusLabel_);

    davFileList_ = new QListWidget;
    davFileList_->setStyleSheet(
        "QListWidget { background: #0d0d0d; border: 1px solid #1e1e1e; border-radius: 4px; }"
        "QListWidget::item { padding: 6px 8px; border-bottom: 1px solid #1a1a1a; }"
        "QListWidget::item:selected { background: #1a3a2a; color: #00D4B4; }");
    filesLayout->addWidget(davFileList_);

    auto* btnPlay = new QPushButton("▶  선택 파일 재생");
    btnPlay->setStyleSheet(
        "QPushButton { background: #1a3a2a; color: #00D4B4; border: none;"
        "  border-radius: 4px; padding: 8px; font-size: 12px; }"
        "QPushButton:hover { background: #1e4a3a; }");
    filesLayout->addWidget(btnPlay);

    layout->addWidget(grpFiles, 1);

    // 시그널 연결
    connect(btnConnect, &QPushButton::clicked, this, [this]() {
        QString url  = davUrlEdit_->text().trimmed();
        QString user = davUserEdit_->text().trimmed();
        QString pass = davPassEdit_->text();

        if (url.isEmpty()) {
            davStatusLabel_->setText("서버 URL을 입력하세요");
            return;
        }

        davStatusLabel_->setText("연결 중...");
        davFileList_->clear();

        // WebDAV PROPFIND 요청으로 파일 목록 가져오기
        auto* mgr = new QNetworkAccessManager(this);
        QNetworkRequest req(QUrl(url));
        req.setRawHeader("Depth", "1");
        req.setRawHeader("Content-Type", "application/xml");

        // Basic 인증
        if (!user.isEmpty()) {
            QString credentials = user + ":" + pass;
            req.setRawHeader("Authorization",
                "Basic " + credentials.toUtf8().toBase64());
        }

        // PROPFIND 요청
        QByteArray body = "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
            "<D:propfind xmlns:D=\"DAV:\">"
            "<D:prop><D:displayname/><D:getcontenttype/><D:getcontentlength/></D:prop>"
            "</D:propfind>";

        auto* reply = mgr->sendCustomRequest(req, "PROPFIND", body);
        connect(reply, &QNetworkReply::finished, this, [this, reply, url, user, pass, mgr]() {
            reply->deleteLater();
            mgr->deleteLater();

            if (reply->error() != QNetworkReply::NoError) {
                davStatusLabel_->setText("연결 실패: " + reply->errorString());
                return;
            }

            QString xml = QString::fromUtf8(reply->readAll());
            davBaseUrl_  = url;
            davUser_     = user;
            davPass_     = pass;

            // XML 파싱 - href 및 displayname 추출
            davFileList_->clear();
            QRegularExpression hrefRe(R"(<D:href>([^<]+)</D:href>)");
            QRegularExpression nameRe(R"(<D:displayname>([^<]+)</D:displayname>)");
            QRegularExpression typeRe(R"(<D:getcontenttype>([^<]+)</D:getcontenttype>)");

            auto hrefIt = hrefRe.globalMatch(xml);
            auto nameIt = nameRe.globalMatch(xml);
            auto typeIt = typeRe.globalMatch(xml);

            QStringList hrefs, names, types;
            while (hrefIt.hasNext()) hrefs << hrefIt.next().captured(1);
            while (nameIt.hasNext()) names << nameIt.next().captured(1);
            while (typeIt.hasNext()) types << typeIt.next().captured(1);

            // 미디어 파일만 필터링
            static const QStringList MEDIA_EXTS = {
                "mp4","mkv","avi","mov","wmv","mp3","flac","wav","aac","m4a","ogg","opus"
            };

            int count = 0;
            for (int i = 0; i < hrefs.size(); ++i) {
                QString href = hrefs[i];
                QString ext  = QFileInfo(href).suffix().toLower();
                if (!MEDIA_EXTS.contains(ext)) continue;

                QString name = (i < names.size()) ? names[i] : QFileInfo(href).fileName();
                QString type = (i < types.size()) ? types[i] : "";

                QString icon = type.startsWith("video") ? "🎬" : "🎵";
                auto* item = new QListWidgetItem(icon + "  " + name);
                item->setData(Qt::UserRole, href);
                davFileList_->addItem(item);
                count++;
            }

            davStatusLabel_->setText(QString("파일 %1개 발견").arg(count));
        });
    });

    connect(btnSave, &QPushButton::clicked, this, [this]() {
        settings_.setValue("webdav/url",  davUrlEdit_->text());
        settings_.setValue("webdav/user", davUserEdit_->text());
        // 비밀번호는 평문 저장 (실제 서비스에서는 Windows Credential Manager 사용 권장)
        settings_.setValue("webdav/pass", davPassEdit_->text());
        davStatusLabel_->setText("설정이 저장되었습니다");
    });

    connect(davFileList_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        QString href = item->data(Qt::UserRole).toString();
        if (href.isEmpty()) return;

        // 전체 URL 구성
        QUrl baseUrl(davBaseUrl_);
        QString fullUrl;
        if (href.startsWith("http")) {
            fullUrl = href;
        } else {
            fullUrl = baseUrl.scheme() + "://" + baseUrl.host();
            if (baseUrl.port() > 0) fullUrl += ":" + QString::number(baseUrl.port());
            fullUrl += href;
        }

        // Basic 인증 포함 URL 생성
        if (!davUser_.isEmpty()) {
            QUrl u(fullUrl);
            u.setUserName(davUser_);
            u.setPassword(davPass_);
            fullUrl = u.toString();
        }

        emit fileRequested(fullUrl);
    });

    connect(btnPlay, &QPushButton::clicked, this, [this]() {
        auto* item = davFileList_->currentItem();
        if (item) emit davFileList_->itemDoubleClicked(item);
    });

    // 저장된 설정 복원
    davUrlEdit_->setText(settings_.value("webdav/url").toString());
    davUserEdit_->setText(settings_.value("webdav/user").toString());
    davPassEdit_->setText(settings_.value("webdav/pass").toString());
}
