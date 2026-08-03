#include "MpvWidget.h"
#include <QResizeEvent>
#include <QShowEvent>
#include <QPainter>
#include <QFont>
#include <QFontMetrics>
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
    // NoPartialUpdate: 매 프레임 전체 재렌더링 → 화면 끊김/잔상 없음
    // PartialUpdate는 이전 버퍼 유지로 깜빡임을 줄이지만 끊김 발생 가능
    setUpdateBehavior(QOpenGLWidget::NoPartialUpdate);

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

    // HiDPI 근본 수정: initializeGL() 완료 후 mpvInitialized 시그널 emit
    // → MainWindow가 pendingStartupFiles_를 처리하는 트리거
    // QTimer::singleShot(0): 현재 initializeGL() 스택이 완전히 끝난 후 emit
    // (renderCtx_ 설정 완료 보장)
    QTimer::singleShot(0, this, [this]() { emit mpvInitialized(); });

    // ── OpenGL 컨텍스트 준비 완료 후 GPU 정보 재감지 ─────────────
    // initialize() 시점에는 OpenGL 컨텍스트가 없어 GPU 벤더 감지 불가.
    // initializeGL() 이후에는 GL_RENDERER/GL_VENDOR 읽기 가능.
    // → GPU 벤더 기반 렌더링 설정 재적용 (화면 해상도만 기반이던 초기 설정 개선)
    core_->redetectGpuAndApply();

    // ── 멀티모니터 이동 감지 ─────────────────────────────────────
    // 소리누리 창을 다른 모니터로 이동 시 해상도/주사율/DPI가 바뀜
    // QWindow::screenChanged 시그널 → redetectGpuAndApply() 재호출
    // 예: FHD 모니터(ewa_lanczossharp) → 4K 모니터(spline36) 자동 전환
    //
    // 폴백: initializeGL() 시점에 windowHandle()이 nullptr일 수 있음
    // (위젯이 최상위 윈도우에 완전히 연결되기 전)
    // → showEvent()에서 screenChangedConnected_ 확인 후 재시도
    if (QWindow* win = window()->windowHandle()) {
        connectScreenChanged(win);
    } else {
        screenChangedConnected_ = false;
        qInfo() << "[MpvWidget] windowHandle 없음 → showEvent에서 멀티모니터 감지 연결 재시도";
    }

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

    // internal_format을 GL_RGBA8로 명시 → MPV가 FBO 포맷을 추측하며
    // rgba16f 등을 시도하다 INVALID_ENUM 오류를 반복하던 문제 해결
    mpv_opengl_fbo fbo = {
        static_cast<int>(defaultFramebufferObject()),
        physW, physH,
        0x8058 /* GL_RGBA8 */
    };
    int flipY = 1;
    mpv_render_param params[] = {
        { MPV_RENDER_PARAM_OPENGL_FBO, &fbo      },
        { MPV_RENDER_PARAM_FLIP_Y,     &flipY    },
        { MPV_RENDER_PARAM_INVALID,    nullptr   }
    };
    mpv_render_context_render(renderCtx_, params);

    // ── AI 자막 오버레이 렌더링 ──────────────────────────────────
    if (!aiSubText_.isEmpty()) {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        // 신뢰도에 따른 배지 색상
        QColor badgeColor;
        QString badge;
        if (aiSubConf_ >= 90) {
            badgeColor = QColor(0, 200, 180);  // 청록 - 높은 신뢰도
            badge = " [AI]";
        } else if (aiSubConf_ >= 80) {
            badgeColor = QColor(255, 200, 0);  // 노란 - 보통 신뢰도
            badge = " [AI?]";
        } else {
            badgeColor = QColor(255, 140, 0);  // 주황 - 낮은 신뢰도
            badge = " [AI!]";
        }

        // 자막 텍스트 + 배지
        QString displayText = aiSubText_;

        // 폰트 설정
        QFont subFont("Malgun Gothic", 16, QFont::Bold);
        painter.setFont(subFont);
        QFontMetrics fm(subFont);

        // 텍스트 영역 계산 (하단 15% 위치)
        int textW = fm.horizontalAdvance(displayText) + 40;
        int textH = fm.height() + 16;
        int x = (width() - textW) / 2;
        int y = height() - textH - static_cast<int>(height() * 0.08);

        // 반투명 배경
        painter.setBrush(QColor(0, 0, 0, 180));
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(x - 8, y - 4, textW + 16, textH + 8, 6, 6);

        // 자막 텍스트
        painter.setPen(Qt::white);
        painter.drawText(x, y + fm.ascent() + 4, displayText);

        // 배지 (우측 끝)
        QFont badgeFont("Consolas", 9, QFont::Bold);
        painter.setFont(badgeFont);
        QFontMetrics bfm(badgeFont);
        int bw = bfm.horizontalAdvance(badge) + 8;
        int bx = x + textW - bw - 4;
        int by = y + (textH - bfm.height()) / 2 - 2;
        painter.setBrush(badgeColor);
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(bx, by, bw, bfm.height() + 4, 3, 3);
        painter.setPen(Qt::black);
        painter.setFont(badgeFont);
        painter.drawText(bx + 4, by + bfm.ascent() + 2, badge);

        painter.end();
    }
}

void MpvWidget::resizeGL(int /*w*/, int /*h*/) {
    // HiDPI: 윈도우 크기 또는 DPI가 변경될 때 Qt가 자동 호출
    // FBO는 Qt가 자동 재생성하므로 paintGL에서 devicePixelRatio로
    // 물리 픽셀 크기를 재계산하면 충분함
    if (renderCtx_) update();
}

void MpvWidget::resizeEvent(QResizeEvent* event) {
    QOpenGLWidget::resizeEvent(event);
    updateLogoPos();
}

// showEvent: 위젯이 실제로 화면에 표시될 때 크기가 확정
void MpvWidget::showEvent(QShowEvent* event) {
    QOpenGLWidget::showEvent(event);
    QTimer::singleShot(0, this, [this]() { updateLogoPos(); });

    // initializeGL()  시점에 windowHandle()이 없었던 경우 여기서 재시도
    if (!screenChangedConnected_) {
        if (QWindow* win = window()->windowHandle()) {
            connectScreenChanged(win);
            qInfo() << "[MpvWidget] showEvent: 멀티모니터 감지 연결 성공 (폴백)";
        }
    }
}

void MpvWidget::connectScreenChanged(QWindow* win) {
    if (!win || screenChangedConnected_) return;
    connect(win, &QWindow::screenChanged, this, [this](QScreen* newScreen) {
        Q_UNUSED(newScreen)
        qInfo() << "[MpvWidget] 모니터 변경 감지 → 렌더링 설정 재적용";
        // 500ms 지연 후 재감지 (모니터 전환 완료 대기)
        QTimer::singleShot(500, this, [this]() {
            core_->redetectGpuAndApply();
        });
    });
    screenChangedConnected_ = true;
    qInfo() << "[MpvWidget] 멀티모니터 이동 감지 활성화";
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

bool MpvWidget::isMpvInitialized() const {
    return core_ && core_->isInitialized();
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
    // MPV 렌더 스레드 → Qt 메인 스레드로 안전하게 전달
    // QueuedConnection: 이벤트 루프를 통해 메인 스레드에서 실행 (스레드 안전)
    MpvWidget* w = reinterpret_cast<MpvWidget*>(ctx);
    QMetaObject::invokeMethod(w, "maybeUpdate", Qt::QueuedConnection);
}

void MpvWidget::maybeUpdate() {
    if (!renderCtx_) return;
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
