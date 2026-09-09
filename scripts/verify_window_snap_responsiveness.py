#!/usr/bin/env python3
"""Windows Snap/유휴 반응성 회귀 검사.

이 검사는 사용자 지정 무테 창이 Windows 시스템 스냅을 우회하지 않는지,
유휴 상태에 불필요한 60fps 이벤트 폴링·시각화 타이머가 남지 않는지 확인한다.
실제 Windows GUI 동작과 GPU 사용률은 Windows CI/사용자 장비에서 추가 확인한다.
"""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]

def text(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")

checks: list[tuple[bool, str]] = []

title_h = text("src/TitleBar.h")
title_cpp = text("src/TitleBar.cpp")
main_h = text("src/MainWindow.h")
main_cpp = text("src/MainWindow.cpp")
core_h = text("src/MpvCore.h")
core_cpp = text("src/MpvCore.cpp")
music_h = text("src/MusicWidget.h")
music_cpp = text("src/MusicWidget.cpp")
compact_cpp = text("src/CompactPlayerWidget.cpp")
mpv_h = text("src/MpvWidget.h")
mpv_cpp = text("src/MpvWidget.cpp")
control_cpp = text("src/ControlBar.cpp")

checks += [
    ("startSystemMove()" in title_cpp,
     "타이틀바 드래그는 Qt의 시스템 이동을 우선 사용해야 합니다."),
    ("enableWindowsSnapIntegration" not in main_h and "enableWindowsSnapIntegration" not in main_cpp,
     "창 생성 뒤 Windows 스타일을 주입하는 이전 Snap 실험 경로가 있으면 안 됩니다."),
    ("setWindowFlags(Qt::Window);" in main_cpp and "setWindowFlags(Qt::FramelessWindowHint" not in main_cpp,
     "Snap 가능한 표준 최상위 창은 생성 시점부터 사용하고 FramelessWindowHint와 혼용하면 안 됩니다."),
    ("WM_NCCALCSIZE" in main_cpp and "m->wParam == TRUE" in main_cpp,
     "표준 프레임을 노출하지 않으려면 WM_NCCALCSIZE에서 클라이언트 영역을 확장해야 합니다."),
    ("DwmDefWindowProc" in main_cpp and "HTMAXBUTTON" not in main_cpp and
     "isMaximizeControlAt" not in main_cpp and "isInteractiveControlAt" in main_cpp,
     "사용자 지정 최대화 버튼은 HTCLIENT로 처리해 hover Snap Layout을 열면 안 됩니다."),
    ("void MainWindow::showEvent(QShowEvent* e)" in main_cpp and
     "SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |" in main_cpp and
     "SWP_NOACTIVATE | SWP_FRAMECHANGED" in main_cpp and
     "SetWindowLong" not in main_cpp and "SetWindowLongPtr" not in main_cpp,
     "프레임 재계산은 초기 표시 시 한 번만 수행하고 런타임 창 스타일 주입은 하면 안 됩니다."),
    ("isInteractiveControlAt" in title_h and "isMaximizeControlAt" not in title_h and
     "isInteractiveControlAt" in title_cpp and "isMaximizeControlAt" not in title_cpp,
     "사용자 지정 타이틀바 버튼은 공통 클릭 영역으로 처리하고 최대화 hover API는 두면 안 됩니다."),
    (all(token in title_cpp for token in (
        "constexpr int kWindowControlSide = 52",
        "btnMin_ = makeIconBtn",
        "btnMax_ = makeIconBtn",
        "btnFullscreen_ = makeIconBtn",
        "btnClose_ = makeIconBtn",
        "kWindowControlSide")),
     "최소화·최대화·전체화면·닫기는 제목 표시줄 전체 높이의 52×52 정사각형 클릭 영역이어야 합니다."),
    ("btnClose_->setFixedSize(52, 52);" in title_cpp and
     "btnClose_->setMinimumWidth(0);" not in title_cpp and
     "btnClose_->setMaximumWidth(QWIDGETSIZE_MAX);" not in title_cpp,
     "닫기 버튼은 반응형 갱신 후에도 너비 제한을 잃지 않는 52×52 정사각형이어야 합니다."),
    ("revealUiForVideoEdge" in main_h and "revealUiForVideoEdge" in main_cpp,
     "비디오의 상단·하단 가장자리 전용 UI 노출 경로가 필요합니다."),
    (all(token in main_cpp for token in ("TOP_UI_REVEAL_ZONE", "BOTTOM_UI_REVEAL_ZONE", "showTopUi()", "showBottomUi()")),
     "상단·하단 UI 노출 영역과 독립 표시 함수가 필요합니다."),
    ("if (event->type() == QEvent::MouseMove) {\n        showUI();\n    }\n    if (obj == mpvWidget_)" not in main_cpp,
     "전역 마우스 이동마다 전체 UI를 표시하면 안 됩니다."),
    ("qApp->installEventFilter(this);" in main_cpp and
     "revealUiForVideoEdge(me->globalPosition().toPoint());" in main_cpp,
     "상·하단 메뉴 위 포인터도 전역 가장자리 노출 함수로 처리해야 합니다."),
    ("fullscreenEdgePollTimer_->setInterval(100);" in main_cpp and
     "fullscreenEdgePollTimer_->setTimerType(Qt::CoarseTimer);" in main_cpp and
     "void MainWindow::syncFullscreenEdgeUi()" in main_cpp and
     "globalPosition.y() <= windowRect.top() + TOP_UI_REVEAL_ZONE" in main_cpp and
     "fullscreenPointerOnTop_" in main_h and "fullscreenPointerOnBottom_" in main_h and
     "fullscreenTopEdgeTrigger_" in main_h and
     "obj == fullscreenTopEdgeTrigger_" in main_cpp and
     "QEvent::Enter" in main_cpp,
     "전체 화면의 숨겨진 상단 가장자리는 투명 진입 트리거와 경량 폴링으로 표시하되 같은 상태의 UI 작업을 반복하면 안 됩니다."),
    ("WM_NCACTIVATE && isFullscreen_" in main_cpp and
     "DefWindowProc(m->hwnd, WM_NCACTIVATE, m->wParam, -1)" in main_cpp,
     "전체 화면 포커스 전환 중 표준 비클라이언트 프레임이 다시 그려져 MPV 화면이 번쩍이면 안 됩니다."),
    ("std::atomic_bool updateQueued_{false};" in mpv_h and
     "if (!w->updateQueued_.exchange(true))" in mpv_cpp and
     "updateQueued_.store(false);" in mpv_cpp,
     "libmpv 프레임 콜백은 Qt GUI 큐에 하나의 최신 repaint만 예약해야 합니다."),
    ("void discardHiddenFrame();" in mpv_h and
     "std::atomic_bool hiddenFrameDiscardQueued_{false};" in mpv_h and
     "void MpvWidget::discardHiddenFrame()" in mpv_cpp and
     "MPV_RENDER_PARAM_SKIP_RENDERING" in mpv_cpp and
     "QMetaObject::invokeMethod(w, \"discardHiddenFrame\", Qt::QueuedConnection);" in mpv_cpp and
     "mpv_render_context_render(renderCtx_, params);" in mpv_cpp,
     "오리지널·OTT처럼 영상이 비가시인 탭에서는 libmpv 프레임을 skip-render로 소비해 복귀 뒤 누적 프레임이 빠르게 소진되면 안 됩니다."),
    ("setCursor(Qt::BlankCursor)" not in main_cpp and
     "mpvWidget_->setCursor(Qt::BlankCursor)" not in main_cpp,
     "UI를 숨길 때 영상 영역의 마우스 포인터를 숨기면 안 됩니다."),
    ("if (cursor().shape() == Qt::BlankCursor) unsetCursor();" in main_cpp and
     "mpvWidget_->unsetCursor();" in main_cpp,
     "이전 상태에서 숨겨진 포인터를 항상 복원해야 합니다."),
    ("if (!isFullscreen_) {\n        showTopUi();\n        showBottomUi();" in main_cpp and
     "void MainWindow::positionTitleBarOverlay()" in main_cpp and
     "void MainWindow::setTitleBarOverlayMode(bool fullscreenOverlay)" in main_cpp and
     "void MainWindow::updateContentSafeInsets(bool fullscreenOverlay)" in main_cpp and
     "const int bottomInset = (!fullscreenOverlay && videoOverlayDeck_->isVisible())" in main_cpp and
     "layout->setContentsMargins(current.left(), topInset, current.right(), bottomInset);" in main_cpp and
     "updateContentSafeInsets(isFullscreen_);" in main_cpp and
     "if (!isFullscreen_ || isMusicMode_ || !isVisible()) return;" in main_cpp and
     "titleBar_->geometry().contains(position)" in main_cpp and
     "videoOverlayDeck_->geometry().contains(position)" in main_cpp and
     "uiHideTimer_->stop();" in main_cpp and
     "UI_AUTO_HIDE_DELAY_MS = 900" in main_cpp,
     "일반·최대화 창에서는 상단 제목바와 하단 컨트롤바 안전 여백을 예약하고, 실제 전체 화면에서만 overlay가 영상 위에 표시되어야 합니다."),
    (all(token in control_cpp for token in (
        "setFixedHeight(78);",
        "seekSlider_->setFixedHeight(20);",
        "QSlider::groove:horizontal { height: 6px;",
        "QSlider::handle:horizontal { width: 16px; height: 16px;",
        "QSlider::handle:horizontal:hover { width: 18px; height: 18px;")),
     "공통 영상 진행바는 6px 시각 트랙과 20px 클릭·드래그 hit 영역을 유지해야 합니다."),
    ("eventTimer_" not in core_h and "eventTimer_" not in core_cpp,
     "wakeup callback과 중복되는 16ms MPV 이벤트 폴링은 없어야 합니다."),
    ("mpv_set_wakeup_callback" in core_cpp and "QTimer::singleShot(0, self, &MpvCore::onMpvEvents)" in core_cpp,
     "libmpv wakeup callback 기반 이벤트 처리를 유지해야 합니다."),
    ("setVisualizationActive" in music_h and "setVisualizationActive" in music_cpp,
     "음악 시각화 활성 상태 제어가 필요합니다."),
    ("peakTimer_->start();" not in music_cpp.split("void MusicWidget::setupUI", 1)[0],
     "MusicWidget 생성자는 유휴 피크 타이머를 즉시 시작하면 안 됩니다."),
    ("peakTimer_->start();" not in compact_cpp.split("void CompactPlayerWidget::setupUI", 1)[0],
     "CompactPlayerWidget 생성자는 유휴 피크 타이머를 즉시 시작하면 안 됩니다."),
    ("setVisualizationActive(false)" in main_cpp and "setSpectrumEnabled(false)" in main_cpp,
     "음악 시각화는 영상 모드·일시정지·정지에서 비활성화해야 합니다."),
    ("taskbarProgressLastUpdateMs_" in main_h and
     "elapsedMs - taskbarProgressLastUpdateMs_ < 250" in main_cpp,
     "재생 위치 신호마다 Windows 작업 표시줄 COM 갱신을 반복하면 안 됩니다."),
]

failed = [message for ok, message in checks if not ok]
if failed:
    print("[FAIL] Windows Snap/반응성 회귀 검사 실패")
    for message in failed:
        print(" -", message)
    sys.exit(1)

print("[PASS] Windows Snap/반응성 회귀 검사 통과")
