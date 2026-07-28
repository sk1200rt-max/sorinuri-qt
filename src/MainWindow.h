#pragma once

#include <QMainWindow>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QPushButton>
#include <QTimer>
#include <QSettings>
#include <QStringList>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QCloseEvent>
#include <QResizeEvent>

#include "MpvWidget.h"
#include "ControlBar.h"
#include "PlaylistWidget.h"
#include "TitleBar.h"
#include "AudioInfoBar.h"
#include "SettingsDialog.h"
#include "TrackSelector.h"

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

    // 창 크기 조절 (프레임리스 윈도우)
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

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
    void showStatsOverlay();

private:
    void setupUI();
    void setupConnections();
    void loadSettings();
    void saveSettings();
    void updateWindowTitle(const QString& filename = {});
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

    // 창 크기 조절
    bool   resizing_    = false;
    QPoint resizeStart_;
    QSize  resizeStartSize_;
    int    resizeEdge_  = 0;  // 0=없음, 1=좌, 2=우, 3=상, 4=하, 5=좌상, 6=우상, 7=좌하, 8=우하
    static const int RESIZE_MARGIN = 6;
    int getResizeEdge(const QPoint& pos) const;
};
