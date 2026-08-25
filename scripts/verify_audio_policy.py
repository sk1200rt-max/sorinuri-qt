#!/usr/bin/env python3
"""멀티채널·스테레오 출력 정책의 의도치 않은 회귀를 CI에서 차단한다."""
from __future__ import annotations

from pathlib import Path
import sys

source = Path(__file__).resolve().parents[1] / "src" / "MpvCore.cpp"
text = source.read_text(encoding="utf-8")

required = {
    "WASAPI 출력 사용": 'mpv_set_property_string(mpv_, "ao", "wasapi")',
    "독점 모드 기본값": 's.value("audio/exclusive", true)',
    "원본 우선 자동 채널 협상": 'mpv_set_property_string(mpv_, "audio-channels", "auto")',
    "디코더 선행 다운믹스 차단": 'mpv_set_property_string(mpv_, "ad-lavc-downmix", "no")',
    "스테레오 다운믹스 정규화": 'mpv_set_property_string(mpv_, "audio-normalize-downmix", "yes")',
    "비트스트림 코덱 목록": '"ac3,eac3,dts,dts-hd,truehd"',
}
for name, needle in required.items():
    if needle not in text:
        print(f"오디오 정책 검증 실패: {name} 설정을 찾을 수 없습니다.", file=sys.stderr)
        sys.exit(1)

# 고정 레이아웃 목록은 Windows 장치 재초기화 뒤 2.0으로 협상될 수 있으므로 금지한다.
forbidden = 'mpv_set_property_string(mpv_, "audio-channels", "7.1,5.1,stereo")'
if forbidden in text:
    print("오디오 정책 검증 실패: 고정 채널 화이트리스트가 남아 있습니다.", file=sys.stderr)
    sys.exit(1)

print("오디오 정책 검증 통과: WASAPI, 원본 우선 자동 다채널 협상, 원본 다운믹스 보호, 비트스트림 확인")
