# Windows Snap 및 자원 점유 조사 근거

## Windows Snap

- Microsoft는 Windows 11 Snap Layout이 최대화 버튼에 마우스를 올리거나 `Win + Z`로 표시되며, Win32 사용자 지정 타이틀바는 `WM_NCHITTEST`에서 최대화/복원 버튼 영역에 `HTMAXBUTTON`을 반환해야 Snap Layout을 제공한다고 명시한다.
- 표준 창 배치 기능은 마우스로 창을 화면 좌·우 모서리/상단으로 끌거나 `Windows 키 + 방향키`로 실행된다.
- Microsoft는 일반적인 Snap Layout 호환성을 위해 최소 창 너비를 500 effective pixels 이하, 권장은 330 이하로 제시한다. 소리누리의 640px 최소 너비는 일반적인 반분할에는 무리가 없지만, 다중 분할 레이아웃의 작은 구역 제한이 될 수 있다. 이번 수정은 기존 HiDPI 최소 크기 정책을 변경하지 않는다.

Sources:
1. https://learn.microsoft.com/en-us/windows/apps/desktop/modernize/ui/apply-snap-layout-menu
2. https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-nchittest
3. https://support.microsoft.com/en-us/windows/experience/snap-your-windows

## Timer resolution / event polling

- Microsoft는 `timeBeginPeriod`를 타이머 서비스를 사용하기 직전에 호출하고 완료 즉시 대응하는 `timeEndPeriod`를 호출하라고 권장한다.
- 낮은 타이머 간격은 스케줄러 전환을 늘리고 시스템 전체 성능과 전력 관리에 불리할 수 있다고 명시한다.
- libmpv의 공식 client API는 GUI 이벤트 루프 통합을 위해 `mpv_set_wakeup_callback()` 후 `mpv_wait_event(..., 0)` 폴링을 사용 가능하다고 설명한다. 소리누리는 이미 wakeup callback을 설정하면서 동시에 16ms polling timer도 사용하고 있어 중복 처리 구조다.

Sources:
1. https://learn.microsoft.com/en-us/windows/win32/api/timeapi/nf-timeapi-timebeginperiod
2. https://github.com/mpv-player/mpv/blob/master/include/mpv/client.h
3. https://mpv-player-mpv.mintlify.app/embedding/libmpv

## Code findings before changes

- `TitleBar::mouseMoveEvent`가 `window()->move(...)`로 직접 창을 움직여 Windows 시스템 드래그를 우회한다.
- `MainWindow::nativeEvent`의 `WM_NCHITTEST`는 리사이즈 모서리만 처리하며, 사용자 지정 최대화 버튼에 `HTMAXBUTTON`을 반환하지 않는다.
- `MpvCore`는 wakeup callback과 별도로 16ms eventTimer를 계속 실행한다.
- `MusicWidget`과 `CompactPlayerWidget`은 숨겨져도 각각 50ms, 40ms 피크 감쇠 타이머를 시작한다.
- 메인 실행 경로는 `timeBeginPeriod(1)`을 앱 전체 수명 동안 요청한다. libmpv도 Windows에서 같은 요청을 한다.

**Caution:** 오디오 채널, WASAPI exclusive, bitstream, `vo=libmpv`, GPU 재감지 정책은 이 작업에서 변경하지 않는다.
