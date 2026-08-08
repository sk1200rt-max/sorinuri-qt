#pragma once
#include <QOpenGLWidget>
#include <QLabel>
#include <QShowEvent>
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
    void setAiSubtitle(const QString& text, int confidence);
    void clearAiSubtitle();

    // MPV 초기화 완료 여부 확인 (시작 파일 로드 타이밍 제어에 사용)
    bool isMpvInitialized() const;

signals:
    // initializeGL() 완료 후 emit → 시작 파일 로드에 사용
    // HiDPI 250% 환경에서 window.show() 직후 loadFile 호출 시
    // initialized_=false 로 무시되는 문제를 근본 해결
    void mpvInitialized();

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;  // HiDPI: DPI 변경 시 FBO 재생성
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

private slots:
    void maybeUpdate();
    // Qt6 공식 권장: OpenGL 컨텍스트 파괴 시 renderCtx_ 안전 해제
    // 절전 복귀, 외부 모니터 연결/해제, reparent 시 컨텍스트가 파괴될 수 있음
    // aboutToBeDestroyed 시그널에 연결하여 리소스 정리
    void onContextAboutToBeDestroyed();

private:
    static void onUpdate(void* ctx);

    MpvCore*             core_       = nullptr;
    mpv_render_context*  renderCtx_  = nullptr;
    bool  screenChangedConnected_ = false;  // 멀티모니터 감지 연결 여부
    void  connectScreenChanged(QWindow* win);  // 멀티모니터 시그널 연결 헬퍼

    QLabel*  logoLabel_  = nullptr;
    void     updateLogoPos();

    // AI 자막 오버레이
    QString  aiSubText_;
    int      aiSubConf_  = 0;
    QTimer*  aiSubTimer_ = nullptr;  // 자막 자동 소거 타이머
};
