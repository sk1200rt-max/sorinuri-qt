#pragma once
#include <QMainWindow>
#include <QVBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QSettings>
#include <QStringList>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QCloseEvent>
#include <QResizeEvent>
#include <QProgressDialog>

#include "MpvWidget.h"
#include "ControlBar.h"
#include "TitleBar.h"
#include "AudioInfoBar.h"
#include "SettingsDialog.h"
#include "TrackSelector.h"
#include "YtdlpManager.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;
    void openFiles(const QStringList& paths);

protected:
    void closeEvent(QCloseEvent* e) override;
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dropEvent(QDropEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    bool nativeEvent(const QByteArray& type, void* msg, qintptr* result) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;

private slots:
    void onFileLoaded(const QString& path);
    void onPlaybackStarted();
    void onPlaybackPaused();
    void onPlaybackEnded();
    void onPlaybackStopped();
    void onPositionChanged(double s);
    void onDurationChanged(double s);
    void onVolumeChanged(int v);
    void onAudioFormatChanged(const QString& codec, int ch, int sr, const QString& out);
    void onVideoInfoChanged(int w, int h, double fps, const QString& codec);
    void onTracksChanged();
    void onOpenFile();
    void onOpenUrl();                    // URL 열기 (유튜브 등)
    void onSettingsRequested();
    void toggleFullscreen();
    void showContextMenu(const QPoint& pos);

    // yt-dlp 관련
    void onYtdlpReady(const QString& path);
    void onYtdlpDownloadProgress(int percent);
    void onYtdlpDownloadFailed(const QString& error);

private:
    void setupUI();
    void setupConnections();
    void loadSettings();
    void saveSettings();
    void updateWindowTitle(const QString& filename = {});
    void openUrl(const QString& url);    // URL 재생 (yt-dlp 필요 시 자동 다운로드)

    // 위젯
    TitleBar*     titleBar_     = nullptr;
    MpvWidget*    mpvWidget_    = nullptr;
    TrackSelector* trackSelector_ = nullptr;
    ControlBar*   controlBar_   = nullptr;
    AudioInfoBar* audioInfoBar_ = nullptr;

    bool   isFullscreen_  = false;
    double totalDuration_ = 0;
    QSettings settings_;

    // yt-dlp 관리자
    YtdlpManager* ytdlp_ = nullptr;
    QString pendingUrl_;             // yt-dlp 다운로드 중 대기 중인 URL
    QProgressDialog* ytdlpProgress_ = nullptr;

    // 창 크기 조절
    bool   resizing_    = false;
    QPoint resizeStart_;
    QSize  resizeStartSize_;
    int    resizeEdge_  = 0;
    static const int RESIZE_MARGIN = 6;
    int getResizeEdge(const QPoint& pos) const;
};
