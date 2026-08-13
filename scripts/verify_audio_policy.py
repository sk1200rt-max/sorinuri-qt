#!/usr/bin/env python3
"""멀티채널 출력 정책의 의도치 않은 회귀를 CI에서 차단한다."""
from __future__ import annotations

from pathlib import Path
import sys

source = Path(__file__).resolve().parents[1] / "src" / "MpvCore.cpp"
text = source.read_text(encoding="utf-8")

required = {
    'WASAPI 출력 사용': 'mpv_set_property_string(mpv_, "ao", "wasapi")',
    '독점 모드 기본값': 's.value("audio/exclusive", true)',
    '자동 다채널 협상': 'mpv_set_property_string(mpv_, "audio-channels", "auto")',
    '비트스트림 코덱 목록': '"ac3,eac3,dts,dts-hd,truehd"',
}
for name, needle in required.items():
    if needle not in text:
        print(f"오디오 정책 검증 실패: {name} 설정을 찾을 수 없습니다.", file=sys.stderr)
        sys.exit(1)

forbidden = 'mpv_set_property_string(mpv_, "audio-channels", "7.1,5.1,stereo")'
if forbidden in text:
    print("오디오 정책 검증 실패: 강제 레이아웃 정책이 다시 추가됐습니다.", file=sys.stderr)
    sys.exit(1)

print("오디오 정책 검증 통과: WASAPI, 독점 기본값, auto 다채널, 비트스트림 설정 확인")
