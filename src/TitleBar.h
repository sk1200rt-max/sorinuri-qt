#pragma once
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QMouseEvent>

class TitleBar : public QWidget {
    Q_OBJECT
public:
    explicit TitleBar(QWidget* parent = nullptr);
    void setTitle(const QString& title);
    void setAudioBadge(const QString& codec);
    void setFullscreenMode(bool fs);
    void setAlwaysOnTop(bool pinned);

    // Windows 무테 창 hit test: 시스템 드래그 영역과 실제 버튼 영역을 구분한다.
    // 최대화 버튼은 HTMAXBUTTON으로 반환해 Windows 11 Snap Layout을 제공한다.
    bool isInteractiveControlAt(const QPoint& localPos) const;
    bool isMaximizeControlAt(const QPoint& localPos) const;

signals:
    void minimizeClicked();
    void maximizeClicked();
    void fullscreenClicked();
    void closeClicked();
    void alwaysOnTopToggled(bool pinned);

protected:
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;

private:
    static QPushButton* makeIconBtn(const QString& svgPath, const QString& tooltip,
                                    const QString& hoverBg, int w = 46);

    QLabel*      titleLabel_    = nullptr;
    QLabel*      badgeLabel_    = nullptr;
    QPushButton* btnPin_        = nullptr;
    QPushButton* btnMin_        = nullptr;
    QPushButton* btnMax_        = nullptr;
    QPushButton* btnFullscreen_ = nullptr;
    QPushButton* btnClose_      = nullptr;

    bool   dragging_  = false;
    bool   pinned_    = false;
    QPoint dragStart_;
};
