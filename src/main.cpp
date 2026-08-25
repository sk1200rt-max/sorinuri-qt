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
//
// 참고: https://stackoverflow.com/questions/16823372
// NVIDIA: NvOptimusEnablement = 1 → 전용 GPU 강제
// AMD:    AmdPowerXpressRequestHighPerformance = 1 → 전용 GPU 강제
// ══════════════════════════════════════════════════════════════════
#ifdef _WIN32
extern "C" {
    // NVIDIA Optimus: 전용 GPU 강제 (노트북 Intel+NVIDIA 듀얼 GPU)
    __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
    // AMD PowerXpress: 전용 GPU 강제 (노트북 Intel+AMD 듀얼 GPU)
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#include <windows.h>
#include <timeapi.h>  // timeBeginPeriod / timeEndPeriod
#pragma comment(lib, "winmm.lib")
#endif

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QPalette>
#include <QColor>
#include <QSurfaceFormat>
#include <QStringList>
#include <QUrl>
#include "MainWindow.h"
#include <QSharedMemory>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QLocalServer>
#include <QLocalSocket>
#include <QDataStream>

static const QString IPC_SERVER_NAME = "SorinuriIPC_v1";

#ifdef _WIN32
// Windows 10/11은 UserChoice 해시로 보호되므로 설치 프로그램이 레지스트리만
// 써서 기본 앱을 강제할 수 없다. 최신 Windows에서 호환되는 기본 앱 설정 화면을
// 열어 사용자가 소리누리를 명시적으로 기본 재생 프로그램으로 선택하게 한다.
static void launchFileAssociationUI()
{
    // 관리자 설치는 HKLM\Software\RegisteredApplications에 소리누리를 등록한다.
    // Windows 11 21H2(2023-04 업데이트) 이상에서는 registeredAppMachine 딥링크로
    // ‘소리누리’의 개별 확장자 기본값 화면을 바로 열 수 있다.
    const QString uri = QStringLiteral("ms-settings:defaultapps?registeredAppMachine=")
        + QString::fromLatin1(QUrl::toPercentEncoding(QStringLiteral("소리누리")));
    const auto result = ShellExecuteW(
        nullptr, L"open", reinterpret_cast<LPCWSTR>(uri.utf16()), nullptr, nullptr, SW_SHOWNORMAL);
    // 이전 Windows 10 빌드는 세부 앱 URI를 인식하지 않을 수 있으므로 기본 페이지로 폴백한다.
    if (reinterpret_cast<INT_PTR>(result) <= 32) {
        ShellExecuteW(nullptr, L"open", L"ms-settings:defaultapps", nullptr, nullptr, SW_SHOWNORMAL);
    }
}
#endif

int main(int argc, char* argv[])
{
#ifdef _WIN32
    // ── Windows 타이머 해상도 1ms로 설정 ─────────────────────────────
    // 기본값 15.6ms → MPV의 display-resample 프레임 타이밍이 부정확해짐
    // 노트북에서 Balanced 전원 모드일 때 특히 심각한 끊김 유발
    // timeBeginPeriod(1): 타이머 해상도를 1ms로 낮춰 정밀한 프레임 타이밍 확보
    // 참고: https://docs.microsoft.com/en-us/windows/win32/api/timeapi/nf-timeapi-timebeginperiod
    timeBeginPeriod(1);
#endif

    // ── 깜빡임 수정: 포커스 전환 시 OpenGL 컨텍스트 재생성 방지 ──
    // QApplication 생성 전에 반드시 설정해야 효과 있음
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    // ── OpenGL 포맷 사전 설정 ─────────────────────────────────────
    // QApplication 생성 전에 설정해야 OpenGL 컨텍스트 협상 시간 단축
    //
    // 노트북 호환성 수정:
    //   - TripleBuffer → DoubleBuffer: 통합 GPU에서 TripleBuffer 요청이
    //     드라이버에 의해 무시되거나 충돌하여 끊김 발생. DoubleBuffer가 안전.
    //   - swapInterval=1: Qt VSync만 사용 (MPV는 opengl-swapinterval=0으로 비활성화)
    QSurfaceFormat fmt;
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setSwapBehavior(QSurfaceFormat::DoubleBuffer);  // 노트북 통합 GPU 호환성
    fmt.setSwapInterval(1);  // Qt VSync만 사용 (이중 VSync 충돌 방지)
    // Qt6 공식 권장: depth/stencil 버퍼 명시 요청
    // depth 24: 일부 Intel GPU에서 depth buffer 미설정 시 depth testing 실패 방지
    // stencil 8: 스텐슬 버퍼 요청 (클리핑 마스크 등)
    fmt.setDepthBufferSize(24);
    fmt.setStencilBufferSize(8);
    QSurfaceFormat::setDefaultFormat(fmt);

    // ── Qt 플러그인 경로 고정 (포터블 실행 속도 핵심) ─────────────
    // QApplication 생성 전에 호출해야 플러그인 탐색 경로가 고정됨
    QApplication::addLibraryPath(QDir::currentPath());

    // ── QApplication 생성 ─────────────────────────────────────────
    QApplication app(argc, argv);
    app.setApplicationName("Sorinuri");
    app.setApplicationDisplayName("소리누리");
    app.setApplicationVersion("6.19.4");
    app.setOrganizationName("Sorinuri");
    app.setWindowIcon(QIcon(":/icons/sorinuri.ico"));

    // ── 멀티 인스턴스 / LocalSocket IPC 처리 ────────────────────────
    QCommandLineParser parser;
    parser.addHelpOption();
    QCommandLineOption newWindowOption(QStringList() << "n" << "new-window", "새 창에서 실행합니다.");
    QCommandLineOption registerAssociationsOption(
        "register-file-associations",
        "Windows 기본 앱 선택 화면을 열어 소리누리 파일 연결을 설정합니다.");
    parser.addOption(newWindowOption);
    parser.addOption(registerAssociationsOption);
    parser.addPositionalArgument("file", "재생할 파일 경로");
    parser.process(app);

    // 설치 프로그램의 파일 연결 작업 전용 경로: 메인 창·단일 인스턴스·MPV는 생성하지 않는다.
    if (parser.isSet(registerAssociationsOption)) {
#ifdef _WIN32
        launchFileAssociationUI();
#endif
#ifdef _WIN32
        timeEndPeriod(1);
#endif
        return 0;
    }

    QSharedMemory sharedMem("Sorinuri_Instance");
    bool isNewWindow = parser.isSet(newWindowOption);
    if (!isNewWindow && !sharedMem.create(1)) {
        // 이미 실행 중인 인스턴스가 있음 → LocalSocket으로 파일 전달 후 종료
        QLocalSocket sock;
        sock.connectToServer(IPC_SERVER_NAME);
        if (sock.waitForConnected(1500)) {
            QStringList positional = parser.positionalArguments();
            QByteArray data;
            QDataStream ds(&data, QIODevice::WriteOnly);
            ds << positional;
            // 길이 프리픽스 포함 전송
            QByteArray packet;
            QDataStream ps(&packet, QIODevice::WriteOnly);
            ps << (quint32)data.size();
            ps.writeRawData(data.constData(), data.size());
            sock.write(packet);
            sock.flush();
            sock.waitForBytesWritten(1500);
        }
        return 0;
    }

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

    // ── LocalSocket IPC 서버 (단일 인스턴스 파일 수신) ────────────
    QLocalServer::removeServer(IPC_SERVER_NAME);  // 이전 소켓 정리
    QLocalServer ipcServer;
    ipcServer.listen(IPC_SERVER_NAME);
    QObject::connect(&ipcServer, &QLocalServer::newConnection, [&]() {
        QLocalSocket* client = ipcServer.nextPendingConnection();
        // 누적 버퍼: readyRead가 여러 번 호출될 수 있음 (분할 수신 처리)
        // 헤더(4바이트 길이) + 데이터 전체가 도착할 때까지 누적
        QByteArray* accumBuf = new QByteArray();
        QObject::connect(client, &QLocalSocket::readyRead, [&window, client, accumBuf]() {
            accumBuf->append(client->readAll());
            // 헤더(4바이트) 수신 대기
            if (accumBuf->size() < 4) return;
            QDataStream ps(*accumBuf);
            quint32 len = 0;
            ps >> len;
            // 전체 패킷이 도착할 때까지 대기
            if ((quint32)accumBuf->size() < 4 + len) return;
            QByteArray data = accumBuf->mid(4, len);
            QDataStream ds(data);
            QStringList files;
            ds >> files;
            if (!files.isEmpty()) {
                // 창을 앞으로 가져오고 파일 열기
                window.raise();
                window.activateWindow();
                window.openFiles(files);
            }
            delete accumBuf;
            client->deleteLater();
        });
        QObject::connect(client, &QLocalSocket::disconnected, [client, accumBuf]() {
            delete accumBuf;
            client->deleteLater();
        });
    });

    // ── 커맨드라인 파일/URL 인수 처리 ────────────────────────────
    // QFile::exists() 체크 제거: 한글/공백/UNC 경로에서 false 반환하는 버그 수정
    // openFiles() 내부에서 MPV가 직접 파일 존재 여부를 처리하므로 여기서 체크 불필요
    QStringList positional = parser.positionalArguments();
    if (!positional.isEmpty()) {
        window.openFiles(positional);
    }

    int ret = app.exec();

#ifdef _WIN32
    // ── 타이머 해상도 복원 ────────────────────────────────────────
    timeEndPeriod(1);
#endif

    return ret;
}
