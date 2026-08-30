# PCM 2.0 스테레오 저음 과출력 조사

## 공식 근거

- mpv stable manual: https://mpv.io/manual/stable/
- `--audio-normalize-downmix`는 서라운드 소리가 스테레오로 다운믹스될 때만 정규화를 켜거나 끄며, **기본값은 `no`**이다. 비활성화하면 다운믹스에서 클리핑이 발생할 수 있다고 명시한다.
- 현재 소리누리 코드에서는 v6.18.9 커밋(`a2bd001`)에서 이 옵션을 `yes`로 명시해 mpv 기본 동작을 바꿨고, 초기화·오디오 복구 양쪽에서 반복 적용하고 있었다.

## 수정 원칙

1. `audio-channels=auto`, `ad-lavc-downmix=no`, DD+/DTS 패스스루 코덱 정책은 변경하지 않는다.
2. 스테레오 경로의 과도한 보정 가능성을 없애기 위해 `audio-normalize-downmix`를 mpv 기본값인 `no`로 일관되게 복원한다.
3. 실제 HDMI 리시버에서 PCM 2.0 저음, DD+ 비트스트림, 절전 복귀를 함께 사전 테스트한다.
