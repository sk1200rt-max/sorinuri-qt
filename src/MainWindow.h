#pragma once

#include <QMainWindow>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QSlider>
#include <QPushButton>
#include <QToolButton>
#include <QComboBox>
#include <QStatusBar>
#include <QTimer>
#include <QSettings>
#include <QStringList>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QCloseEvent>

#include "MpvWidget.h"
#include "ControlBar.h"
#include "PlaylistWidget.h"
#include "TitleBar.h"
#include "AudioInfoBar.h"
#include "SettingsDialog.h"
#include "TrackSelector.h"

/**
 * MainWindow - 소리누리 메인 윈도우
 *
 * 레이아웃:
 * ┌─────────────────────────────────────────────────────┐
 * │  TitleBar (커스텀 타이틀바 + 오디오 배지)            │
 * ├──────────┬──────────────────────────────────────────┤
 * │          │                                          │
 * │ Playlist │      MpvWidget (비디오 영역)              │
 * │  (사이드) │                                          │
 * │          │                                          │
 * ├──────────┴──────────────────────────────────────────┤
 * │  TrackSelector (오디오/자막 트랙 선택)               │
 * ├─────────────────────────────────────────────────────┤
 * │  ControlBar (재생 컨트롤 + 진행바)                   │
 * ├─────────────────────────────────────────────────────┤
 * │  AudioInfoBar (포맷 배지 + 채널 레벨 미터 + 비디오 정보) │
 * └─────────────────────────────────────────────────────┘
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    void openFiles(const QStringList& paths);

protected:
    void closeEvent(QCloseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private slots:
    void onFileLoaded(const QString& path);
    void onPlaybackStarted();
    void onPlaybackPaused();
    void onPlaybackEnded();
    void onPlaybackStopped();
    void onPositionChanged(double seconds);
    void onDurationChanged(double seconds);
    void onVolumeChanged(int vol);
    void onAudioFormatChanged(const QString& codec, int channels,
                              int sampleRate, const QString& output);
    void onVideoInfoChanged(int width, int height, double fps, const QString& codec);
    void onTracksChanged();
    void onErrorOccurred(const QString& message);

    void onOpenFile();
    void onPlaylistItemDoubleClicked(int index);
    void onSettingsRequested();
    void toggleFullscreen();
    void togglePlayPause();
    void showStatsOverlay();

private:
    void setupUI();
    void setupConnections();
    void loadSettings();
    void saveSettings();
    void updateWindowTitle(const QString& filename = {});
    void updateAudioBadge(const QString& codec);
    QString formatTime(double seconds) const;

    // 위젯
    TitleBar*       titleBar_      = nullptr;
    QSplitter*      splitter_      = nullptr;
    PlaylistWidget* playlist_      = nullptr;
    MpvWidget*      mpvWidget_     = nullptr;
    TrackSelector*  trackSelector_ = nullptr;
    ControlBar*     controlBar_    = nullptr;
    AudioInfoBar*   audioInfoBar_  = nullptr;

    // 통계 오버레이
    QLabel*         statsOverlay_  = nullptr;
    QTimer*         statsTimer_    = nullptr;

    // 상태
    bool     isFullscreen_  = false;
    double   totalDuration_ = 0;
    QSettings settings_;
};
