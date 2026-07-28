#pragma once
#include <QOpenGLWidget>
#include <QLabel>
#include "MpvCore.h"
#include <mpv/render.h>
#include <mpv/render_gl.h>

// MPV 공식 예제(qt_opengl) 방식: QOpenGLWidget + mpv_render_context
// WA_PaintOnScreen 없음 → 투명 문제 없음
// Qt OpenGL 컨텍스트를 MPV에 전달 → MPV가 Qt FBO에 직접 렌더링
class MpvWidget : public QOpenGLWidget {
    Q_OBJECT
public:
    explicit MpvWidget(QWidget* parent = nullptr);
    ~MpvWidget() override;

    MpvCore* core() const { return core_; }
    void loadFile(const QString& path);
    void appendFile(const QString& path);
    void showLogo(bool show);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void maybeUpdate();

private:
    static void onUpdate(void* ctx);

    MpvCore*             core_       = nullptr;
    mpv_render_context*  renderCtx_  = nullptr;

    QLabel*  logoLabel_  = nullptr;
    void     updateLogoPos();
};
