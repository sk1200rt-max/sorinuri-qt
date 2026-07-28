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

signals:
    void minimizeClicked();
    void maximizeClicked();
    void closeClicked();

protected:
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;

private:
    QLabel*      logoLabel_  = nullptr;
    QLabel*      badgeLabel_ = nullptr;
    QLabel*      titleLabel_ = nullptr;
    QPushButton* btnMin_     = nullptr;
    QPushButton* btnMax_     = nullptr;
    QPushButton* btnClose_   = nullptr;

    bool   dragging_ = false;
    QPoint dragStart_;
};
