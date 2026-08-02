// ══════════════════════════════════════════════════════════════════
// HiDPI 처리 원칙 (이 주석을 절대 삭제하지 말 것)
//
// Qt6 + Windows HiDPI 올바른 방식:
//   1. resources/sorinuri.manifest 에 PerMonitorV2 선언 (완료)
//   2. CMakeLists.txt 에 /MANIFEST:NO 링커 플래그 (완료)
//   3. main.cpp 에서 DPI 관련 코드를 일절 추가하지 않는다.
//
// Qt6는 manifest의 PerMonitorV2를 자동으로 감지하여 처리한다.
// SetProcessDpiAwarenessContext, AA_EnableHighDpiScaling,
// Qt::HighDpiScaleFactorRoundingPolicy 등을 추가하면 오히려 충돌이
// 발생하여 클릭 좌표가 어긋난다.
//
// MpvWidget::paintGL() 에서만 devicePixelRatio()를 사용하여
// FBO 크기를 물리 픽셀로 계산한다. 나머지 위젯은 Qt가 자동 처리.
// ══════════════════════════════════════════════════════════════════

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QPalette>
#include <QColor>
#include <QSurfaceFormat>
#include <QStringList>
#include "MainWindow.h"

int main(int argc, char* argv[])
{
    // ── 깜빡임 수정: 포커스 전환 시 OpenGL 컨텍스트 재생성 방지 ──
    // QApplication 생성 전에 반드시 설정해야 효과 있음
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    // ── OpenGL 포맷 사전 설정 (시작속도 최적화) ───────────────────
    // QApplication 생성 전에 설정해야 OpenGL 컨텍스트 협상 시간 단축
    QSurfaceFormat fmt;
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setSwapBehavior(QSurfaceFormat::TripleBuffer);  // 프레임 대기 제거 (4K HiDPI 끊김 방지)
    fmt.setSwapInterval(1);  // Qt VSync만 사용 (이중 VSync 충돌 방지)
    QSurfaceFormat::setDefaultFormat(fmt);

    // ── Qt 플러그인 경로 고정 (포터블 실행 속도 핵심) ─────────────
    // QApplication 생성 전에 호출해야 플러그인 탐색 경로가 고정됨
    QApplication::addLibraryPath(QDir::currentPath());

    // ── QApplication 생성 ─────────────────────────────────────────
    QApplication app(argc, argv);
    app.setApplicationName("Sorinuri");
    app.setApplicationDisplayName("소리누리");
    app.setApplicationVersion("6.3.10");
    app.setOrganizationName("Sorinuri");
    app.setWindowIcon(QIcon(":/icons/sorinuri.ico"));

    // ── 다크 테마 ─────────────────────────────────────────────────
    app.setStyle("Fusion");
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window,          QColor(20, 20, 20));
    darkPalette.setColor(QPalette::WindowText,      QColor(224, 224, 224));
    darkPalette.setColor(QPalette::Base,            QColor(17, 17, 17));
    darkPalette.setColor(QPalette::AlternateBase,   QColor(25, 25, 25));
    darkPalette.setColor(QPalette::ToolTipBase,     QColor(30, 30, 30));
    darkPalette.setColor(QPalette::ToolTipText,     QColor(200, 200, 200));
    darkPalette.setColor(QPalette::Text,            QColor(224, 224, 224));
    darkPalette.setColor(QPalette::Button,          QColor(30, 30, 30));
    darkPalette.setColor(QPalette::ButtonText,      QColor(224, 224, 224));
    darkPalette.setColor(QPalette::BrightText,      Qt::red);
    darkPalette.setColor(QPalette::Link,            QColor(79, 195, 247));
    darkPalette.setColor(QPalette::Highlight,       QColor(26, 58, 92));
    darkPalette.setColor(QPalette::HighlightedText, Qt::white);
    app.setPalette(darkPalette);

    // ── 메인 창 생성 및 표시 ──────────────────────────────────────
    MainWindow window;
    window.show();

    // ── 커맨드라인 파일/URL 인수 처리 ────────────────────────────
    QStringList args = app.arguments();
    if (args.size() > 1) {
        QStringList files;
        for (int i = 1; i < args.size(); ++i) {
            const QString& arg = args[i];
            if (arg.startsWith("http://")  || arg.startsWith("https://") ||
                arg.startsWith("rtmp://")  || arg.startsWith("rtsp://")  ||
                QFile::exists(arg)) {
                files << arg;
            }
        }
        if (!files.isEmpty()) window.openFiles(files);
    }

    return app.exec();
}
