#pragma once
#include <QWidget>
#include <QSlider>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include "MpvCore.h"

class ControlBar : public QWidget {
    Q_OBJECT
public:
    explicit ControlBar(QWidget* parent = nullptr);

    void setPlaying(bool playing);
    void setPosition(double pos, double dur);
    void setDuration(double dur);
    void setVolume(int vol);
    void updateTracks(MpvCore* core);

signals:
    void playPauseClicked();
    void stopClicked();
    void seeked(double seconds);
    void volumeChanged(int vol);
    void muteToggled(bool muted);
    void speedChanged(double speed);
    void openFileClicked();
    void fullscreenToggled();
    void prevClicked();
    void nextClicked();

private slots:
    void onSeekSliderMoved(int value);
    void onVolumeSliderMoved(int value);

private:
    QString formatTime(double seconds) const;

    // 진행바
    QSlider*     seekSlider_   = nullptr;
    QLabel*      timeLabel_    = nullptr;

    // 재생 버튼
    QPushButton* btnPrev_      = nullptr;
    QPushButton* btnPlay_      = nullptr;
    QPushButton* btnNext_      = nullptr;
    QPushButton* btnStop_      = nullptr;

    // 볼륨
    QPushButton* btnMute_      = nullptr;
    QSlider*     volSlider_    = nullptr;
    QLabel*      volLabel_     = nullptr;

    // 기타
    QComboBox*   speedCombo_   = nullptr;
    QPushButton* btnOpen_      = nullptr;
    QPushButton* btnFullscreen_= nullptr;

    double totalDuration_ = 0;
    bool   seeking_       = false;
    bool   muted_         = false;
};
