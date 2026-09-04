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
    // 클릭한 위치로 즉시 이동 (HiDPI 모든 배율 안전)
    // Qt6 PerMonitorV2: e->pos()는 항상 논리 픽셀 → DPR 무관하게 정확
    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton) {
            int val = valueAtPos(e->pos());
            setValue(val);
            emit sliderMoved(val);
            emit sliderPressed();
            e->accept();
            return;
        }
        QSlider::mousePressEvent(e);
    }
    // 드래그 중 실시간 업데이트
    void mouseMoveEvent(QMouseEvent* e) override {
        if (e->buttons() & Qt::LeftButton) {
            int val = valueAtPos(e->pos());
            setValue(val);
            emit sliderMoved(val);
            e->accept();
            return;
        }
        QSlider::mouseMoveEvent(e);
    }
    // 릴리즈 시 최종 값 확정
    void mouseReleaseEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton) {
            int val = valueAtPos(e->pos());
            setValue(val);
            emit sliderMoved(val);
            emit sliderReleased();
            e->accept();
            return;
        }
        QSlider::mouseReleaseEvent(e);
    }
private:
    // 마우스 위치 → 슬라이더 값 변환 (논리 픽셀 기준, HiDPI 안전)
    int valueAtPos(const QPoint& pos) const {
        QStyleOptionSlider opt;
        initStyleOption(&opt);
        QRect groove = style()->subControlRect(QStyle::CC_Slider, &opt,
                                                QStyle::SC_SliderGroove, this);
        QRect handle = style()->subControlRect(QStyle::CC_Slider, &opt,
                                                QStyle::SC_SliderHandle, this);
        int sliderMin, sliderMax, sliderLength;
        if (orientation() == Qt::Horizontal) {
            sliderLength = handle.width();
            sliderMin = groove.x();
            sliderMax = groove.right() - sliderLength + 1;
        } else {
            sliderLength = handle.height();
            sliderMin = groove.y();
            sliderMax = groove.bottom() - sliderLength + 1;
        }
        int p = orientation() == Qt::Horizontal
                ? pos.x() - sliderLength / 2
                : pos.y() - sliderLength / 2;
        return QStyle::sliderValueFromPosition(
            minimum(), maximum(), p - sliderMin,
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
    // 타임라인에 챕터 마커 표시
    void setChapters(const QVector<ChapterMark>& chapters, double duration);

    // 모드 버튼 접근자
    QPushButton* playerModeBtn() const { return btnPlayerMode_; }
    QPushButton* ottModeBtn()    const { return btnOttMode_; }
    QPushButton* originalsModeBtn() const { return btnOriginalsMode_; }

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
    void originalsModeClicked();

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

    ClickSeekSlider* seekSlider_ = nullptr;
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
    QHBoxLayout* btnRow_         = nullptr;  // 재생 제어·출력 상태 행
    QHBoxLayout* trackRow_       = nullptr;  // 오디오·자막 트랙 및 모드 선택 행

    // 서비스 전환 버튼 (플레이어 / OTT / 소리누리 오리지널)
    QPushButton* btnPlayerMode_    = nullptr;
    QPushButton* btnOttMode_       = nullptr;
    QPushButton* btnOriginalsMode_ = nullptr;

    // 인라인 오디오/비디오 정보 (AudioInfoBar 대체)
    QLabel*      audioInfoLabel_ = nullptr;

    double totalDuration_ = 0;
    bool   seeking_       = false;
    bool   muted_         = false;
    QVector<ChapterMark> chapters_;  // 타임라인 챕터 마커
};
