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
    shutdown();
}

void MpvWidget::shutdown() {
    if (shutdownStarted_) return;
    shutdownStarted_ = true;
    mpvInitializationQueued_ = false;

    // libmpv를 종료하기 전에 Qt OpenGL 컨텍스트에 묶인 render context를 먼저
    // 해제한다. closeEvent에서 이 함수가 동기 완료되므로 WASAPI 독점 핸들이
    // QApplication 종료·객체 소멸까지 남지 않는다.
    if (context()) {
        makeCurrent();
        if (renderCtx_) {
            mpv_render_context_set_update_callback(renderCtx_, nullptr, nullptr);
            mpv_render_context_free(renderCtx_);
            renderCtx_ = nullptr;
        }
        doneCurrent();
    } else {
        renderCtx_ = nullptr;
    }

    if (core_) core_->shutdown();
}

// ── OpenGL 컨텍스트 파괴 시 renderCtx_ 안전 해제 ───────────────────────────────────
// Qt6 공식 권장 방식: aboutToBeDestroyed 시그널 핸들러
// 절전 복귀, 외부 모니터 연결/해제, reparent 시 컨텍스트가 파괴될 수 있음
// 이 슬롯에서 renderCtx_를 안전하게 해제하면 다음 initializeGL()에서 재생성됨
void MpvWidget::onContextAboutToBeDestroyed() {
    qInfo() << "[MpvWidget] OpenGL 컨텍스트 파괴 감지 → renderCtx_ 안전 해제";
    // makeCurrent()는 이미 컨텍스트가 파괴 중이라 호출 불필요
    // DirectConnection으로 호출되므로 이미 컨텍스트는 현재이며 유효함
    if (renderCtx_) {
        // MPV 렌더 콜백 제거 (파괴된 컨텍스트로 콜백 호출 방지)
        mpv_render_context_set_update_callback(renderCtx_, nullptr, nullptr);
        mpv_render_context_free(renderCtx_);
        renderCtx_ = nullptr;
        qInfo() << "[MpvWidget] renderCtx_ 해제 완료";
    }
}

void MpvWidget::initializeGL() {
    // Qt는 이 시점에 OpenGL 컨텍스트와 빈 FBO를 준비한다. libmpv 초기화는
    // WASAPI·GPU·렌더 환경을 동기 설정하므로, 먼저 빈 프레임을 합성한 후
    // 이벤트 루프에서 시작해 노트북의 창 표시 지연을 줄인다.
    queueDeferredMpvInitialization();

    // ── 멀티모니터 이동 감지 ─────────────────────────────────────
    // Qt가 FBO·DPI·위젯 크기를 다시 만들고 paintGL()은 매 프레임 물리 크기를
    // 계산한다. 화면 이동으로 MPV 출력/오디오를 재초기화하지 않는다.
    if (QWindow* win = window()->windowHandle()) {
        connectScreenChanged(win);
    } else {
        screenChangedConnected_ = false;
        qInfo() << "[MpvWidget] windowHandle 없음 → showEvent에서 멀티모니터 감지 연결 재시도";
    }

    updateLogoPos();
}

void MpvWidget::queueDeferredMpvInitialization() {
    if (shutdownStarted_ || mpvInitializationQueued_ || renderCtx_ || !context()) return;
    mpvInitializationQueued_ = true;

    // 첫 프레임이 화면에 반영될 시간을 보장한다. 이 지연 동안 파일 열기 요청은
    // MainWindow의 pendingStartupFiles_에 보관되며 mpvInitialized 이후 처리된다.
    QTimer::singleShot(80, this, [this]() {
        mpvInitializationQueued_ = false;
        if (shutdownStarted_ || renderCtx_ || !context()) return;

        makeCurrent();
        const bool ready = initializeMpvRenderContext();
        doneCurrent();
        if (ready) update();
    });
}

bool MpvWidget::initializeMpvRenderContext() {
    if (renderCtx_) return true;
    if (!core_->initialize(0)) {
        qCritical() << "[MpvWidget] MPV 초기화 실패";
        return false;
    }

    mpv_opengl_init_params glInitParams = { getGlProcAddress, nullptr };
    mpv_render_param params[] = {
        { MPV_RENDER_PARAM_API_TYPE,            const_cast<char*>(MPV_RENDER_API_TYPE_OPENGL) },
        { MPV_RENDER_PARAM_OPENGL_INIT_PARAMS,  &glInitParams },
        { MPV_RENDER_PARAM_INVALID,             nullptr }
    };

    if (mpv_render_context_create(&renderCtx_, core_->handle(), params) < 0) {
        qCritical() << "[MpvWidget] MPV render context 생성 실패";
        return false;
    }

    mpv_render_context_set_update_callback(renderCtx_, MpvWidget::onUpdate,
                                           reinterpret_cast<void*>(this));
    connect(context(), &QOpenGLContext::aboutToBeDestroyed,
            this, &MpvWidget::onContextAboutToBeDestroyed,
            Qt::DirectConnection);

    qInfo() << "[MpvWidget] 지연 OpenGL/MPV render context 초기화 완료";
    QTimer::singleShot(0, this, [this]() { emit mpvInitialized(); });
    return true;
}

void MpvWidget::paintGL() {
    if (!renderCtx_) {
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        return;
    }

    // ── NVIDIA Optimus 검은 화면 수정 ────────────────────────────
    // Optimus 환경에서 QOpenGLWidget 초기화 직후 defaultFramebufferObject()가
    // 0을 반환하는 경우가 있음. FBO 0(기본 프레임버퍼)에 MPV가 렌더링하면
    // Intel GPU 디스플레이 버퍼에 출력되어 NVIDIA 렌더링 결과가 화면에 안 나옴.
    // → FBO ID가 0이면 렌더링 건너뛰고 16ms 후 재시도.
    GLuint fboId = defaultFramebufferObject();
    if (fboId == 0) {
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        QTimer::singleShot(16, this, [this]() { if (renderCtx_) update(); });
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
        static_cast<int>(fboId),
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
        // 화면 이동 시 Qt가 FBO/DPI를 재구성한다. MPV의 출력·오디오 장치를
        // 건드리면 HDMI 협상과 렌더 컨텍스트가 불필요하게 재시작될 수 있다.
        QTimer::singleShot(0, this, [this]() {
            updateLogoPos();
            update();
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
    // 최소화 시 렌더링 완전 중단
    // 이전 코드: isMinimized() 시 makeCurrent()+paintGL()+swapBuffers() 직접 호용
    // 문제: Qt 콤포지팅 파이프라인을 우회하여 Optimus 환경에서
    //   Intel GPU 디스플레이 버퍼에 잘못 렌더링하는 문제 발생 가능
    // 해결: 최소화 시 MPV에게 렌더링 완전 중단
    //   MPV는 렌더링 콜백이 없으면 자체적으로 대기
    if (window()->isMinimized()) return;
    update();
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
