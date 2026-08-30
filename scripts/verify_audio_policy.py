#!/usr/bin/env python3
"""WASAPI·원본 다채널·HDMI 비트스트림 정책의 회귀를 차단한다."""
from __future__ import annotations

from pathlib import Path
import sys

source = Path(__file__).resolve().parents[1] / "src" / "MpvCore.cpp"
text = source.read_text(encoding="utf-8")

required = {
    "WASAPI 출력 사용": 'mpv_set_option_string(mpv_, "ao", "wasapi")',
    "저장 출력 장치 적용": 's.value("audio/device", "auto")',
    "빈 장치값 auto 정규화": 'if (savedDevice.isEmpty()) savedDevice = QStringLiteral("auto")',
    "독점 모드 기본값": 's.value("audio/exclusive", true)',
    "패스스루 기본값": 's.value("audio/passthrough", true)',
    "DD+ E-AC3 코덱": 'codecs << QStringLiteral("eac3")',
    "원본 우선 자동 채널 협상": 'mpv_set_property_string(mpv_, "audio-channels", "auto")',
    "디코더 선행 다운믹스 차단": 'mpv_set_property_string(mpv_, "ad-lavc-downmix", "no")',
    "스테레오 다운믹스 기본 정규화 정책": 'mpv_set_property_string(mpv_, "audio-normalize-downmix", "no")',
    "실제 출력 endpoint 감지": '"audio-out-detected-device"',
    "HDMI/AVR 패스스루 보존": 'const bool passthroughCapable = deviceLikelySupportsPassthrough()',
}
for name, needle in required.items():
    if needle not in text:
        print(f"오디오 정책 검증 실패: {name} 설정을 찾을 수 없습니다.", file=sys.stderr)
        sys.exit(1)

# WASAPI 장치·독점·DD+ 패스스루는 첫 AO 협상 이전에 한 묶음으로 설정해야 한다.
init_index = text.index("int ret = mpv_initialize(mpv_)")
for needle in (
    'mpv_set_option_string(mpv_, "ao", "wasapi")',
    'mpv_set_option_string(mpv_, "audio-device"',
    'mpv_set_option_string(mpv_, "audio-exclusive"',
    'mpv_set_option_string(mpv_, "audio-spdif"',
):
    if text.index(needle) > init_index:
        print(f"오디오 정책 검증 실패: {needle}가 mpv_initialize 뒤에 적용됩니다.", file=sys.stderr)
        sys.exit(1)

# 고정 레이아웃 목록은 Windows 장치 재초기화 뒤 2.0으로 협상될 수 있으므로 금지한다.
if 'mpv_set_property_string(mpv_, "audio-channels", "7.1,5.1,stereo")' in text:
    print("오디오 정책 검증 실패: 고정 채널 화이트리스트가 남아 있습니다.", file=sys.stderr)
    sys.exit(1)

# 스테레오 폴백에서는 mpv 기본 정규화 정책을 유지한다. 강제 yes는
# 리시버의 저음 관리와 겹쳐 저역 체감을 바꿀 수 있으므로 허용하지 않는다.
if 'mpv_set_property_string(mpv_, "audio-normalize-downmix", "yes")' in text:
    print("오디오 정책 검증 실패: 스테레오 다운믹스 정규화를 강제 활성화하면 안 됩니다.", file=sys.stderr)
    sys.exit(1)

# HDMI/AVR로 추정되는 출력에서 워치독·AO 실패 복구가 audio-spdif를 무조건 지우면 안 된다.
for marker in ('워치독: 재생 멈춤', 'AO 초기화 실패 감지'):
    start = text.find(marker)
    if start < 0:
        print(f"오디오 정책 검증 실패: {marker} 복구 경로가 없습니다.", file=sys.stderr)
        sys.exit(1)
    section = text[start:start + 1700]
    if 'if (!passthroughCapable)' not in section:
        print(f"오디오 정책 검증 실패: {marker} 경로가 HDMI 패스스루를 보호하지 않습니다.", file=sys.stderr)
        sys.exit(1)

print("오디오 정책 검증 통과: 초기 WASAPI HDMI 협상, DD+ 패스스루, 원본 다채널, 스테레오 저음 보정, HDMI 복구 보호 확인")
