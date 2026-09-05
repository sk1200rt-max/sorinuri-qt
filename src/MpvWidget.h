#pragma once
#include <QOpenGLWidget>
#include <QLabel>
#include <QShowEvent>
#include <atomic>
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

    // OTT·오리지널·음악 화면처럼 영상 표면이 보이지 않는 동안에는 OpenGL repaint만
    // 멈춘다. libmpv 재생·오디오 출력은 계속 유지하고, 플레이어로 돌아오면 현재
    // 프레임을 즉시 한 번 갱신한다.
    void setPresentationActive(bool active);

    // 메인 창 closeEvent에서 호출한다. OpenGL render context를 먼저 해제한 뒤
    // MpvCore를 동기 종료하여 WASAPI 독점 핸들이 다른 앱을 막지 않게 한다.
    void shutdown();

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
    // 첫 Qt 프레임이 합성된 뒤 libmpv·WASAPI·렌더 컨텍스트를 준비한다.
    // OpenGL 컨텍스트가 current인 GUI 스레드에서만 호출한다.
    bool initializeMpvRenderContext();
    void queueDeferredMpvInitialization();

    MpvCore*             core_       = nullptr;
    mpv_render_context*  renderCtx_  = nullptr;
    bool mpvInitializationQueued_ = false;
    bool  shutdownStarted_ = false;
    bool  screenChangedConnected_ = false;  // 멀티모니터 감지 연결 여부
    std::atomic_bool presentationActive_{true};        // 화면에 실제로 보이는 영상 표면만 repaint
    std::atomic_bool presentationRefreshPending_{false};
    void  connectScreenChanged(QWindow* win);  // 멀티모니터 시그널 연결 헬퍼

    QLabel*  logoLabel_  = nullptr;
    void     updateLogoPos();

    // AI 자막 오버레이
    QString  aiSubText_;
    int      aiSubConf_  = 0;
    QTimer*  aiSubTimer_ = nullptr;  // 자막 자동 소거 타이머
};
