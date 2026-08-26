# 설치·첫 실행 반응성 조사 기록

## 확인일

2026-08-26

## 사용자 보고

- 노트북에서 설치 프로그램을 실행한 뒤 설치 화면이 늦게 표시된다.
- 소리누리 실행 뒤 첫 창이 늦게 표시된다.

## 코드 조사 결과

| 구간 | 현재 동작 | 평가 |
|---|---|---|
| Inno Setup 압축 | `lzma2/normal`, `SolidCompression=no` 사용 | 기존 고압축 대비 해제 메모리 부담을 낮춘 상태다. |
| 설치 시작 | `CloseApplications=yes`와 `CloseApplicationsFilter=Sorinuri.exe`로 Restart Manager가 기존 실행 중인 소리누리를 탐지한다. | 기존 앱이 실행 중이면 설치 준비 단계가 지연될 수 있으나, 업그레이드 안정성에 필요하다. |
| 런타임 설치 | `vc_redist.x64.exe`는 파일 복사 후 별도 `[Run]` 단계에서 조용히 실행된다. | 설치 화면이 뜨기 전 지연 원인은 아니다. |
| 첫 창 UI | `MainWindow`가 `setupUI()`에서 `MpvWidget`, ControlBar, TrackSelector, 오버레이를 동기 생성한다. | 첫 창 표시 전 위젯 트리 생성 비용이 존재한다. |
| 첫 OpenGL 프레임 | `MpvWidget::initializeGL()`이 `MpvCore::initialize()`를 즉시 호출한다. | GPU/GL 컨텍스트, libmpv, WASAPI, 렌더링 환경 감지와 다수 mpv 옵션 설정이 첫 프레임을 막는다. |
| 시작 뒤 작업 | yt-dlp, 원격 서버, SMTC, 업데이트 확인, 광고 요청은 0.5~5초 뒤로 이미 지연된다. | 즉시 첫 창 지연의 직접 원인이 아니다. |

## 외부 근거

| 주제 | 확인 내용 | 출처 |
|---|---|---|
| Inno Setup 압축 | 높은 압축 수준은 압축·해제 시간을 늘리고 메모리 요구량도 높일 수 있다. `lzma2/normal`은 지원되는 일반 수준이다. | [Inno Setup: Compression](https://jrsoftware.org/ishelp/topic_setup_compression.htm) |
| 기존 앱 종료 감지 | `CloseApplications=yes`는 Windows Restart Manager로 업데이트 대상 파일을 사용하는 앱을 감지하고 설치 준비 단계에서 종료를 요청한다. | [Inno Setup: CloseApplications](https://jrsoftware.org/ishelp/topic_setup_closeapplications.htm) |
| Qt OpenGL 초기화 | `QOpenGLWidget::initializeGL()`은 첫 표시 전에 OpenGL 리소스를 만드는 시점이며, 컨텍스트가 current인 상태에서 리소스를 설정해야 한다. | [Qt 6: QOpenGLWidget](https://doc.qt.io/qt-6/qopenglwidget.html) |
| SmartScreen | Windows는 인터넷에서 내려받은 앱·설치 프로그램의 파일/서명 평판을 확인한다. 미서명 또는 자체 서명 파일은 새 버전마다 평판을 이어받지 못할 수 있다. | [Microsoft: SmartScreen reputation](https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/smartscreen-reputation) |

## 안전한 개선 방향

1. `MpvWidget`의 OpenGL 컨텍스트 생성은 Qt 수명주기에서 유지하되, libmpv·WASAPI·렌더링 환경의 무거운 초기화를 첫 빈 프레임이 합성된 다음 이벤트 루프로 지연한다.
2. 초기화 완료 전 파일 열기 요청은 기존 `pendingStartupFiles_` 경로에 보관하고 `mpvInitialized` 후 재생한다.
3. 오디오 출력 정책, GPU 전용 선택, 파일 연결, 설치 압축·기존 실행 앱 종료 정책은 변경하지 않는다.
4. 설치 파일 실행 전 보안 평판 검사 구간은 앱 코드로 제거할 수 없으며, 장기적으로 일관된 공인 서명 또는 Microsoft Artifact Signing이 필요하다.

## Inno Setup 추가 확인

- `LZMAUseSeparateProcess`는 설치 프로그램 실행 시의 압축 해제가 아니라 **설치 패키지를 빌드할 때의 압축 프로세스**에 영향을 주므로 사용자 설치 화면 지연 개선 수단이 아니다. [Inno Setup: LZMAUseSeparateProcess](https://jrsoftware.org/ishelp/topic_setup_lzmauseseparateprocess.htm)
- 현재 설정한 `SolidCompression=no`는 파일별 임의 접근을 가능하게 해, solid 압축에서 발생하는 불필요한 선행 데이터 해제와 재시도 비용을 피한다. [Inno Setup: SolidCompression](https://jrsoftware.org/ishelp/topic_setup_solidcompression.htm)

따라서 설치 화면이 실행 파일을 연 직후 늦게 뜨는 현상은 현재 압축 설정을 더 바꾸는 것보다, Windows가 인터넷에서 내려받은 신규 실행 파일의 SmartScreen/Defender 평판 검사를 마치는 시간과 관련될 가능성을 분리해서 봐야 한다. 앱 코드 변경으로는 해당 보안 검사 단계를 제거하지 않는다.

## 상용 플레이어 대비 추가 분석

### 확인된 차이

소리누리는 `MpvWidget` 생성자에서 `MpvCore`를 즉시 만들고, Windows 실행 파일은 `libmpv-2.dll`을 일반 import library로 연결한다. 따라서 기존 구조에서는 첫 창을 표시하기 전에 Windows 로더가 libmpv 및 종속 미디어 DLL을 적재하고, 이어서 `MpvCore::initialize()`가 GPU/WASAPI/렌더링 환경을 설정한다. 반면 상용 플레이어는 일반적으로 재생 엔진과 코덱 모듈을 첫 창 표시 이후 또는 재생 요청 시점에 적재한다.

### 추가 최적화 후보

| 후보 | 근거 | 안전 조건 |
|---|---|---|
| `libmpv-2.dll` delay-load | MSVC `/DELAYLOAD`는 지정 DLL을 해당 DLL 함수의 첫 호출 때까지 적재하지 않는다. | `delayimp.lib`를 링크하고, libmpv 첫 호출이 첫 창이 표시된 이후의 지연 초기화 경로에만 남도록 확인한다. |
| 첫 창 이후 mpv 객체 생성 | 현재 `MpvCore` 생성자가 `mpv_create()`를 즉시 호출한다. | 기존 `pendingStartupFiles_`와 `mpvInitialized` 신호로 외부 파일 전달·자동 재생을 보존한다. |
| 부가 기능 매니저 지연 생성 | 초기 `MainWindow`는 광고·클라우드·Last.fm·SMTC 객체와 다수 컨트롤을 동기 구성한다. | 재생 엔진과 직접 관계없는 기능만 창 표시 뒤로 미루며, 사용자 요청 시 즉시 생성한다. |

### 공식 근거

MSVC의 `/DELAYLOAD:<dll>`는 프로그램이 해당 DLL 함수를 **처음 호출할 때까지** 지정 DLL의 적재를 미룬다. 기본 helper를 사용할 경우 `delayimp.lib` 링크가 필요하다. [Microsoft: /DELAYLOAD](https://learn.microsoft.com/en-us/cpp/build/reference/delayload-delay-load-import?view=msvc-170) [Microsoft: Delay-loaded DLL linker support](https://learn.microsoft.com/en-us/cpp/build/reference/linker-support-for-delay-loaded-dlls?view=msvc-170)

지연 적재 중 DLL을 못 찾으면 런타임 예외가 발생할 수 있으므로, 설치·포터블 패키지의 `libmpv-2.dll` 포함 검증은 유지해야 하며 첫 호출은 현재의 명시적인 `MpvCore::initialize()` 경로에 한정한다.
