#pragma once
#include <QWidget>
#include <QSlider>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include "TrackSelector.h"

class ControlBar : public QWidget {
    Q_OBJECT
public:
    explicit ControlBar(QWidget* parent = nullptr);

    void setPlaying(bool playing);
    void setPosition(double pos, double dur);
    void setDuration(double dur);
    void setVolume(int vol);
    void setMuted(bool muted);
    void embedTrackSelector(TrackSelector* selector);

signals:
    void playPauseClicked();
    void stopClicked();
    void seeked(double seconds);
    void volumeChanged(int vol);
    void muteToggled(bool muted);
    void openFileClicked();
    void prevClicked();
    void nextClicked();
    void settingsClicked();

private slots:
    void onSeekMoved(int value);
    void onVolMoved(int value);

private:
    static QPushButton* makeBtn(const QString& svg, const QString& tip, int size = 30);
    QString formatTime(double s) const;

    QSlider*     seekSlider_ = nullptr;
    QLabel*      timeLabel_  = nullptr;
    QPushButton* btnOpen_    = nullptr;
    QPushButton* btnPrev_    = nullptr;
    QPushButton* btnPlay_    = nullptr;
    QPushButton* btnNext_    = nullptr;
    QPushButton* btnStop_    = nullptr;
    QPushButton* btnMute_    = nullptr;
    QSlider*     volSlider_  = nullptr;
    QLabel*      volLabel_   = nullptr;
    QPushButton* btnSettings_ = nullptr;
    QHBoxLayout* btnRow_     = nullptr;  // 중앙 TrackSelector 삽입용

    double totalDuration_ = 0;
    bool   seeking_       = false;
    bool   muted_         = false;
};
