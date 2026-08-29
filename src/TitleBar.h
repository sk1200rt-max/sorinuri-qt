#pragma once
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QRect>

class TitleBar : public QWidget {
    Q_OBJECT
public:
    explicit TitleBar(QWidget* parent = nullptr);
    void setTitle(const QString& title);
    void setAudioBadge(const QString& codec);
    void setFullscreenMode(bool fs);
    void setAlwaysOnTop(bool pinned);
    // Windows Snap Layout이 사용자 지정 최대화 버튼을 인식하도록
    // MainWindow의 WM_NCHITTEST 처리에서 사용할 논리 좌표 영역을 반환한다.
    QRect maximizeButtonRectInWindow() const;

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
