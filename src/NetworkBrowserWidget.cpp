#include "NetworkBrowserWidget.h"
#include "MpvCore.h"
#include <QVBoxLayout>
#include <QNetworkInterface>
#include <QAbstractSocket>
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

    auto* smbPage = new QWidget(); buildSmbTab(smbPage);
    auto* vrPage  = new QWidget(); build360Tab(vrPage);
    auto* castPage= new QWidget(); buildCastTab(castPage);

    tabs->addTab(smbPage,  "SMB/NAS");
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
        "현재 버전에서는 MPV의 --stream-to 옵션을 통해 RTSP/HTTP 스트리밍으로\n"
        "네트워크 기기에 미디어를 전송합니다.\n\n"
        "크롬캐스트 네이티브 프로토콜 지원은 향후 업데이트 예정입니다.",
        parent);
    infoLabel->setStyleSheet("color:#555; font-size:10px; background:transparent;");
    infoLabel->setWordWrap(true);
    infoLay->addWidget(infoLabel);
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

    // MPV에 스트리밍 시작 명령 (실험적)
    // core_->command({"script-message", "cast-start", streamUrl});
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
