#!/usr/bin/env python3
"""다중 소리누리 창의 shared PCM 정책과 단일 실행 파일 전달 회귀를 차단한다."""
from __future__ import annotations

from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]
main = (root / "src" / "main.cpp").read_text(encoding="utf-8")
coordinator_h = (root / "src" / "InstanceCoordinator.h").read_text(encoding="utf-8")
coordinator_cpp = (root / "src" / "InstanceCoordinator.cpp").read_text(encoding="utf-8")
core_h = (root / "src" / "MpvCore.h").read_text(encoding="utf-8")
core_cpp = (root / "src" / "MpvCore.cpp").read_text(encoding="utf-8")
window_h = (root / "src" / "MainWindow.h").read_text(encoding="utf-8")
window_cpp = (root / "src" / "MainWindow.cpp").read_text(encoding="utf-8")
settings_cpp = (root / "src" / "SettingsDialog.cpp").read_text(encoding="utf-8")
cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")

required = {
    "coordinator 모듈 빌드 등록": "src/InstanceCoordinator.cpp",
    "명시적 새 창 CLI": '"new-window"',
    "새 창 다중 세션 요청": "MessageType::StartMultiSession",
    "기본 파일 전달 메시지": "MessageType::OpenFiles",
    "새 IPC protocol": "SorinuriIPC_v2",
    "다중 세션 shared-memory marker": "SorinuriIPC_v2_MultiSession",
    "IPC 요청 ACK": 'constexpr char kAck[] = "OK"',
    "stale endpoint 보호용 lock": "SorinuriIPC_v2_Coordinator",
    "멀티 세션 오디오 정책 enum": "AudioSessionPolicy",
    "공유 PCM 정책": "MultiShared",
    "초기화 전 정책 주입": "setAudioSessionPolicy(AudioSessionPolicy::MultiShared, false)",
    "runtime policy 적용": "applyAudioSessionPolicy",
    "기존 창 shared 전환": "enableMultiInstanceSharedAudio",
    "명시적 새 창 메뉴": "새 플레이어 창 열기 (공유 PCM 다중 재생)",
    "설정 오디오 잠금": "multiInstanceAudioLocked_",
}
all_text = "\n".join((main, coordinator_h, coordinator_cpp, core_h, core_cpp, window_h, window_cpp, settings_cpp, cmake))
for label, needle in required.items():
    if needle not in all_text:
        print(f"다중 인스턴스 검증 실패: {label}을 찾을 수 없습니다.", file=sys.stderr)
        sys.exit(1)

# 새 창은 기존 coordinator socket을 삭제해서는 안 된다. removeServer는 coordinator lock을
# 실제로 확보한 후 InstanceCoordinator 안에서만 한 번 실행될 수 있다.
if "QLocalServer::removeServer" in main:
    print("다중 인스턴스 검증 실패: main.cpp가 기존 IPC server를 직접 삭제합니다.", file=sys.stderr)
    sys.exit(1)
remove_index = coordinator_cpp.find("QLocalServer::removeServer(serverName())")
lock_index = coordinator_cpp.find("if (!coordinatorLock_.create(1))")
if remove_index < 0 or lock_index < 0 or remove_index < lock_index:
    print("다중 인스턴스 검증 실패: stale IPC endpoint 정리가 coordinator lock 뒤에 있지 않습니다.", file=sys.stderr)
    sys.exit(1)

# MultiShared와 OnlineShared는 init·resume에서 반드시 exclusive=no와 audio-spdif 빈 값을 쓴다.
# 온라인 YouTube 정책까지 포괄하되, 단일 HDMI 재생의 저장된 고음질 선호는 유지한다.
policy_start = core_cpp.find("void MpvCore::applyAudioSessionPolicy")
policy_end = core_cpp.find("void MpvCore::setAudioDevice", policy_start)
policy_section = core_cpp[policy_start:policy_end]
for needle in (
    'const bool sharedPcm = audioSessionPolicy_ != AudioSessionPolicy::SinglePreferred',
    'const bool exclusive = !sharedPcm && settings.value("audio/exclusive", true).toBool()',
    'const bool passthrough = !sharedPcm && settings.value("audio/passthrough", true).toBool()',
    'mpv_set_property_string(mpv_, "audio-exclusive", exclusive ? "yes" : "no")',
    'mpv_set_property_string(mpv_, "audio-spdif",',
    'mpv_set_property_string(mpv_, "audio-channels", "auto")',
    'mpv_set_property_string(mpv_, "ad-lavc-downmix", "no")',
    'mpv_set_property_string(mpv_, "audio-normalize-downmix", "no")',
):
    if needle not in policy_section:
        print(f"다중 인스턴스 검증 실패: shared PCM 정책에 {needle}가 없습니다.", file=sys.stderr)
        sys.exit(1)

# 절전·HDMI hot-plug 복구가 전역 QSettings의 exclusive/bitstream으로 되돌아가면 안 된다.
resume_start = core_cpp.find("void MpvCore::restoreAudioOutputAfterDeviceChange")
resume_end = core_cpp.find("void MpvCore::setAudioSessionPolicy", resume_start)
resume_section = core_cpp[resume_start:resume_end]
if "applyAudioSessionPolicy(true)" not in resume_section:
    print("다중 인스턴스 검증 실패: 절전/장치 복구가 세션 오디오 정책을 재사용하지 않습니다.", file=sys.stderr)
    sys.exit(1)

# 설정 창은 다중 모드의 false/disabled 값을 영구 QSettings에 기록하면 안 된다.
apply_start = settings_cpp.find("void SettingsDialog::onApply")
apply_end = settings_cpp.find("void SettingsDialog::onOk", apply_start)
apply_section = settings_cpp[apply_start:apply_end]
if "if (!multiInstanceAudioLocked_)" not in apply_section:
    print("다중 인스턴스 검증 실패: 설정 저장이 다중 오디오 잠금 상태를 구분하지 않습니다.", file=sys.stderr)
    sys.exit(1)

# shared PCM 상태에서 런타임 단축키가 exclusive를 재활성화하면 안 된다.
shortcut_start = window_cpp.find("case Qt::Key_E:")
shortcut_end = window_cpp.find("case Qt::Key_Plus:", shortcut_start)
shortcut_section = window_cpp[shortcut_start:shortcut_end]
if "if (multiInstanceSharedAudio_)" not in shortcut_section:
    print("다중 인스턴스 검증 실패: Ctrl+Shift+E가 다중 세션에서 잠기지 않습니다.", file=sys.stderr)
    sys.exit(1)

print("다중 인스턴스 오디오 정책 검증 통과: 파일 전달 IPC, stale 보호, shared PCM, 멀티채널 협상, 절전 복구, 설정 잠금 확인")
