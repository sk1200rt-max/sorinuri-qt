#pragma once
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QListWidget>
#include <QComboBox>
#include <QCheckBox>
#include <QSettings>
#include <QNetworkAccessManager>
#include <QNetworkReply>

/**
 * NetworkBrowserWidget - 네트워크 미디어 탐색 패널
 *
 * 탭 구성:
 *  1. SMB/NAS    - UNC 경로(\\server\share) 직접 입력 및 즐겨찾기
 *  2. ☁ WebDAV  - Nextcloud/ownCloud/개인 NAS WebDAV 스트리밍 (v6.17.0 신규)
 *  3. 360도 VR   - 현재 재생 중인 영상에 360도 구면 투영 적용
 *  4. 캐스팅     - 크롬캐스트/AirPlay 송출 (URL 기반)
 *
 * 기존 코드에 영향 없는 독립 모듈.
 * 파일 열기는 openFileRequested / fileRequested 시그널을 통해 MainWindow에 위임.
 */
class MpvCore;

class NetworkBrowserWidget : public QWidget {
    Q_OBJECT
public:
    explicit NetworkBrowserWidget(MpvCore* core, QWidget* parent = nullptr);

signals:
    void openFileRequested(const QString& path);   // SMB 경로 열기 요청
    void castRequested(const QString& url);         // 캐스팅 요청
    void fileRequested(const QString& url);         // WebDAV 파일 재생 요청

public slots:
    void loadSettings();
    void saveSettings();

private slots:
    void onSmbConnect();
    void onSmbFavoriteAdd();
    void onSmbFavoriteOpen(QListWidgetItem* item);
    void on360Toggle(bool on);
    void onCastStart();

private:
    void setupUI();
    void buildSmbTab(QWidget* parent);
    void build360Tab(QWidget* parent);
    void buildCastTab(QWidget* parent);
    void buildWebDavTab(QWidget* parent);  // v6.17.0 신규

    MpvCore*  core_     = nullptr;
    QSettings settings_;

    // SMB 탭
    QLineEdit*   smbPathEdit_      = nullptr;
    QPushButton* smbConnectBtn_    = nullptr;
    QPushButton* smbFavAddBtn_     = nullptr;
    QListWidget* smbFavList_       = nullptr;
    QLabel*      smbStatusLabel_   = nullptr;

    // 360도 VR 탭
    QCheckBox*   vr360Check_       = nullptr;
    QComboBox*   vrProjectionCombo_= nullptr;
    QLabel*      vr360StatusLabel_ = nullptr;

    // 캐스팅 탭
    QLineEdit*   castUrlEdit_      = nullptr;
    QPushButton* castStartBtn_     = nullptr;
    QLabel*      castStatusLabel_  = nullptr;

    // WebDAV 탭 (v6.17.0 신규)
    QLineEdit*   davUrlEdit_       = nullptr;
    QLineEdit*   davUserEdit_      = nullptr;
    QLineEdit*   davPassEdit_      = nullptr;
    QListWidget* davFileList_      = nullptr;
    QLabel*      davStatusLabel_   = nullptr;
    QString      davBaseUrl_;
    QString      davUser_;
    QString      davPass_;
};
