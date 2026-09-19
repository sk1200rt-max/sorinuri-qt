#!/usr/bin/env python3
"""절전 복귀 뒤 WASAPI 멀티채널 PCM 복구 경로를 정적으로 검사한다."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
main = (ROOT / "src" / "MainWindow.cpp").read_text(encoding="utf-8")
main_h = (ROOT / "src" / "MainWindow.h").read_text(encoding="utf-8")
core = (ROOT / "src" / "MpvCore.cpp").read_text(encoding="utf-8")
core_h = (ROOT / "src" / "MpvCore.h").read_text(encoding="utf-8")

errors: list[str] = []

for needle in [
    "bool hasUnexpectedStereoFallbackForMultichannelContent() const;",
    "bool hasActiveMultichannelPcmContent() const;",
]:
    if needle not in core_h:
        errors.append(f"멀티채널 PCM 상태 API 누락: {needle}")

for needle in [
    "bool MpvCore::hasActiveMultichannelPcmContent() const",
    '"audio-params/channel-count"',
    '"audio-out-params/channel-count"',
    '"audio-out-params/format"',
    'outputFormat.contains("spdif", Qt::CaseInsensitive)',
    "bool MpvCore::hasUnexpectedStereoFallbackForMultichannelContent() const",
    "const bool unexpectedStereo = outputChannels <= 2;",
]:
    if needle not in core:
        errors.append(f"멀티채널 PCM fallback 판별 구현 누락: {needle}")

for needle in [
    "void beginSleepAudioOutputRecovery();",
    "void cancelSleepAudioOutputRecovery();",
    "void finishSleepAudioOutputRecovery();",
    "sleepAudioStabilizationTimer_",
    "sleepAudioVerificationTimer_",
    "sleepAudioFinalCheckTimer_",
    "sleepAudioRecoveryPending_",
    "sleepAudioRecoveryFinalRetries_",
]:
    if needle not in main_h:
        errors.append(f"절전 멀티채널 복구 상태/타이머 누락: {needle}")

for needle in [
    '"audio/sleep_multichannel_recovery_pending"',
    "void MainWindow::beginSleepAudioOutputRecovery()",
    "scheduleAudioOutputRecovery(1800);",
    "sleepAudioStabilizationTimer_->start(5200);",
    "sleepAudioVerificationTimer_->start(8200);",
    "hasUnexpectedStereoFallbackForMultichannelContent()",
    "hasActiveMultichannelPcmContent()",
    "restoreAudioOutputAfterDeviceChange();",
    "void MainWindow::finishSleepAudioOutputRecovery()",
    "settings_.remove(\"audio/sleep_multichannel_recovery_pending\")",
    "void MainWindow::cancelSleepAudioOutputRecovery()",
    "beginSleepAudioOutputRecovery();",
]:
    if needle not in main:
        errors.append(f"절전 복귀 WASAPI 재협상 경로 누락: {needle}")

power_start = main.find("if (m->message == WM_POWERBROADCAST)")
power_end = main.find("// ── 오디오 기기 핫플러그 처리", power_start)
power = main[power_start:power_end] if power_start >= 0 and power_end > power_start else ""
if "cancelSleepAudioOutputRecovery();" not in power:
    errors.append("절전 진입 때 대기 중인 멀티채널 복구 타이머를 취소하지 않습니다.")
if "beginSleepAudioOutputRecovery();" not in power:
    errors.append("절전 복귀 때 단계적 멀티채널 복구를 시작하지 않습니다.")

file_loaded_start = main.find("void MainWindow::onFileLoaded")
file_loaded_end = main.find("namespace {", file_loaded_start)
file_loaded = main[file_loaded_start:file_loaded_end] if file_loaded_start >= 0 and file_loaded_end > file_loaded_start else ""
if "sleepAudioRecoveryPending_ && !sleepAudioRecoveryScheduled_" not in file_loaded:
    errors.append("재실행 후 첫 파일 로드에서 보류된 멀티채널 복구를 재개하지 않습니다.")

audio_format_start = main.find("void MainWindow::onAudioFormatChanged")
audio_format_end = main.find("void MainWindow::onVideoInfoChanged", audio_format_start)
audio_format = main[audio_format_start:audio_format_end] if audio_format_start >= 0 and audio_format_end > audio_format_start else ""
for needle in [
    "sleepAudioRecoveryPending_ && !sleepAudioRecoveryScheduled_",
    "hasActiveMultichannelPcmContent()",
    "beginSleepAudioOutputRecovery();",
]:
    if needle not in audio_format:
        errors.append(f"지연된 멀티채널 포맷 도착 복구가 누락되었습니다: {needle}")

close_start = main.find("void MainWindow::closeEvent")
close_end = main.find("void MainWindow::dragEnterEvent", close_start)
close_event = main[close_start:close_end] if close_start >= 0 and close_end > close_start else ""
if "cancelSleepAudioOutputRecovery();" not in close_event:
    errors.append("종료 시 멀티채널 복구 타이머를 중단하지 않습니다.")

# 정책 회귀: stereo 고정, decoder downmix, downmix normalization을 recovery에 넣지 않는다.
recovery_start = main.find("void MainWindow::beginSleepAudioOutputRecovery()")
recovery_end = main.find("void MainWindow::cancelSleepAudioOutputRecovery()", recovery_start)
recovery = main[recovery_start:recovery_end] if recovery_start >= 0 and recovery_end > recovery_start else ""
for forbidden in ["audio-channels\", \"stereo", "ad-lavc-downmix\", \"yes", "audio-normalize-downmix\", \"yes"]:
    if forbidden in recovery:
        errors.append(f"절전 멀티채널 복구가 금지된 다운믹스 정책을 설정합니다: {forbidden}")

if errors:
    print("절전 복귀 멀티채널 WASAPI 검증 실패:", file=sys.stderr)
    for error in errors:
        print(f"- {error}", file=sys.stderr)
    raise SystemExit(1)

print("절전 복귀 멀티채널 WASAPI 검증 통과: PCM 2.0 fallback 감지·단계적 재협상·재실행 후 복구 재개·원본 채널 정책 유지 확인")
