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

#include "MpvWidget.h"
#include "ControlBar.h"
#include "PlaylistWidget.h"
#include "TitleBar.h"

/**
 * MainWindow - 소리누리 메인 윈도우
 *
 * 레이아웃:
 * ┌─────────────────────────────────────────┐
 * │  TitleBar (커스텀 타이틀바)              │
 * ├──────────┬──────────────────────────────┤
 * │          │                              │
 * │ Playlist │      MpvWidget               │
 * │  (사이드) │      (비디오 영역)           │
 * │          │                              │
 * ├──────────┴──────────────────────────────┤
 * │  ControlBar (재생 컨트롤)               │
 * ├─────────────────────────────────────────┤
 * │  StatusBar (오디오 포맷 정보)            │
 * └─────────────────────────────────────────┘
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
    void onPositionChanged(double seconds);
    void onDurationChanged(double seconds);
    void onVolumeChanged(int vol);
    void onAudioFormatChanged(const QString& codec, int channels,
                              int sampleRate, const QString& output);
    void onTracksChanged();

    void onOpenFile();
    void onPlaylistItemDoubleClicked(int index);
    void toggleFullscreen();
    void togglePlayPause();

private:
    void setupUI();
    void setupMenuBar();
    void setupConnections();
    void loadSettings();
    void saveSettings();
    void updateWindowTitle(const QString& filename = {});
    void updateAudioBadge(const QString& codec);

    // 위젯
    TitleBar*      titleBar_    = nullptr;
    QSplitter*     splitter_    = nullptr;
    PlaylistWidget* playlist_   = nullptr;
    MpvWidget*     mpvWidget_   = nullptr;
    ControlBar*    controlBar_  = nullptr;
    QLabel*        statusLabel_ = nullptr;

    // 상태
    bool     isFullscreen_  = false;
    double   totalDuration_ = 0;
    QSettings settings_;
};
