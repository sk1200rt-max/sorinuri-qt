#include "MpvWidget.h"
#include <QResizeEvent>
#include <QOpenGLContext>
#include <QDebug>
#include <stdexcept>

static void* getGlProcAddress(void* /*ctx*/, const char* name) {
    QOpenGLContext* ctx = QOpenGLContext::currentContext();
    if (!ctx) return nullptr;
    return reinterpret_cast<void*>(ctx->getProcAddress(QByteArray(name)));
}

MpvWidget::MpvWidget(QWidget* parent) : QOpenGLWidget(parent) {
    // QOpenGLWidget은 Qt가 직접 OpenGL FBO를 관리
    // 배경은 Qt가 검은색으로 초기화 (clearColor)
    setAutoFillBackground(false);

    core_ = new MpvCore(this);

    // ── 소리누리 로고 오버레이 ────────────────────────────────────
    logoLabel_ = new QLabel(this);
    logoLabel_->setAttribute(Qt::WA_TransparentForMouseEvents);
    logoLabel_->setStyleSheet("background: transparent; border: none;");
    logoLabel_->setAlignment(Qt::AlignCenter);

    QPixmap logo(":/sorinuri-logo-center.png");
    if (!logo.isNull()) {
        QPixmap scaled = logo.scaledToWidth(320, Qt::SmoothTransformation);
        logoLabel_->setPixmap(scaled);
        logoLabel_->resize(scaled.size());
    } else {
        logoLabel_->setText("소리누리");
        logoLabel_->setStyleSheet(
            "background: transparent; color: rgba(255,255,255,80);"
            "font-size: 36px; font-weight: 700; font-family: 'Malgun Gothic';");
        logoLabel_->adjustSize();
    }
    logoLabel_->show();
    logoLabel_->raise();
}

MpvWidget::~MpvWidget() {
    makeCurrent();
    if (renderCtx_) {
        mpv_render_context_free(renderCtx_);
        renderCtx_ = nullptr;
    }
    doneCurrent();
}

void MpvWidget::initializeGL() {
    // OpenGL 컨텍스트가 준비된 후 MPV 초기화
    // vo=libmpv 으로 설정하면 --wid 없이 render API 사용
    if (!core_->initialize(0)) {  // wid=0 → render API 사용
        qCritical() << "[MpvWidget] MPV 초기화 실패";
        return;
    }

    // MPV render context 생성 (OpenGL)
    mpv_opengl_init_params glInitParams = { getGlProcAddress, nullptr };
    mpv_render_param params[] = {
        { MPV_RENDER_PARAM_API_TYPE,            const_cast<char*>(MPV_RENDER_API_TYPE_OPENGL) },
        { MPV_RENDER_PARAM_OPENGL_INIT_PARAMS,  &glInitParams },
        { MPV_RENDER_PARAM_INVALID,             nullptr }
    };

    if (mpv_render_context_create(&renderCtx_, core_->handle(), params) < 0) {
        qCritical() << "[MpvWidget] MPV render context 생성 실패";
        return;
    }

    // MPV가 새 프레임을 준비하면 Qt update() 호출
    mpv_render_context_set_update_callback(renderCtx_, MpvWidget::onUpdate,
                                           reinterpret_cast<void*>(this));

    qInfo() << "[MpvWidget] OpenGL render context 초기화 완료";
    updateLogoPos();
}

void MpvWidget::paintGL() {
    if (!renderCtx_) {
        // render context 없으면 검은 배경만
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        return;
    }

    mpv_opengl_fbo fbo = {
        static_cast<int>(defaultFramebufferObject()),
        width(), height(), 0
    };
    int flipY = 1;
    mpv_render_param params[] = {
        { MPV_RENDER_PARAM_OPENGL_FBO, &fbo      },
        { MPV_RENDER_PARAM_FLIP_Y,     &flipY    },
        { MPV_RENDER_PARAM_INVALID,    nullptr   }
    };
    mpv_render_context_render(renderCtx_, params);
}

void MpvWidget::resizeEvent(QResizeEvent* event) {
    QOpenGLWidget::resizeEvent(event);
    updateLogoPos();
}

void MpvWidget::updateLogoPos() {
    if (!logoLabel_ || !logoLabel_->isVisible()) return;
    int x = (width()  - logoLabel_->width())  / 2;
    int y = (height() - logoLabel_->height()) / 2;
    logoLabel_->move(x, y);
    logoLabel_->raise();
}

void MpvWidget::showLogo(bool show) {
    if (!logoLabel_) return;
    logoLabel_->setVisible(show);
    if (show) { logoLabel_->raise(); updateLogoPos(); }
}

void MpvWidget::loadFile(const QString& path) {
    showLogo(false);
    core_->loadFile(path, false);
}

void MpvWidget::appendFile(const QString& path) {
    core_->loadFile(path, true);
}

// MPV가 새 프레임 준비 완료 시 호출 (별도 스레드에서 호출될 수 있음)
void MpvWidget::onUpdate(void* ctx) {
    QMetaObject::invokeMethod(reinterpret_cast<MpvWidget*>(ctx),
                              "maybeUpdate", Qt::QueuedConnection);
}

void MpvWidget::maybeUpdate() {
    if (window()->isMinimized()) {
        // 최소화 상태에서는 수동으로 렌더링
        makeCurrent();
        paintGL();
        context()->swapBuffers(context()->surface());
        doneCurrent();
    } else {
        update();  // Qt가 paintGL() 호출하도록 요청
    }
}
