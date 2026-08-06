#pragma once
#include <QWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QJsonArray>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QSettings>

// ──────────────────────────────────────────────────────────────────────────────
// OriginalsWidget — SORINURI ORIGINALS 탭
//
// songs.json을 5분마다 자동 fetch하여 곡 목록을 표시합니다.
// 검색, 카테고리 필터, 정렬, 새 곡 알림, 재생 중 표시 기능을 포함합니다.
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

class OriginalsWidget : public QWidget {
    Q_OBJECT
public:
    explicit OriginalsWidget(QWidget* parent = nullptr);
    ~OriginalsWidget() override = default;

    // 현재 재생 중인 파일 경로가 변경될 때 호출 → 목록 하이라이트 갱신
    void setCurrentFile(const QString& filePath);

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
    void showToast(const QString& msg);

    QNetworkAccessManager* nam_       = nullptr;
    QTimer*               refreshTimer_ = nullptr;
    QSettings             settings_;

    QString   apiUrl_     = "https://sorinuri.com/api/songs.json";
    QString   m3uUrl_     = "https://sorinuri.com/api/sorinuri_originals.m3u";
    QString   baseUrl_    = "https://sorinuri.com";

    QJsonArray  rawSongs_;       // 서버에서 받은 원본 배열
    QList<SongInfo> songs_;      // 파싱된 전체 목록
    QList<SongInfo> filtered_;   // 현재 필터 적용 목록
    int         lastSongCount_ = -1;  // 새 곡 알림용

    QString   currentFile_;      // 현재 재생 중인 파일 URL
    QString   activeCategory_    = "전체";
    QString   searchText_;
    int       sortIndex_         = 0;

    // UI 위젯
    QLineEdit*    searchEdit_    = nullptr;
    QLabel*       statusLabel_   = nullptr;
    QListWidget*  listWidget_    = nullptr;
    QComboBox*    sortCombo_     = nullptr;
    QLabel*       toastLabel_    = nullptr;
    QTimer*       toastTimer_    = nullptr;
    QWidget*      catBar_        = nullptr;
    QList<QPushButton*> catBtns_;

    // 카테고리 목록
    static const QStringList CATEGORIES;
};
