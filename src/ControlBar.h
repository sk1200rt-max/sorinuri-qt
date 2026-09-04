#pragma once
#include <QWidget>
#include <QSlider>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QVector>
#include <QMouseEvent>
#include <QStyleOptionSlider>
#include "TrackSelector.h"
#include "MpvCore.h"

struct ChapterMark { double startSec; QString title; };

/**
 * ClickSeekSlider - HiDPI 250% 환경에서 클릭 즉시 이동 지원
 * QSlider 기본 동작(pageStep 이동)을 오버라이드하여
 * 클릭한 위치로 즉시 이동하도록 구현
 */
class ClickSeekSlider : public QSlider {
    Q_OBJECT
public:
    explicit ClickSeekSlider(Qt::Orientation o, QWidget* parent = nullptr)
        : QSlider(o, parent) {}
protected:
    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton) {
            const int val = valueAtPos(e->pos());
            setValue(val);
            emit sliderMoved(val);
            emit sliderPressed();
            e->accept();
            return;
        }
        QSlider::mousePressEvent(e);
    }
    void mouseMoveEvent(QMouseEvent* e) override {
        if (e->buttons() & Qt::LeftButton) {
            const int val = valueAtPos(e->pos());
            setValue(val);
            emit sliderMoved(val);
            e->accept();
            return;
        }
        QSlider::mouseMoveEvent(e);
    }
    void mouseReleaseEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton) {
            const int val = valueAtPos(e->pos());
            setValue(val);
            emit sliderMoved(val);
            emit sliderReleased();
            e->accept();
            return;
        }
        QSlider::mouseReleaseEvent(e);
    }
private:
    int valueAtPos(const QPoint& pos) const {
        QStyleOptionSlider opt;
        initStyleOption(&opt);
        const QRect groove = style()->subControlRect(QStyle::CC_Slider, &opt,
                                                     QStyle::SC_SliderGroove, this);
        const QRect handle = style()->subControlRect(QStyle::CC_Slider, &opt,
                                                     QStyle::SC_SliderHandle, this);
        const int sliderLength = orientation() == Qt::Horizontal ? handle.width() : handle.height();
        const int sliderMin = orientation() == Qt::Horizontal ? groove.x() : groove.y();
        const int sliderMax = orientation() == Qt::Horizontal
            ? groove.right() - sliderLength + 1
            : groove.bottom() - sliderLength + 1;
        const int point = orientation() == Qt::Horizontal ? pos.x() : pos.y();
        return QStyle::sliderValueFromPosition(
            minimum(), maximum(), point - sliderLength / 2 - sliderMin,
            sliderMax - sliderMin, opt.upsideDown);
    }
};

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
    void setChapters(const QVector<ChapterMark>& chapters, double duration);

signals:
    void playPauseClicked();
    void stopClicked();
    void seeked(double seconds);
    void volumeChanged(int vol);
    void muteToggled(bool muted);
    void prevClicked();
    void nextClicked();

public slots:
    void onAudioFormatChanged(const QString& codec, int channels,
                              int sampleRate, const QString& output);
    void onVideoInfoChanged(int width, int height, double fps, const QString& codec);
    void onPlaybackStopped();

private slots:
    void onSeekMoved(int value);
    void onVolMoved(int value);

private:
    static QPushButton* makeBtn(const QString& svg, const QString& tip, int size = 30);
    QString formatTime(double s) const;
    QString getDisplayCodec(const QString& codec) const;
    QString formatChannels(int ch) const;

    ClickSeekSlider* seekSlider_ = nullptr;
    QLabel*      timeLabel_  = nullptr;
    QPushButton* btnPrev_    = nullptr;
    QPushButton* btnPlay_    = nullptr;
    QPushButton* btnNext_    = nullptr;
    QPushButton* btnStop_    = nullptr;
    QPushButton* btnMute_    = nullptr;
    QSlider*     volSlider_  = nullptr;
    QLabel*      volLabel_   = nullptr;
    QHBoxLayout* transportRow_ = nullptr;
    QHBoxLayout* trackRow_     = nullptr;
    QLabel*      audioInfoLabel_ = nullptr;

    double totalDuration_ = 0;
    bool   seeking_       = false;
    bool   muted_         = false;
    QVector<ChapterMark> chapters_;
};
