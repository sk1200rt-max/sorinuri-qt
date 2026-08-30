#!/usr/bin/env python3
"""절전 복귀 다채널 출력과 새 재생 자막 스타일 적용 경로를 검사한다."""
from __future__ import annotations

from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
core = (ROOT / "src" / "MpvCore.cpp").read_text(encoding="utf-8")
header = (ROOT / "src" / "MpvCore.h").read_text(encoding="utf-8")
main = (ROOT / "src" / "MainWindow.cpp").read_text(encoding="utf-8")

errors: list[str] = []

for needle in [
    "void MpvCore::restoreAudioOutputAfterDeviceChange()",
    'mpv_set_property_string(mpv_, "audio-channels", "auto")',
    'mpv_set_property_string(mpv_, "ad-lavc-downmix", "no")',
    'mpv_set_property_string(mpv_, "audio-normalize-downmix", "no")',
    'mpv_set_property_string(mpv_, "audio-spdif",',
    'const char* reloadArgs[] = { "ao-reload", nullptr };',
]:
    if needle not in core:
        errors.append(f"절전/장치 변경 후 오디오 복구 경로 누락: {needle}")

if "void restoreAudioOutputAfterDeviceChange();" not in header:
    errors.append("오디오 출력 복구 API 선언이 없습니다.")

restore_start = core.find("void MpvCore::restoreAudioOutputAfterDeviceChange()")
restore_end = core.find("void MpvCore::setAudioDevice", restore_start)
restore = core[restore_start:restore_end] if restore_start >= 0 and restore_end > restore_start else ""
for property_name in ["audio-device", "audio-exclusive", "audio-channels", "audio-spdif"]:
    prop_index = restore.find(property_name)
    reload_index = restore.find('"ao-reload"')
    if prop_index < 0 or reload_index < 0 or prop_index > reload_index:
        errors.append(f"ao-reload 전 {property_name} 복원 순서가 보장되지 않습니다.")

for needle in [
    "PBT_APMRESUMESUSPEND",
    "PBT_APMRESUMEAUTOMATIC",
    "scheduleAudioOutputRecovery(1200)",
    "DBT_DEVICEARRIVAL_W",
    "scheduleAudioOutputRecovery(800)",
    "core()->restoreAudioOutputAfterDeviceChange()",
]:
    if needle not in main:
        errors.append(f"절전/장치 변경 오디오 재협상 경로 누락: {needle}")

if 'core->command({"ao-reload"})' in main:
    errors.append("MainWindow에 정책 복원 없는 직접 ao-reload 호출이 남아 있습니다.")

# 종료 전 명시적 종료가 WASAPI 독점 핸들의 생명주기를 Qt 객체 소멸보다 앞서게 한다.
for needle in [
    "void MpvCore::shutdown()",
    "mpv_set_wakeup_callback(mpv_, nullptr, nullptr)",
    "mpv_terminate_destroy(mpv_)",
    "mpv_ = nullptr",
    "void MpvWidget::shutdown()",
    "mpv_render_context_free(renderCtx_)",
    "if (shutdownStarted_ || renderCtx_ || !context()) return;",
    "core_->shutdown()",
    "if (mpvWidget_) mpvWidget_->shutdown()",
]:
    if needle not in core + header + main + (ROOT / "src" / "MpvWidget.cpp").read_text(encoding="utf-8") + (ROOT / "src" / "MpvWidget.h").read_text(encoding="utf-8"):
        errors.append(f"종료 뒤 외부 앱 오디오 해제 경로 누락: {needle}")

close_start = main.find("void MainWindow::closeEvent")
close_end = main.find("void MainWindow::dragEnterEvent", close_start)
close_event = main[close_start:close_end] if close_start >= 0 and close_end > close_start else ""
if "mpvWidget_->shutdown()" not in close_event or close_event.find("mpvWidget_->shutdown()") > close_event.find("e->accept()"):
    errors.append("창 종료 승인 전에 MpvWidget 종료가 완료되지 않습니다.")

for needle in [
    "void MpvCore::applyStoredSubtitleStyle()",
    'settings.value("subtitle/size", 36)',
    'mpv_set_property_string(mpv_, "sub-font-size",',
    "applyStoredSubtitleStyle();",
    "QTimer::singleShot(150, this",
]:
    if needle not in core:
        errors.append(f"새 재생 자막 스타일 복원 경로 누락: {needle}")

file_loaded_start = core.find("case MPV_EVENT_FILE_LOADED")
file_loaded_end = core.find("case MPV_EVENT_PLAYBACK_RESTART", file_loaded_start)
file_loaded = core[file_loaded_start:file_loaded_end] if file_loaded_start >= 0 else ""
if "applyStoredSubtitleStyle();" not in file_loaded:
    errors.append("FILE_LOADED에서 저장된 자막 스타일을 적용하지 않습니다.")

if errors:
    print("절전 복귀 오디오·자막 수명주기 검증 실패:", file=sys.stderr)
    for error in errors:
        print(f"- {error}", file=sys.stderr)
    raise SystemExit(1)

print("절전 복귀·종료 오디오·자막 수명주기 검증 통과: 원본 채널 재협상·WASAPI 동기 해제·새 파일 자막 스타일 재적용 확인")
