# v6.19.9 무테 창 Snap 회귀 조사

## 사용자 관찰

테스트 전용 v6.19.9에서 상단 흰색 Windows 시스템 바가 나타나고, 창의 기존 다크 무테 UI가 잘리거나 위치가 어긋난다.

## 직접 원인

`MainWindow::enableWindowsSnapIntegration()`이 창을 표시한 뒤 `WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU`를 HWND에 직접 추가하고 `SWP_FRAMECHANGED`를 호출한다. Qt의 `Qt::FramelessWindowHint` 레이아웃은 클라이언트 영역이 창 전체라고 가정하므로, Windows가 새 비클라이언트 프레임을 추가하면 기존 Qt 레이아웃의 상단·가장자리 좌표와 충돌한다.

## 안전한 즉시 조치

1. `enableWindowsSnapIntegration()` 및 `showEvent()` 호출을 제거하고, `WS_*` 스타일을 런타임에 보강하지 않는다.
2. `WM_NCHITTEST`의 `HTMAXBUTTON` 사용자 지정 최대화 버튼 처리를 제거한다. 표준 비클라이언트 프레임을 되살리지 않는 이상 Snap Layout hover만으로는 안정적으로 지원할 수 없다.
3. `TitleBar::mousePressEvent()`의 `QWindow::startSystemMove()` 시도는 상단·가장자리 끌기 스냅을 지원할 수 있으며 표준 프레임을 노출하지 않으므로 별도 실장비 테스트 대상으로 유지한다.
4. `Windows 키 + 방향키`와 Windows 11 최대화 버튼 hover Snap Layout은 무테 Qt Widgets 창에서 표준 Windows 프레임을 완전히 되살리지 않고 보장할 수 없다. 기존 UI가 무너지지 않는 것을 최우선으로 한다.

## 검증 원칙

정식 production은 v6.19.7에 유지된다. 후속 수정은 테스트 전용 경로에서 상단 흰색 바·UI 잘림이 없는 것을 실장비로 먼저 확인한 뒤에만 정식 승격한다.
