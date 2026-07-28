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
    // 이미 콘솔이 열려있으면 (배치 파일에서 실행 시) 연결
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
        freopen("CONIN$",  "r", stdin);
    }
}
#endif

int main(int argc, char* argv[]) {
#ifdef Q_OS_WIN
    attachConsole();
#endif
    // ── 시작속도 최적화: OpenGL 포맷을 앱 생성 전에 설정 ─────────────
    QSurfaceFormat fmt;
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    fmt.setSwapInterval(1);
    QSurfaceFormat::setDefaultFormat(fmt);

    // High DPI 지원
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

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
            // URL 또는 존재하는 파일이면 재생
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
