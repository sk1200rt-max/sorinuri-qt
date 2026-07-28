#include <QApplication>
#include <QDir>
#include <QDebug>
#include <QIcon>
#include <QFile>
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

// Qt6에서 Per-Monitor V2 DPI 인식을 소프트웨어적으로도 명시
// (manifest의 하드웨어 선언과 함께 사용)
static void setDpiAwareness() {
    // SetProcessDpiAwarenessContext는 Windows 10 1607 이상에서 사용 가능
    // Per Monitor V2: DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
    typedef BOOL (WINAPI* SetProcessDpiAwarenessContextFunc)(HANDLE);
    HMODULE user32 = LoadLibraryW(L"user32.dll");
    if (user32) {
        auto fn = reinterpret_cast<SetProcessDpiAwarenessContextFunc>(
            GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
        if (fn) {
            // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 = -4
            fn(reinterpret_cast<HANDLE>(-4));
        }
        FreeLibrary(user32);
    }
}
#endif

int main(int argc, char* argv[]) {
#ifdef Q_OS_WIN
    attachConsole();
    // QApplication 생성 전에 DPI 인식 설정 (반드시 먼저 호출)
    setDpiAwareness();
#endif

    // ── Qt6 HiDPI 정책: 소수점 배율(125%, 150%, 175%, 250%)을 그대로 사용 ──
    // PassThrough: 250% → 2.5배율로 정확히 처리 (반올림 없음)
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    // ── 시작속도 최적화: OpenGL 포맷을 앱 생성 전에 설정 ─────────────
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
    app.setApplicationVersion("1.0.0");
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
