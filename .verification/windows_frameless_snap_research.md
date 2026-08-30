# 무테 Qt 창과 Windows Snap 조사 기록

## 사용자 확인 사항

- v6.19.9의 `WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU` 런타임 주입과 `SWP_FRAMECHANGED`는 상단 흰 시스템 바와 Qt 레이아웃 잘림을 유발했다.
- 이 스타일 주입을 제거한 v6.20.0 테스트 패키지에서는 기존 무테 UI는 복구되지만 Windows 키+방향키 및 화면 가장자리 끌기 Snap은 동작하지 않는다.

## 공식·검증된 근거

1. Microsoft는 custom title bar에서 Snap Layout 메뉴를 위해 최대화 버튼 영역에 `WM_NCHITTEST`의 `HTMAXBUTTON`을 반환하라고 안내한다. 다만 Snap 자체가 제대로 동작하려면 창의 최대화 캡션 버튼과 스냅 가능한 창 스타일도 필요하다.
   - https://learn.microsoft.com/en-us/windows/apps/desktop/modernize/ui/apply-snap-layout-menu

2. Microsoft Q&A의 Win32 답변은 Snap에 `WS_THICKFRAME | WS_MAXIMIZEBOX`가 필요하고, 제목 표시줄·프레임을 보이지 않게 하려면 DWM custom frame 또는 `WM_NCCALCSIZE`와 `WM_NCHITTEST` 처리를 사용해야 한다고 설명한다.
   - https://learn.microsoft.com/en-us/answers/questions/2120539/how-to-disable-frame-and-titlebar-but-keep-resizin

3. `WM_NCHITTEST`는 custom frame caption button을 구현할 경우 `DwmDefWindowProc`에 먼저 넘겨 DWM의 caption button hit testing을 유지하도록 Microsoft가 권고한다.
   - https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-nchittest

4. QWindowKit은 Qt Widgets/Qt Quick용 Apache-2.0 라이선스 프레임리스 윈도우 프레임워크로, Windows Snap Layout·이동·크기 조절·시스템 메뉴를 목표 기능으로 제공한다. Qt 6.6.2 이상을 권장하고 Qt private API 의존성 및 런타임 frame switching 금지를 명시한다. 외부 라이브러리를 바로 도입하지 않고 해당 구조를 참고하되, 소리누리 Qt 버전과 배포 DLL 영향 검증이 필요하다.
   - https://github.com/stdware/qwindowkit

## 후속 설계 원칙

- `Qt::FramelessWindowHint` 위에서 사후 WS 스타일만 주입하지 않는다.
- Snap에 필요한 `WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU`는 창이 생성되기 전에 보존하고, `WM_NCCALCSIZE`로 비클라이언트 영역을 클라이언트로 확장하여 소리누리의 다크 커스텀 타이틀바가 전체 상단을 사용하게 한다.
- `WM_NCHITTEST`는 먼저 `DwmDefWindowProc`에 전달한 뒤, 리사이즈 테두리·최대화 버튼·타이틀바 드래그 영역을 안전하게 판정한다.
- 표준 프레임을 지운 상태에서 Windows 키 조합을 앱이 재구현하지 않는다. Windows 셸이 표준 Snap을 처리하도록 필요한 Win32 스타일과 메시지 처리를 제공한다.
- HiDPI 다중 모니터와 최대화 상태에서 비클라이언트 계산이 달라지므로 Windows 실제 장비 테스트 전 production 승격을 금지한다.

## 추가 설계 확인

현재 설치 연결 26개 형식과 런타임이 실제로 지원 목록에 넣는 형식을 대조한 결과, `.mpg`, `.mpeg`, `.dts`, `.ac3`, `.truehd`, `.alac`, `.aiff`, `.aif`, `.wv`, `.dsd`가 설치 등록·Capabilities·Open With 지원 형식에서 빠져 있다. 이 10개를 더하면 코드에 명시된 오디오·비디오 형식의 합집합은 36개가 된다.

Microsoft Default Programs 문서는 Capabilities 하위의 FileAssociations, application-specific ProgID, RegisteredApplications 및 Applications\\Sorinuri.exe\\SupportedTypes를 모두 등록해야 기본 앱 후보·Open With 목록에 표시된다고 설명한다. 설치 프로그램은 UserChoice 기본값을 강제할 수 없으므로, 파일 연결 체크를 사용자가 선택한 경우에만 설치 후 소리누리의 해당 기본 앱 설정 화면을 열어 사용자가 Windows의 ‘기본값으로 설정’을 한 번만 누르도록 한다.

DWM Custom Frame 문서는 Snap에 필요한 표준 창 스타일을 유지한 채 WM_NCCALCSIZE에서 클라이언트 영역을 창 전체로 확장하고, WM_NCHITTEST를 DwmDefWindowProc에 먼저 전달한 뒤 사용자 타이틀바의 리사이즈·캡션·최대화 버튼 영역을 판정하도록 안내한다. 따라서 기존의 FramelessWindowHint 창에 스타일을 사후 주입하는 방법은 폐기한다. 다음 구현은 처음부터 Snap 가능한 표준 스타일을 유지한 창을 만들고, WM_NCCALCSIZE로 시스템 제목 표시줄을 제거하며, 완전히 숨겨진 소리누리 타이틀바와 DWM hit test를 함께 사용한다.

Sources: https://learn.microsoft.com/en-us/windows/apps/desktop/modernize/ui/apply-snap-layout-menu ; https://learn.microsoft.com/en-us/windows/win32/dwm/customframe ; https://learn.microsoft.com/en-us/windows/win32/shell/default-programs ; https://learn.microsoft.com/en-us/windows/win32/shell/app-registration

## 파일 연결 공식 근거

Microsoft Default Programs 문서에 따르면 각 파일 형식은 application-specific ProgID와 `Capabilities\\FileAssociations` 값으로 선언하고, 애플리케이션의 Capabilities 경로를 `HKLM\\Software\\RegisteredApplications`에 등록해야 Windows 기본 앱 후보가 된다. Microsoft Application Registration 문서는 Explorer ‘연결 프로그램’ 목록을 위해 `HKLM\\Software\\Classes\\Applications\\Sorinuri.exe\\shell\\open\\command`와 `SupportedTypes`를 함께 등록할 수 있다고 안내한다. 기존 사용자의 UserChoice 기본값은 Windows 10/11에서 설치 프로그램이 강제 변경할 수 없으므로, 연결 체크를 선택한 사용자가 기본 앱 설정 화면에서 소리누리를 선택·확정하는 흐름을 사용한다.

Sources: https://learn.microsoft.com/en-us/windows/win32/shell/default-programs ; https://learn.microsoft.com/en-us/windows/win32/shell/app-registration ; https://learn.microsoft.com/en-us/windows/win32/shell/fa-progids

## DD+ / WASAPI 패스스루 조사 근거

mpv stable manual은 `audio-spdif` 목록으로 AC-3, E-AC-3, DTS, DTS-HD, TrueHD를 비트스트림 출력 대상으로 설정하고 WASAPI의 exclusive mode가 직접 오디오 장치 출력을 지원한다고 설명한다. 또한 런타임에 바꾼 파일-local 옵션은 다음 파일 재생 시 초기화될 수 있으므로, 지속되어야 하는 오디오 정책은 파일 로드·절전 복귀 시점에 저장된 설정을 재적용해야 한다.

Source: https://mpv.io/manual/stable/

mpv 공식 매뉴얼의 핵심 제약: `audio-device=auto`는 기본 장치에서 지원 AO를 자동 시도하며, 지정 장치는 `audio-device-list`에 표시된 정확한 이름을 사용해야 한다. `audio-exclusive=yes`는 WASAPI 등 일부 AO에서만 동작한다. `audio-spdif=ac3,eac3,dts,dts-hd,truehd`는 HDMI와 S/PDIF 모두에 적용된다. 또한 `ao`와 `audio-device`를 함께 강제하지 말아야 한다. 현재 소리누리는 `ao=wasapi`를 고정하므로, 사용자 저장 HDMI endpoint가 있으면 초기화 시 명시적으로 `wasapi/<device>` 형식으로 적용해 auto 장치 선택에 의존하지 않도록 해야 한다.

Source: https://mpv.io/manual/stable/ (audio-device, audio-exclusive, audio-spdif sections)
