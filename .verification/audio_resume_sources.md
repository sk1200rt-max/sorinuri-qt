# 멀티채널·절전 복귀·자막 수명주기 조사 기록

## 확인일

2026-08-25

## 외부 근거

| 주제 | 확인 내용 | 출처 |
|---|---|---|
| mpv 출력 채널 | `--audio-channels=auto`는 장치가 받아들이는 레이아웃 중 오디오 원본 레이아웃을 우선한다. 레이아웃 목록 모드는 필터 출력만 조정하며 오디오 API가 다른 레이아웃을 선택할 수 있다. | [mpv manual](https://mpv.io/manual/master/) |
| mpv 오디오 재로드 | `ao-reload`는 mpv 매뉴얼에서 experimental/internal 명령으로 표기된다. 따라서 기존 장치·독점·채널·패스스루 설정을 재적용한 뒤 최소 횟수로 호출해야 한다. | [mpv manual](https://mpv.io/manual/master/) |
| mpv 파일별 설정 | mpv는 파일 로컬 런타임 옵션을 다음 파일 전환 때 재설정할 수 있다. 새 `FILE_LOADED` 시점에서 저장된 자막 스타일을 재적용하는 근거다. | [mpv manual](https://mpv.io/manual/master/) |
| Windows 절전 복귀 | `PBT_APMRESUMEAUTOMATIC`은 시스템이 절전/최대절전에서 매번 복귀할 때 `WM_POWERBROADCAST`로 전달된다. 사용자 상호작용 시 `PBT_APMRESUMESUSPEND`도 전달될 수 있다. | [Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/power/pbt-apmresumeautomatic) |
| Windows 오디오 엔드포인트 | 오디오 엔드포인트 장치 상태·연결·역할 변경은 애플리케이션에 이벤트로 통지되며, 애플리케이션은 장치 사용 방식을 동적으로 변경해 재생을 복구할 수 있다. | [Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/coreaudio/device-events) |
| WASAPI 복구 필요성 | WASAPI는 사용 중인 엔드포인트가 무효화되면 `AUDCLNT_E_DEVICE_INVALIDATED`를 반환할 수 있으며, 애플리케이션이 복구할 수 있다고 문서화한다. | [Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/coreaudio/wasapi) |

## 코드 조사 결론

1. v6.19.3은 `audio-channels="7.1,5.1,stereo"` 고정 목록을 사용하고 있었으며, 6채널 원본이 HDMI/WASAPI 재협상 뒤 PCM 2.0으로 내려갈 수 있는 경로가 존재했다.
2. 절전 복귀 처리(`WM_POWERBROADCAST`)는 렌더링만 갱신하고 오디오 출력 정책을 복원하지 않았다.
3. 자막 스타일은 SettingsDialog에서 저장되고 Apply 시에만 mpv에 전달됐으며, 새 파일 `FILE_LOADED` 시점에 저장값을 복원하는 경로가 없었다.

## 수정 원칙

- WASAPI 독점, 원본 다운믹스 차단, 사용자 선택 패스스루 코덱 정책은 유지한다.
- 절전/장치 변경 뒤에는 저장된 장치·독점·채널·패스스루 설정을 먼저 복원한 후 한 번만 AO를 재초기화한다.
- 새 파일 로드 시 저장된 폰트·크기·굵기·색상·그림자·자동 로드 설정을 즉시 및 트랙 준비 후 한 번 더 적용한다.
