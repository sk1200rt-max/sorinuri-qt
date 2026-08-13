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
    "HDMI 7.1 화이트리스트": 'mpv_set_property_string(mpv_, "audio-channels", "7.1,5.1,stereo")',
    "디코더 선행 다운믹스 차단": 'mpv_set_property_string(mpv_, "ad-lavc-downmix", "no")',
    "스테레오 다운믹스 정규화": 'mpv_set_property_string(mpv_, "audio-normalize-downmix", "yes")',
    "비트스트림 코덱 목록": '"ac3,eac3,dts,dts-hd,truehd"',
}
for name, needle in required.items():
    if needle not in text:
        print(f"오디오 정책 검증 실패: {name} 설정을 찾을 수 없습니다.", file=sys.stderr)
        sys.exit(1)

# HDMI 리시버에서 OS가 과도한 레이아웃을 보고할 수 있으므로 auto는 금지한다.
forbidden = 'mpv_set_property_string(mpv_, "audio-channels", "auto")'
if forbidden in text:
    print("오디오 정책 검증 실패: HDMI에서 안전하지 않은 auto 채널 협상이 추가됐습니다.", file=sys.stderr)
    sys.exit(1)

print("오디오 정책 검증 통과: WASAPI, 7.1/5.1/stereo, 원본 다운믹스 보호, 비트스트림 확인")
