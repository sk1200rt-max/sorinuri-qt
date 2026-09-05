#pragma once
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QMouseEvent>

class TitleBar : public QWidget {
    Q_OBJECT
public:
    enum class Service {
        Player,
        Ott,
        Originals
    };

    explicit TitleBar(QWidget* parent = nullptr);
    void setTitle(const QString& title);
    void setAudioBadge(const QString& codec);
    void setFullscreenMode(bool fs);
    void setAlwaysOnTop(bool pinned);
    void setActiveService(Service service);

    // Windows 무테 창 hit test: 시스템 드래그 영역과 실제 버튼 영역을 구분한다.
    // 모든 커스텀 버튼은 Qt 클릭 처리로만 동작해 hover Snap Layout을 열지 않는다.
    bool isInteractiveControlAt(const QPoint& localPos) const;

signals:
    void minimizeClicked();
    void maximizeClicked();
    void fullscreenClicked();
    void closeClicked();
    void alwaysOnTopToggled(bool pinned);

    // 앱 전체에서 항상 같은 위치를 사용하는 상단 서비스·명령 진입점.
    void playerServiceClicked();
    void ottServiceClicked();
    void originalsServiceClicked();
    void openFileClicked();
    void toolsClicked();

protected:
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;

private:
    static QPushButton* makeIconBtn(const QString& svgPath, const QString& tooltip,
                                    const QString& hoverBg, int w = 46);
    static QPushButton* makeCommandBtn(const QString& text, const QString& tooltip);
    static QPushButton* makeServiceBtn(const QString& text, const QString& tooltip);
    void refreshServiceButtons();
    void updateResponsiveLayout();

    QLabel*      titleLabel_    = nullptr;
    QLabel*      badgeLabel_    = nullptr;
    QPushButton* btnPlayer_     = nullptr;
    QPushButton* btnOtt_        = nullptr;
    QPushButton* btnOriginals_  = nullptr;
    QPushButton* btnOpen_       = nullptr;
    QPushButton* btnTools_      = nullptr;
    QPushButton* btnPin_        = nullptr;
    QPushButton* btnMin_        = nullptr;
    QPushButton* btnMax_        = nullptr;
    QPushButton* btnFullscreen_ = nullptr;
    QPushButton* btnClose_      = nullptr;

    Service activeService_ = Service::Player;
    bool   dragging_  = false;
    bool   pinned_    = false;
    bool   compactLayout_ = false;
    QPoint dragStart_;
};
