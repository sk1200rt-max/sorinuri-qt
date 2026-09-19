#!/usr/bin/env python3
"""Windows 절전 복귀 시 libmpv OpenGL render context 복구 경로를 정적으로 검사한다."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
main = (ROOT / "src" / "MainWindow.cpp").read_text(encoding="utf-8")
widget_h = (ROOT / "src" / "MpvWidget.h").read_text(encoding="utf-8")
widget = (ROOT / "src" / "MpvWidget.cpp").read_text(encoding="utf-8")

errors: list[str] = []

for needle in [
    "void prepareForSystemSuspend();",
    "void recoverAfterSystemResume();",
    "void releaseMpvRenderContext();",
    "void performSystemResumeRecovery();",
    "std::atomic_bool systemPowerTransition_{false};",
    "bool resumeRecoveryQueued_ = false;",
]:
    if needle not in widget_h:
        errors.append(f"절전 복귀 렌더 API/상태 누락: {needle}")

for needle in [
    "PBT_APMSUSPEND",
    "mpvWidget_->prepareForSystemSuspend();",
    "PBT_APMRESUMESUSPEND",
    "PBT_APMRESUMEAUTOMATIC",
    "PBT_APMRESUMECRITICAL",
    "mpvWidget_->recoverAfterSystemResume();",
    "scheduleAudioOutputRecovery(1200);",
]:
    if needle not in main:
        errors.append(f"Windows 전원 이벤트 처리 누락: {needle}")

power_start = main.find("if (m->message == WM_POWERBROADCAST)")
power_end = main.find("// ── 오디오 기기 핫플러그 처리", power_start)
power = main[power_start:power_end] if power_start >= 0 and power_end > power_start else ""
if not power:
    errors.append("WM_POWERBROADCAST 처리 블록을 찾을 수 없습니다.")
else:
    if power.find("mpvWidget_->recoverAfterSystemResume();") > power.find("scheduleAudioOutputRecovery(1200);"):
        errors.append("절전 복귀 렌더 컨텍스트 복구가 오디오 재협상보다 먼저 예약되지 않습니다.")
    if "mpvWidget_->update();" in power:
        errors.append("절전 복귀에서 오래된 FBO를 단순 update()로 다시 사용하면 안 됩니다.")

for needle in [
    "void MpvWidget::prepareForSystemSuspend()",
    "systemPowerTransition_.exchange(true)",
    "releaseMpvRenderContext();",
    "void MpvWidget::recoverAfterSystemResume()",
    "SYSTEM_RESUME_RENDER_DELAY_MS = 750",
    "SYSTEM_RESUME_RENDER_MAX_ATTEMPTS = 16",
    "void MpvWidget::performSystemResumeRecovery()",
    "!isValid() || !context() || !context()->isValid()",
    "makeCurrent();",
    "const bool ready = initializeMpvRenderContext();",
    "systemPowerTransition_.store(false);",
    "if (w->systemPowerTransition_.load()) return;",
    "if (systemPowerTransition_.load() || !renderCtx_) return;",
]:
    if needle not in widget:
        errors.append(f"안전한 libmpv OpenGL context 복구 구현 누락: {needle}")

context_destroy_start = widget.find("void MpvWidget::onContextAboutToBeDestroyed()")
context_destroy_end = widget.find("void MpvWidget::releaseMpvRenderContext()", context_destroy_start)
context_destroy = widget[context_destroy_start:context_destroy_end]
if "makeCurrent();" not in context_destroy or "releaseMpvRenderContext();" not in context_destroy:
    errors.append("Qt context 파괴 전에는 makeCurrent() 후 render context를 해제해야 합니다.")

if errors:
    print("절전 복귀 렌더 안정화 검증 실패:", file=sys.stderr)
    for error in errors:
        print(f"- {error}", file=sys.stderr)
    raise SystemExit(1)

print("절전 복귀 렌더 안정화 검증 통과: 절전 전 callback 차단·유효 GL context 재생성·오디오 정책 재협상 순서 확인")
