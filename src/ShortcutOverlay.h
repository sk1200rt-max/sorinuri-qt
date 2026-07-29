#pragma once
#include <QWidget>
#include <QLabel>
#include <QKeyEvent>

/**
 * ShortcutOverlay - 단축키 도움말 오버레이
 *
 * '?' 키를 누르면 영상 위에 반투명 오버레이로 단축키 목록을 표시합니다.
 * 아무 키나 누르거나 클릭하면 닫힙니다.
 */
class ShortcutOverlay : public QWidget {
    Q_OBJECT
public:
    explicit ShortcutOverlay(QWidget* parent = nullptr);
    void toggle();

protected:
    void paintEvent(QPaintEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
};
