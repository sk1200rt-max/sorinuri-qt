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
#include "SettingsDialog.h"
#include "TrackSelector.h"
#include "YtdlpManager.h"
#include "OttWidget.h"
#include "ProFeaturesWidget.h"
#include "ShortcutOverlay.h"
#include "ShortcutConfigWidget.h"
#include "ScreenRecorder.h"
#include "MusicWidget.h"
#include "HiFiEngine.h"
#include "MiniPlayerWidget.h"
#include "CompactPlayerWidget.h"
#include "WhisperWidget.h"
#include "UpscaleWidget.h"
#include "ChapterWidget.h"
#include "MultiViewWidget.h"
#include "SubtitleSearchDialog.h"
#include "AudioAdvancedWidget.h"
#include "VideoAdvancedWidget.h"
#include "NetworkBrowserWidget.h"
#include "OsdWidget.h"
#include "MediaLibraryWidget.h"
#include "SubtitleEditorWidget.h"
#include "AdManager.h"
#include "SplashAdWidget.h"
#include "OriginalsWidget.h"
#include "PlaybackQueue.h"
#include "SMTCManager.h"
#include "ScrobbleManager.h"
#include "CloudDriveManager.h"
#include "CloudDriveBrowserWidget.h"
#include "VoiceControlWidget.h"
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>


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
    bool eventFilter(QObject* obj, QEvent* event) override;

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
    void onSubtitleSearch();
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
    void toggleCompactPlayer();   // 소형 모드 (AIMP 스타일 세로 창)
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
    void playQueue(const QList<PlaybackQueue::Entry>& entries, int startIndex = 0);
    void applyQueueRepeatMode();
    void loadMusicMeta(const QString& path);
    void showUI();
    void hideUI();

    // 이어보기 (재생 위치 저장/복원)
    void saveResumePosition();
    void clearResumePosition(const QString& path);
    void tryResumePosition(const QString& path);
    // 최근 파일 목록
    void addToRecentFiles(const QString& path);
    void showRecentFilesMenu();

    // ── 메인 스택 (플레이어 / OTT 모드) ──────────────
    QStackedWidget* mainStack_   = nullptr;
    QWidget*        playerPage_  = nullptr;
    OttWidget*      ottPage_     = nullptr;

    // ── 플레이어 페이지 내부 스택 (영상 / 음악) ───────────
    QStackedWidget* playerStack_  = nullptr;
    QWidget*        videoPage_    = nullptr;
    MusicWidget*    musicPage_    = nullptr;

    // 위젯
    TitleBar*          titleBar_        = nullptr;
    MpvWidget*         mpvWidget_       = nullptr;
    TrackSelector*     trackSelector_   = nullptr;
    ControlBar*        controlBar_      = nullptr;
    ProFeaturesWidget* proFeatures_     = nullptr;
    ShortcutOverlay*   shortcutOverlay_  = nullptr;

    // HiFi 엔진
    HiFiEngine*        hifiEngine_      = nullptr;
    MiniPlayerWidget*  miniPlayer_      = nullptr;
    CompactPlayerWidget* compactPlayer_ = nullptr;  // 소형 모드 창
    WhisperWidget*     whisperWidget_   = nullptr;
    UpscaleWidget*     upscaleWidget_   = nullptr;
    ChapterWidget*     chapterWidget_   = nullptr;
    MultiViewWidget*   multiViewWidget_    = nullptr;
    AudioAdvancedWidget* audioAdvancedWidget_ = nullptr;  // 하이엔드 오디오 (컨볼루션/VST)
    VideoAdvancedWidget* videoAdvancedWidget_ = nullptr;  // 하이엔드 비디오 (3D LUT)
    NetworkBrowserWidget* networkBrowserWidget_ = nullptr;  // SMB/DLNA + 360도 VR + 캐스팅
    OsdWidget*           osdWidget_            = nullptr;  // 화면 중앙 OSD
    MediaLibraryWidget*  mediaLibrary_         = nullptr;  // 스마트 미디어 라이브러리
    SubtitleEditorWidget* subtitleEditor_      = nullptr;  // 자막 편집기
    QWidget*              statsWidget_          = nullptr;  // 재생 통계/최근 감상 화면
    OriginalsWidget*      originalsWidget_      = nullptr;  // SORINURI ORIGINALS 탭
    PlaybackQueue*        playbackQueue_        = nullptr;  // 오리지널·YouTube·로컬 통합 대기열
    AdManager*            adManager_            = nullptr;  // 광고 관리자
    SplashAdWidget*       splashAdWidget_       = nullptr;  // 시작 화면 광고
    SMTCManager*          smtcManager_          = nullptr;  // Windows 잠금 화면 미디어 컨트롤
    ShortcutConfigWidget* shortcutConfigWidget_  = nullptr;  // 단축키 커스터마이징 UI
    ScreenRecorder*       screenRecorder_        = nullptr;  // 화면 녹화
    VoiceControlWidget*   voiceControlWidget_    = nullptr;  // AI 음성 명령 제어
    ScrobbleManager*      scrobbleManager_       = nullptr;  // Last.fm 스크로블링
    CloudDriveManager*         cloudDriveManager_         = nullptr;  // 클라우드 드라이브 연동
    CloudDriveBrowserWidget*   cloudDriveBrowserWidget_   = nullptr;  // 클라우드 드라이브 UI

    bool   isFullscreen_      = false;
    bool   isOttMode_         = false;
    bool   isMusicMode_       = false;
    bool   isProFeaturesOpen_ = false;
    bool   isPlaying_         = false;
    bool   uiVisible_         = true;
    QTimer* uiHideTimer_      = nullptr;
    double totalDuration_     = 0;
    double lastPosition_      = 0;   // 이어보기용 현재 재생 위치
    QString currentFilePath_;
    QSettings settings_;
    bool pendingQueueEndUiUpdate_ = false;

    // Windows 작업표시줄 진행률 표시 (ITaskbarList3)
    void* taskbarList_  = nullptr;  // ITaskbarList3* (void*로 선언하여 헤더 의존성 제거)
    void  updateTaskbarProgress(double pos, double dur, bool paused, bool stopped);
    void  initTaskbarList();

    // yt-dlp 관리자
    YtdlpManager* ytdlp_ = nullptr;

    // 스마트폰 리모컨 (QTcpServer 기반 HTTP 서버)
    QTcpServer*   remoteServer_    = nullptr;
    bool          remoteEnabled_   = false;
    void          startRemoteServer();
    void          stopRemoteServer();
    void          handleRemoteRequest(QTcpSocket* socket);
    QString pendingUrl_;
    QList<PlaybackQueue::Entry> pendingYouTubeQueue_;
    int pendingYouTubeQueueStartIndex_ = 0;
    QProgressDialog* ytdlpProgress_ = nullptr;

    // HiDPI 근본 수정: 시작 파일 대기 큐
    // main.cpp에서 window.show() 직후 openFiles() 호출 시
    // initializeGL()이 아직 실행되지 않아 loadFile이 무시되는 문제 해결
    // mpvInitialized 시그널 수신 후 이 큐를 처리
    QStringList pendingStartupFiles_;

    // 창 크기 조절
    bool   resizing_    = false;
    QPoint resizeStart_;
    QSize  resizeStartSize_;
    int    resizeEdge_  = 0;
    static const int RESIZE_MARGIN = 6;
    int getResizeEdge(const QPoint& pos) const;
};
