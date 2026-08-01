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

/**
 * MediaLibraryWidget - 스마트 미디어 라이브러리
 *
 * - 지정 폴더 스캔 → 포스터/앨범아트 그리드 뷰
 * - SQLite 기반 인덱싱 (빠른 검색/필터)
 * - 비디오/음악 탭 분리
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

private:
    void setupUI();
    void setupDatabase();
    void scanFolder(const QString& path);
    void populateList(QListWidget* list, const QStringList& files, const QString& filter = {});
    QString dbPath() const;

    QTabWidget*   tabs_        = nullptr;
    QListWidget*  videoList_   = nullptr;
    QListWidget*  audioList_   = nullptr;
    QLineEdit*    searchEdit_  = nullptr;
    QProgressBar* progressBar_ = nullptr;
    QLabel*       statusLabel_ = nullptr;

    QStringList   videoFiles_;
    QStringList   audioFiles_;
    QStringList   folders_;

    QThread*      scanThread_  = nullptr;
    bool          scanning_    = false;
};
