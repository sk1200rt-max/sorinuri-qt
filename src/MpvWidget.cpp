#include "MpvWidget.h"
#include <QResizeEvent>
#include <QShowEvent>
#include <QOpenGLContext>
#include <QDebug>
#include <QTimer>
#include <QRegularExpression>
#include <QWindow>
#include <stdexcept>

static void* getGlProcAddress(void* /*ctx*/, const char* name) {
    QOpenGLContext* ctx = QOpenGLContext::currentContext();
    if (!ctx) return nullptr;
    return reinterpret_cast<void*>(ctx->getProcAddress(QByteArray(name)));
}

MpvWidget::MpvWidget(QWidget* parent) : QOpenGLWidget(parent) {
    setAutoFillBackground(false);
    // 깜빡임 수정: 프레임 버퍼를 유지하여 포커스 전환 시 깜빡임 제거
    // PartialUpdate: 이전 프레임을 지우지 않고 유지 → 검은 화면 깜빡임 없음
    setUpdateBehavior(QOpenGLWidget::PartialUpdate);

    core_ = new MpvCore(this);

    // ── 소리누리 로고 오버레이 ────────────────────────────────────
    logoLabel_ = new QLabel(this);
    logoLabel_->setAttribute(Qt::WA_TransparentForMouseEvents);
    logoLabel_->setStyleSheet("background: transparent; border: none;");
    logoLabel_->setAlignment(Qt::AlignCenter);

    QPixmap logo(":/sorinuri-logo-center.png");
    if (!logo.isNull()) {
        // devicePixelRatio를 고려한 로고 크기 설정
        // 논리 픽셀 320px → HiDPI에서도 동일한 논리 크기 유지
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
    // 생성자 시점에는 위젯 크기가 0이므로 화면 밖에 숨겨둠
    logoLabel_->move(-9999, -9999);
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
    if (!core_->initialize(0)) {
        qCritical() << "[MpvWidget] MPV 초기화 실패";
        return;
    }

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

    mpv_render_context_set_update_callback(renderCtx_, MpvWidget::onUpdate,
                                           reinterpret_cast<void*>(this));

    qInfo() << "[MpvWidget] OpenGL render context 초기화 완료";
    updateLogoPos();
}

void MpvWidget::paintGL() {
    if (!renderCtx_) {
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        return;
    }

    // ── HiDPI 핵심: FBO 크기를 물리 픽셀(physical pixels)로 전달 ──
    // width()/height()는 논리 픽셀이므로 devicePixelRatio를 곱해야
    // 4K 250% 배율에서 영상이 전체 화면을 채움
    const qreal dpr = devicePixelRatio();
    const int physW = static_cast<int>(width()  * dpr);
    const int physH = static_cast<int>(height() * dpr);

    mpv_opengl_fbo fbo = {
        static_cast<int>(defaultFramebufferObject()),
        physW, physH, 0
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

// showEvent: 위젯이 실제로 화면에 표시될 때 크기가 확정
void MpvWidget::showEvent(QShowEvent* event) {
    QOpenGLWidget::showEvent(event);
    QTimer::singleShot(0, this, [this]() { updateLogoPos(); });
}

void MpvWidget::updateLogoPos() {
    if (!logoLabel_ || !logoLabel_->isVisible()) return;
    if (width() <= 0 || height() <= 0) return;
    // 논리 픽셀 기준으로 중앙 계산 (Qt 위젯 좌표계는 항상 논리 픽셀)
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

void MpvWidget::onUpdate(void* ctx) {
    QMetaObject::invokeMethod(reinterpret_cast<MpvWidget*>(ctx),
                              "maybeUpdate", Qt::QueuedConnection);
}

void MpvWidget::maybeUpdate() {
    if (window()->isMinimized()) {
        makeCurrent();
        paintGL();
        context()->swapBuffers(context()->surface());
        doneCurrent();
    } else {
        update();
    }
}

void MpvWidget::setAiSubtitle(const QString& text, int confidence) {
    aiSubText_ = text;
    aiSubConf_ = confidence;
    // 5초 후 자동 소거
    if (!aiSubTimer_) {
        aiSubTimer_ = new QTimer(this);
        aiSubTimer_->setSingleShot(true);
        connect(aiSubTimer_, &QTimer::timeout, this, &MpvWidget::clearAiSubtitle);
    }
    aiSubTimer_->start(5000);
    update();
}

void MpvWidget::clearAiSubtitle() {
    aiSubText_.clear();
    aiSubConf_ = 0;
    update();
}
