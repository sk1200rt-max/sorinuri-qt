#pragma once
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QDoubleSpinBox>
#include "MpvCore.h"
#include <QTabWidget>

/**
 * ProFeaturesWidget - 전문가 기능 패널
 *
 * 컨트롤바 아래에 접이식(토글)으로 표시되는 전문 기능 모음:
 * - A-B 구간 반복
 * - 재생속도 조절 (0.25x ~ 4.0x)
 * - 오디오 딜레이 조정 (-500ms ~ +500ms)
 * - 자막 딜레이 조정
 * - 화면 필터 (밝기/대비/채도/색온도)
 * - 스크린샷 저장
 */
class ProFeaturesWidget : public QWidget {
    Q_OBJECT
public:
    explicit ProFeaturesWidget(QWidget* parent = nullptr);
    void addTab(QWidget* w, const QString& title);
    void connectMpv(MpvCore* core);

    // A-B 구간 반복
    void setAbPointA();
    void setAbPointB();
    void clearAbLoop();

    // 재생속도
    void setSpeed(double speed);

    // 딜레이
    void setAudioDelay(double ms);
    void setSubDelay(double ms);

    // 화면 필터
    void resetFilters();

    // 스크린샷
    void takeScreenshot();

    // 현재 값 조회 (단축키에서 사용)
    double audioDelay() const { return audioDelaySlider_ ? audioDelaySlider_->value() : 0; }
    double subDelay()   const { return subDelaySlider_   ? subDelaySlider_->value()   : 0; }
    double currentSpeed() const { return currentSpeed_; }

signals:
    void closeRequested();  // 패널 닫기 요청 (앱 종료 아님)
    void speedChanged(double speed);
    void audioDelayChanged(double ms);
    void subDelayChanged(double ms);
    void brightnessChanged(int val);
    void contrastChanged(int val);
    void saturationChanged(int val);
    void gammaChanged(int val);

private slots:
    void onSpeedPreset(double speed);
    void onAudioDelaySlider(int val);
    void onSubDelaySlider(int val);

private:
    void updateAbLabel();

    MpvCore*    mpv_       = nullptr;
    QTabWidget* tabWidget_ = nullptr;  // 탭 컨테이너 - addTab()으로 외부 위젯 추가 가능

    // A-B 반복
    QPushButton* btnSetA_   = nullptr;
    QPushButton* btnSetB_   = nullptr;
    QPushButton* btnClearAB_= nullptr;
    QLabel*      abLabel_   = nullptr;
    double abPointA_    = -1;
    double abPointB_    = -1;
    double currentSpeed_ = 1.0;

    // 재생속도
    QLabel*      speedLabel_  = nullptr;

    // 오디오 딜레이
    QSlider*     audioDelaySlider_ = nullptr;
    QLabel*      audioDelayLabel_  = nullptr;

    // 자막 딜레이
    QSlider*     subDelaySlider_   = nullptr;
    QLabel*      subDelayLabel_    = nullptr;

    // 화면 필터
    QSlider*     brightnessSlider_ = nullptr;
    QSlider*     contrastSlider_   = nullptr;
    QSlider*     saturationSlider_ = nullptr;
    QSlider*     gammaSlider_      = nullptr;
};
