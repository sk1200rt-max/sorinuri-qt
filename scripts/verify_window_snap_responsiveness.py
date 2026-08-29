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
    ("maximizeButtonRectInWindow" in title_h and "maximizeButtonRectInWindow" in title_cpp,
     "사용자 지정 최대화 버튼 영역 API가 존재해야 합니다."),
    ("startSystemMove()" in title_cpp,
     "타이틀바 드래그는 Windows 시스템 이동을 우선 사용해야 합니다."),
    ("enableWindowsSnapIntegration" in main_h and "enableWindowsSnapIntegration" in main_cpp,
     "표준 크기조절/최대화 창 스타일 복구가 있어야 합니다."),
    (all(token in main_cpp for token in ("WS_THICKFRAME", "WS_MAXIMIZEBOX", "WS_MINIMIZEBOX", "WS_SYSMENU", "SWP_FRAMECHANGED")),
     "무테 창에도 Windows Snap에 필요한 표준 창 스타일을 적용해야 합니다."),
    ("HTMAXBUTTON" in main_cpp,
     "Windows 11 Snap Layout용 HTMAXBUTTON 히트 테스트가 필요합니다."),
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
