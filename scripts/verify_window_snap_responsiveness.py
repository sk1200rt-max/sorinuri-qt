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

checks += [
    ("startSystemMove()" in title_cpp,
     "타이틀바 드래그는 Qt의 시스템 이동을 우선 사용해야 합니다."),
    ("enableWindowsSnapIntegration" not in main_h and "enableWindowsSnapIntegration" not in main_cpp,
     "무테 Qt 레이아웃을 깨뜨리는 런타임 Windows 창 스타일 주입이 있으면 안 됩니다."),
    (not any(token in main_cpp for token in ("WS_THICKFRAME", "WS_MAXIMIZEBOX", "WS_MINIMIZEBOX", "WS_SYSMENU", "SWP_FRAMECHANGED", "HTMAXBUTTON")),
     "무테 UI에 상단 시스템 프레임을 되살리는 Windows 스타일·최대화 히트 테스트가 있으면 안 됩니다."),
    ("maximizeButtonRectInWindow" not in title_h and "maximizeButtonRectInWindow" not in title_cpp,
     "제거된 Snap Layout 실험용 최대화 버튼 API가 남아 있으면 안 됩니다."),
    ("revealUiForVideoEdge" in main_h and "revealUiForVideoEdge" in main_cpp,
     "비디오의 상단·하단 가장자리 전용 UI 노출 경로가 필요합니다."),
    (all(token in main_cpp for token in ("TOP_UI_REVEAL_ZONE", "BOTTOM_UI_REVEAL_ZONE", "showTopUi()", "showBottomUi()")),
     "상단·하단 UI 노출 영역과 독립 표시 함수가 필요합니다."),
    ("if (event->type() == QEvent::MouseMove) {\n        showUI();\n    }\n    if (obj == mpvWidget_)" not in main_cpp,
     "전역 마우스 이동마다 전체 UI를 표시하면 안 됩니다."),
    ("revealUiForVideoEdge(me->pos());" in main_cpp,
     "비디오 마우스 이동은 가장자리 노출 함수로만 처리해야 합니다."),
    ("if (controlBar_) controlBar_->hide();\n        showTopUi();" in main_cpp and
     "if (titleBar_) titleBar_->hide();\n        showBottomUi();" in main_cpp,
     "상단·하단 가장자리에서는 반대쪽 UI를 함께 표시하면 안 됩니다."),
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
     "음악 시각화은 영상 모드·일시정지·정지에서 비활성화해야 합니다."),
    (not re.search(r"\btimeBeginPeriod\s*\(\s*1\s*\)\s*;", main_cpp),
     "전원 상태 변경에서 timeBeginPeriod(1)을 누적 호출하면 안 됩니다."),
]

failed = [message for ok, message in checks if not ok]
if failed:
    print("[FAIL] Windows Snap/반응성 회귀 검사 실패")
    for message in failed:
        print(" -", message)
    sys.exit(1)

print("[PASS] Windows Snap/반응성 회귀 검사 통과")
