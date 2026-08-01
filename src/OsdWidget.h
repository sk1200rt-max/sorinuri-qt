#pragma once
#include <QWidget>
#include <QLabel>
#include <QTimer>
#include <QPropertyAnimation>

/**
 * OsdWidget - 화면 중앙 반투명 OSD (On-Screen Display)
 *
 * 볼륨, 시크, 음소거, 재생속도 변경 시 화면 중앙에 잠깐 표시됨
 * 팟플레이어 스타일: 아이콘 + 텍스트 + 선택적 프로그레스바
 */
class OsdWidget : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal opacity READ windowOpacity WRITE setWindowOpacity)
public:
    enum Type { Volume, Seek, Mute, Speed, Info };

    explicit OsdWidget(QWidget* parent = nullptr);

    // 볼륨 OSD (0~200, 아이콘 + 퍼센트 + 바)
    void showVolume(int vol, bool muted = false);
    // 시크 OSD (시간 텍스트)
    void showSeek(double pos, double dur);
    // 재생속도 OSD
    void showSpeed(double speed);
    // 일반 텍스트 OSD
    void showInfo(const QString& text, int durationMs = 2000);

protected:
    void paintEvent(QPaintEvent* e) override;

private:
    void showOsd(int durationMs = 1800);
    void updateLayout();

    QLabel*  iconLabel_  = nullptr;
    QLabel*  textLabel_  = nullptr;
    QTimer*  hideTimer_  = nullptr;
    QPropertyAnimation* fadeAnim_ = nullptr;

    Type    currentType_ = Info;
    int     barValue_    = 0;   // 0~100 (볼륨/시크 바)
    bool    showBar_     = false;
};
