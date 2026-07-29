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
#include <QStackedWidget>
#include <QPushButton>

#include "MpvWidget.h"
#include "ControlBar.h"
#include "TitleBar.h"
#include "AudioInfoBar.h"
#include "SettingsDialog.h"
#include "TrackSelector.h"
#include "YtdlpManager.h"
#include "OttWidget.h"
#include "ProFeaturesWidget.h"
#include "ShortcutOverlay.h"
#include "MusicWidget.h"
#include "HiFiEngine.h"
#include "MiniPlayerWidget.h"
#include "WhisperWidget.h"
#include "UpscaleWidget.h"
#include "ChapterWidget.h"
#include "MultiViewWidget.h"


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
    void resizeEvent(QResizeEvent* e) override;

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
    void onOpenUrl();
    void onSettingsRequested();
    void toggleFullscreen();
    void showContextMenu(const QPoint& pos);
    void toggleProFeatures();

    // 모드 전환
    void switchToPlayerMode();
    void switchToOttMode();
    void switchToMusicMode();
    void switchToVideoMode();

    // 음악 모드 관련
    void onMusicSeekRequested(double pos);
    void onMusicVolumeChanged(int vol);
    void toggleMiniPlayer();
    void toggleWhisper(bool on);
    void onChapterBookmark();
    void toggleMultiView(MultiViewLayout l);

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
    void openUrl(const QString& url);
    void loadMusicMeta(const QString& path);

    // ── 메인 스택 (플레이어 / OTT 모드) ──────────────
    QStackedWidget* mainStack_   = nullptr;
    QWidget*        playerPage_  = nullptr;
    OttWidget*      ottPage_     = nullptr;

    // 모드 전환 버튼
    QPushButton* playerModeBtn_ = nullptr;
    QPushButton* ottModeBtn_    = nullptr;

    // ── 플레이어 페이지 내부 스택 (영상 / 음악) ───────────
    QStackedWidget* playerStack_  = nullptr;
    QWidget*        videoPage_    = nullptr;
    MusicWidget*    musicPage_    = nullptr;

    // 위젯
    TitleBar*          titleBar_        = nullptr;
    MpvWidget*         mpvWidget_       = nullptr;
    TrackSelector*     trackSelector_   = nullptr;
    ControlBar*        controlBar_      = nullptr;
    AudioInfoBar*      audioInfoBar_    = nullptr;
    ProFeaturesWidget* proFeatures_     = nullptr;
    ShortcutOverlay*   shortcutOverlay_ = nullptr;

    // HiFi 엔진
    HiFiEngine*        hifiEngine_      = nullptr;
    MiniPlayerWidget*  miniPlayer_      = nullptr;
    WhisperWidget*     whisperWidget_   = nullptr;
    UpscaleWidget*     upscaleWidget_   = nullptr;
    ChapterWidget*     chapterWidget_   = nullptr;
    MultiViewWidget*   multiViewWidget_ = nullptr;

    bool   isFullscreen_      = false;
    bool   isOttMode_         = false;
    bool   isMusicMode_       = false;
    bool   isProFeaturesOpen_ = false;
    double totalDuration_     = 0;
    QString currentFilePath_;
    QSettings settings_;

    // yt-dlp 관리자
    YtdlpManager* ytdlp_ = nullptr;
    QString pendingUrl_;
    QProgressDialog* ytdlpProgress_ = nullptr;

    // 창 크기 조절
    bool   resizing_    = false;
    QPoint resizeStart_;
    QSize  resizeStartSize_;
    int    resizeEdge_  = 0;
    static const int RESIZE_MARGIN = 6;
    int getResizeEdge(const QPoint& pos) const;
};
