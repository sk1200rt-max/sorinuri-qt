#pragma once
#include <QWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QJsonArray>
#include <QListWidget>
#include <QListWidgetItem>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QSettings>
#include <QPixmap>
#include <QMap>
#include <QAbstractItemDelegate>
#include <QPainter>
#include <QStyleOptionViewItem>

// ──────────────────────────────────────────────────────────────────────────────
// OriginalsWidget — SORINURI ORIGINALS 탭
//
// songs.json을 5분마다 자동 fetch하여 곡 목록을 표시합니다.
// 썸네일 + 제목 + 재생시간 표시, 카테고리 필터, 검색, 정렬,
// 새 곡 알림, 재생 중 표시, 더블클릭 재생, 전체 재생 기능 포함.
//
// 브랜드 컬러: 민트 #00D4B4
// API URL: https://sorinuri.com/api/songs.json
// M3U URL: https://sorinuri.com/api/sorinuri_originals.m3u
// ──────────────────────────────────────────────────────────────────────────────

struct SongInfo {
    QString id;
    QString title;
    QString artist;
    QString duration;
    QString mp3;          // 서버 상대 경로 (/music/originals/...)
    QString cover;        // 서버 상대 경로
    QString youtubeId;
    QString youtubeUrl;
    QStringList categories;
    QString mood;
    QStringList tags;
};

// ── 커스텀 아이템 델리게이트 (썸네일 + 제목 + 재생시간) ─────────────────────
class SongItemDelegate : public QAbstractItemDelegate {
    Q_OBJECT
public:
    explicit SongItemDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;

    void setThumbnail(const QString& coverId, const QPixmap& px);
    void setCurrentUrl(const QString& url) { currentUrl_ = url; }

private:
    QMap<QString, QPixmap> thumbCache_;
    QString currentUrl_;

    static const int THUMB_W = 52;
    static const int THUMB_H = 52;
    static const int ITEM_H  = 64;
    static const int PAD     = 8;
};

// ── OriginalsWidget ───────────────────────────────────────────────────────────
class OriginalsWidget : public QWidget {
    Q_OBJECT
public:
    explicit OriginalsWidget(QWidget* parent = nullptr);
    ~OriginalsWidget() override = default;

    // 현재 재생 중인 파일 URL이 변경될 때 호출 → 목록 하이라이트 갱신
    void setCurrentFile(const QString& fileUrl);
    // 설정에서 API URL 변경 시 호출
    void setApiUrl(const QString& url);

signals:
    // 곡 URL을 MainWindow에 전달하여 재생 요청
    void playRequested(const QString& url);
    // 전체 재생 (M3U URL)
    void playlistRequested(const QString& m3uUrl);

private slots:
    void fetchSongs();
    void onFetchFinished(QNetworkReply* reply);
    void onThumbFinished(QNetworkReply* reply);
    void onSearchChanged(const QString& text);
    void onCategoryClicked(const QString& category);
    void onSortChanged(int index);
    void onItemDoubleClicked(QListWidgetItem* item);
    void onPlayAllClicked();
    void onYouTubeClicked();

private:
    void setupUI();
    void applyFilter();
    void updateList();
    void fetchThumbnail(const SongInfo& song);
    void showToast(const QString& msg);

    QNetworkAccessManager* nam_          = nullptr;
    QTimer*                refreshTimer_ = nullptr;
    QSettings              settings_;

    QString   apiUrl_  = "https://sorinuri.com/api/songs.json";
    QString   m3uUrl_  = "https://sorinuri.com/api/sorinuri_originals.m3u";
    QString   baseUrl_ = "https://sorinuri.com";

    QJsonArray      rawSongs_;
    QList<SongInfo> songs_;
    QList<SongInfo> filtered_;
    int             lastSongCount_ = -1;

    QString   currentFile_;
    QString   activeCategory_ = "전체";
    QString   searchText_;
    int       sortIndex_      = 0;

    // UI
    QLineEdit*           searchEdit_   = nullptr;
    QLabel*              statusLabel_  = nullptr;
    QListWidget*         listWidget_   = nullptr;
    QComboBox*           sortCombo_    = nullptr;
    QLabel*              toastLabel_   = nullptr;
    QTimer*              toastTimer_   = nullptr;
    QWidget*             catBar_       = nullptr;
    QList<QPushButton*>  catBtns_;
    SongItemDelegate*    delegate_     = nullptr;

    static const QStringList CATEGORIES;
};
