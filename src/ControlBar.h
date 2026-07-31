#pragma once
#include <QWidget>
#include <QSlider>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include "TrackSelector.h"
#include "MpvCore.h"

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
    void connectMpv(MpvCore* core);

    // 모드 버튼 접근자
    QPushButton* playerModeBtn() const { return btnPlayerMode_; }
    QPushButton* ottModeBtn()    const { return btnOttMode_; }

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
    void playerModeClicked();
    void ottModeClicked();

public slots:
    // AudioInfoBar 대체 - 인라인 오디오 정보 업데이트
    void onAudioFormatChanged(const QString& codec, int channels,
                              int sampleRate, const QString& output);
    void onVideoInfoChanged(int width, int height, double fps, const QString& codec);
    void onPlaybackStopped();

private slots:
    void onSeekMoved(int value);
    void onVolMoved(int value);

private:
    static QPushButton* makeBtn(const QString& svg, const QString& tip, int size = 30);
    static QPushButton* makeModeBtn(const QString& text, const QString& tip);
    QString formatTime(double s) const;
    QString getDisplayCodec(const QString& codec) const;
    QString formatChannels(int ch) const;

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
    QPushButton* btnSettings_    = nullptr;
    QHBoxLayout* btnRow_         = nullptr;  // 중앙 TrackSelector 삽입용

    // 모드 버튼 (파일 플레이어 / OTT)
    QPushButton* btnPlayerMode_  = nullptr;
    QPushButton* btnOttMode_     = nullptr;

    // 인라인 오디오/비디오 정보 (AudioInfoBar 대체)
    QLabel*      audioInfoLabel_ = nullptr;

    double totalDuration_ = 0;
    bool   seeking_       = false;
    bool   muted_         = false;
};
