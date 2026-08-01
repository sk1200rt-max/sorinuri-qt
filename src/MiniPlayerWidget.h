#pragma once
#include <QWidget>
#include <QLabel>
#include <QSlider>
#include <QPushButton>
#include <QPixmap>
#include <QTimer>

class MpvCore;

/**
 * MiniPlayerWidget - 항상 위에 고정되는 콜팟트 음악 플레이어 바
 *
 * 크기: 300~700px (사용자 조절 가능)
 * 위치: 화면 우측 하단 (항상 위에 고정)
 * 단축키: M 키로 메인 창에서 토글
 * 마우스 오버 시 콘트롤 버튼 표시
 */
class MiniPlayerWidget : public QWidget {
    Q_OBJECT
public:
    explicit MiniPlayerWidget(QWidget* parent = nullptr);

    void setMeta(const QString& title, const QString& artist,
                 const QPixmap& art, const QString& codec, int bitDepth);
    void updatePosition(double pos, double duration);
    void setPlaying(bool playing);

signals:
    void playPauseRequested();
    void prevRequested();
    void nextRequested();
    void expandRequested();   // 메인 창으로 돌아가기
    void closeRequested();    // 미니 플레이어 닫기 (메인 창 복귀)
    void seekRequested(double pos);

protected:
    void paintEvent(QPaintEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void enterEvent(QEnterEvent* e) override;
    void leaveEvent(QEvent* e) override;

private:
    void setupUI();

    QLabel*      albumArtLabel_ = nullptr;
    QLabel*      titleLabel_    = nullptr;
    QLabel*      artistLabel_   = nullptr;
    QLabel*      codecLabel_    = nullptr;
    QSlider*     seekSlider_    = nullptr;
    QLabel*      timeLabel_     = nullptr;
    QPushButton* btnPrev_       = nullptr;
    QPushButton* btnPlay_       = nullptr;
    QPushButton* btnNext_       = nullptr;
    QPushButton* btnExpand_     = nullptr;

    // 마우스 오버 시 표시되는 콘트롤 컨테이너
    QWidget*     ctrlWidget_         = nullptr;
    QTimer*      controlHideTimer_   = nullptr;

    bool   isPlaying_  = false;
    double duration_   = 0;

    // 드래그 이동
    bool   dragging_   = false;
    QPoint dragOffset_;
};
