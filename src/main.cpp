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

// ══════════════════════════════════════════════════════════════════
// 노트북 전용 GPU 강제 선택 (NVIDIA Optimus / AMD Hybrid)
//
// 노트북에서 소리누리가 기본적으로 Intel 통합 GPU에서 실행되는 문제 수정.
// NvOptimusEnablement / AmdPowerXpressRequestHighPerformance 심볼을
// DLL 익스포트로 선언하면 드라이버가 자동으로 전용 GPU를 선택한다.
// ══════════════════════════════════════════════════════════════════
#ifdef _WIN32
extern "C" {
    __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#include <windows.h>
#include <timeapi.h>
#pragma comment(lib, "winmm.lib")
#endif

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QPalette>
#include <QSurfaceFormat>
#include <QThread>
#include <QUrl>

#include "InstanceCoordinator.h"
#include "MainWindow.h"

#ifdef _WIN32
// Windows 10/11은 UserChoice 해시로 보호되므로 설치 프로그램이 레지스트리만
// 써서 기본 앱을 강제할 수 없다. 최신 Windows에서 호환되는 기본 앱 설정 화면을
// 열어 사용자가 소리누리를 명시적으로 기본 재생 프로그램으로 선택하게 한다.
static void launchFileAssociationUI()
{
    const QString uri = QStringLiteral("ms-settings:defaultapps?registeredAppMachine=")
        + QString::fromLatin1(QUrl::toPercentEncoding(QStringLiteral("소리누리")));
    const auto result = ShellExecuteW(
        nullptr, L"open", reinterpret_cast<LPCWSTR>(uri.utf16()), nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result) <= 32) {
        ShellExecuteW(nullptr, L"open", L"ms-settings:defaultapps", nullptr, nullptr, SW_SHOWNORMAL);
    }
}
#endif

static bool sendToExistingCoordinator(
    InstanceCoordinator& coordinator,
    const InstanceCoordinator::Message& message)
{
    // 동시에 두 실행이 시작되는 race에서는 첫 coordinator가 socket을 listen하기까지
    // 아주 짧은 시간이 필요하다. 새 server를 강제로 삭제하지 않고 재접속만 제한적으로 시도한다.
    for (int attempt = 0; attempt < 3; ++attempt) {
        if (coordinator.sendMessage(message)) return true;
        QThread::msleep(120);
    }
    return false;
}

int main(int argc, char* argv[])
{
#ifdef _WIN32
    // 기본값 15.6ms → MPV의 display-resample 프레임 타이밍이 부정확해짐
    timeBeginPeriod(1);
#endif

    // QApplication 생성 전에 설정해야 OpenGL 컨텍스트 협상 시간 단축
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QSurfaceFormat fmt;
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    fmt.setSwapInterval(1);
    fmt.setDepthBufferSize(24);
    fmt.setStencilBufferSize(8);
    QSurfaceFormat::setDefaultFormat(fmt);

    QApplication::addLibraryPath(QDir::currentPath());
    QApplication app(argc, argv);
    app.setApplicationName("Sorinuri");
    app.setApplicationDisplayName("소리누리");
    app.setApplicationVersion("6.21.1");
    app.setOrganizationName("Sorinuri");
    app.setWindowIcon(QIcon(":/icons/sorinuri.ico"));

    QCommandLineParser parser;
    parser.addHelpOption();
    QCommandLineOption newWindowOption(
        QStringList() << "n" << "new-window",
        "새 플레이어 창을 열고 모든 소리누리를 공유 PCM 다중 재생 세션으로 전환합니다.");
    QCommandLineOption registerAssociationsOption(
        "register-file-associations",
        "Windows 기본 앱 선택 화면을 열어 소리누리 파일 연결을 설정합니다.");
    parser.addOption(newWindowOption);
    parser.addOption(registerAssociationsOption);
    parser.addPositionalArgument("file", "재생할 파일 경로");
    parser.process(app);

    // 설치 프로그램의 파일 연결 작업 전용 경로: 메인 창·IPC·MPV는 생성하지 않는다.
    if (parser.isSet(registerAssociationsOption)) {
#ifdef _WIN32
        launchFileAssociationUI();
        timeEndPeriod(1);
#endif
        return 0;
    }

    const QStringList positional = parser.positionalArguments();
    const bool requestedNewWindow = parser.isSet(newWindowOption);
    InstanceCoordinator coordinator(&app);
    bool startInMultiSharedSession = InstanceCoordinator::multiSessionActive();

    if (requestedNewWindow) {
        // 이미 실행 중인 기본 창이 있으면 먼저 그 창을 shared PCM으로 옮긴다.
        // ACK 뒤에만 새 창의 libmpv 초기화를 시작하여 exclusive endpoint 경합을 줄인다.
        InstanceCoordinator::Message request;
        request.type = InstanceCoordinator::MessageType::StartMultiSession;
        if (!sendToExistingCoordinator(coordinator, request)) {
            // coordinator가 없으면 이 새 창이 coordinator가 된다. 실패 시에는 race를
            // 고려해 기존 coordinator가 생겼는지 마지막으로 재확인한다.
            if (!coordinator.acquireCoordinator() && !sendToExistingCoordinator(coordinator, request)) {
                qCritical() << "[Instance] 새 창 다중 재생 coordinator를 시작할 수 없습니다";
#ifdef _WIN32
                timeEndPeriod(1);
#endif
                return 2;
            }
        }
        if (!coordinator.joinMultiSession()) {
            qCritical() << "[Instance] 다중 재생 shared PCM 세션을 만들 수 없습니다";
#ifdef _WIN32
            timeEndPeriod(1);
#endif
            return 3;
        }
        startInMultiSharedSession = true;
    } else {
        // 일반 실행과 파일 연결 더블클릭은 기존 coordinator로만 전달한다.
        InstanceCoordinator::Message request;
        request.type = InstanceCoordinator::MessageType::OpenFiles;
        request.files = positional;
        if (sendToExistingCoordinator(coordinator, request)) {
#ifdef _WIN32
            timeEndPeriod(1);
#endif
            return 0;
        }

        // 기존 server에 접속할 수 없는 경우에만 coordinator lock을 획득하고 stale
        // endpoint를 정리한다. lock 경쟁에 진 경우 새 server를 만들지 않고 전달을 재시도한다.
        if (!coordinator.acquireCoordinator()) {
            if (sendToExistingCoordinator(coordinator, request)) {
#ifdef _WIN32
                timeEndPeriod(1);
#endif
                return 0;
            }
            qCritical() << "[Instance] 기존 실행을 확인할 수 없고 coordinator도 시작하지 못했습니다";
#ifdef _WIN32
            timeEndPeriod(1);
#endif
            return 4;
        }
        startInMultiSharedSession = coordinator.isInMultiSession();
        // 이전 coordinator가 비정상 종료한 뒤 이 프로세스가 coordinator가 된 경우에도
        // 기존 다중 세션 marker를 붙잡아 마지막 기존 창이 종료돼도 정책이 흔들리지 않게 한다.
        if (startInMultiSharedSession && !coordinator.joinMultiSession()) {
            qCritical() << "[Instance] 기존 다중 재생 shared PCM 세션에 참가할 수 없습니다";
#ifdef _WIN32
            timeEndPeriod(1);
#endif
            return 5;
        }
    }

    MainWindow window(startInMultiSharedSession);
    coordinator.setMessageHandler([&](const InstanceCoordinator::Message& message) {
        switch (message.type) {
        case InstanceCoordinator::MessageType::OpenFiles:
            window.raise();
            window.activateWindow();
            if (!message.files.isEmpty()) window.openFiles(message.files);
            break;
        case InstanceCoordinator::MessageType::StartMultiSession:
            // 이 창을 먼저 shared PCM으로 전환하고 marker를 유지한다. 새 창은 ACK 뒤
            // 최초 MPV 초기화를 하므로 기존 exclusive stream과의 endpoint 경쟁을 피한다.
            if (coordinator.joinMultiSession()) {
                window.enableMultiInstanceSharedAudio();
            }
            break;
        }
    });

    window.show();
    if (!positional.isEmpty()) {
        // 기본 첫 실행 또는 --new-window file의 시작 파일이다. 일반 파일 더블클릭은
        // 위에서 이미 실행 중인 창으로 전달되어 이 경로에 도달하지 않는다.
        window.openFiles(positional);
    }

    const int ret = app.exec();

#ifdef _WIN32
    timeEndPeriod(1);
#endif
    return ret;
}
