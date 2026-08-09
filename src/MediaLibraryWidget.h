#pragma once
#include <QWidget>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QProgressBar>
#include <QTimer>
#include <QThread>
#include <QMutex>
#include <QStringList>
#include <QHash>
#include <QPixmap>
#include <QProcess>

/**
 * MediaLibraryWidget - 스마트 미디어 라이브러리
 *
 * - 지정 폴더 스캔 → 포스터/앨범아트 그리드 뷰
 * - SQLite 기반 인덱싱 (빠른 검색/필터)
 * - 비디오/음악 탭 분리
 * - AI 오디오 태깅 (BPM, 분위기, 장르 자동 분석)
 * - 더블클릭으로 재생
 */
class MediaLibraryWidget : public QWidget {
    Q_OBJECT
public:
    explicit MediaLibraryWidget(QWidget* parent = nullptr);
    ~MediaLibraryWidget() override;

    void addFolder(const QString& path);
    void refresh();

signals:
    void fileRequested(const QString& path);

private slots:
    void onAddFolder();
    void onSearch(const QString& text);
    void onTabChanged(int idx);
    void onItemDoubleClicked(QListWidgetItem* item);
    void onScanProgress(int current, int total);
    void onScanFinished(const QStringList& videoFiles, const QStringList& audioFiles);
    void onThumbnailLoaded(const QString& path, const QPixmap& thumb);
    // AI 오디오 태깅
    void onAnalyzeAll();
    void onAnalyzeFinished(int exitCode, QProcess::ExitStatus status);

private:
    void setupUI();
    void setupDatabase();
    void scanFolder(const QString& path);
    void populateList(QListWidget* list, const QStringList& files, const QString& filter = {});
    void loadThumbnailAsync(const QString& path, QListWidgetItem* item);
    QPixmap makeThumbnailFromAlbumArt(const QString& path);
    QString dbPath() const;
    // AI 태깅 헬퍼
    void analyzeNextFile();
    QString inferMood(double bpm, const QString& genre) const;

    QTabWidget*   tabs_        = nullptr;
    QListWidget*  videoList_   = nullptr;
    QListWidget*  audioList_   = nullptr;
    QLineEdit*    searchEdit_  = nullptr;
    QProgressBar* progressBar_ = nullptr;
    QLabel*       statusLabel_ = nullptr;
    QPushButton*  btnAnalyze_  = nullptr;  // AI 태깅 버튼

    // AI 태깅 결과 필터 (v6.18.0 신규)
    QComboBox*    moodFilterCombo_  = nullptr;
    QComboBox*    bpmFilterCombo_   = nullptr;
    QComboBox*    genreFilterCombo_ = nullptr;

    QStringList   videoFiles_;
    QStringList   audioFiles_;
    QStringList   folders_;

    QThread*      scanThread_  = nullptr;
    bool          scanning_    = false;
    QHash<QString, QPixmap> thumbCache_;  // 썸네일 캐시 (path → pixmap)

    // AI 태깅 상태
    QProcess*     analyzeProcess_   = nullptr;
    QStringList   analyzeQueue_;     // 분석 대기 파일 목록
    int           analyzeTotal_     = 0;
    int           analyzeDone_      = 0;
};
