#include <QApplication>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QSurfaceFormat>
#include "MainWindow.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>

static void attachConsole() {
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
        freopen("CONIN$",  "r", stdin);
    }
}
#endif

// ============================================================
// HiDPI 근본 해결 원칙 (이 주석을 절대 삭제하지 말 것)
//
// sorinuri.manifest 에 PerMonitorV2 선언이 포함되어 있음.
// Windows EXE 로더가 manifest를 읽어 DPI 인식 모드를 설정.
// Qt6는 이 manifest를 감지하여 devicePixelRatio를 자동 계산.
//
// 절대 추가하지 말 것:
//   1) SetProcessDpiAwarenessContext() 코드 호출
//      → manifest와 충돌, 클릭 좌표 어긋남 발생
//   2) setHighDpiScaleFactorRoundingPolicy(PassThrough)
//      → Qt 논리 좌표와 Windows 마우스 이벤트 좌표 불일치
//   3) Qt::AA_EnableHighDpiScaling 수동 설정
//      → Qt6에서 deprecated, 오히려 문제 유발
//
// 올바른 방식: manifest 선언 하나만. 코드에서 DPI 건드리지 않음.
// ============================================================

int main(int argc, char* argv[]) {
#ifdef Q_OS_WIN
    attachConsole();
    // DPI 관련 코드 없음 - manifest가 처리함 (위 주석 참조)
#endif

    // 깜빡임 수정: 포커스 전환 시 OpenGL 컨텍스트 재생성 방지
    // QApplication 생성 전에 반드시 설정
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    // OpenGL 포맷 사전 설정 (시작속도 최적화)
    QSurfaceFormat fmt;
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    fmt.setSwapInterval(1);
    QSurfaceFormat::setDefaultFormat(fmt);

    // 포터블 환경: 현재 디렉토리에서 Qt 플러그인 로드
    QApplication::addLibraryPath(QDir::currentPath());

    QApplication app(argc, argv);
    app.setApplicationName("Sorinuri");
    app.setApplicationDisplayName("소리누리");
    app.setApplicationVersion("4.2.0");
    app.setOrganizationName("Sorinuri");
    app.setWindowIcon(QIcon(":/sorinuri-app.png"));

    // 다크 테마
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

    MainWindow window;
    window.show();

    // 커맨드라인 파일/URL 인수 처리
    QStringList args = app.arguments();
    if (args.size() > 1) {
        QStringList files;
        for (int i = 1; i < args.size(); ++i) {
            const QString& arg = args[i];
            if (arg.startsWith("http://") || arg.startsWith("https://") ||
                arg.startsWith("rtmp://")  || arg.startsWith("rtsp://") ||
                QFile::exists(arg)) {
                files << arg;
            }
        }
        if (!files.isEmpty()) window.openFiles(files);
    }

    return app.exec();
}
