# v4.3.8 진단 로그 분석 (파일 열기 자동 재생 안 됨)

## 앱 로그 (sorinuri_app.log)
```
16:47:29.992 PROP pause=no
16:47:39.207 loadFile: ... mode=replace
16:47:39.209 EVENT START_FILE
16:47:39.476 EVENT FILE_LOADED
16:47:39.477 PROP time-pos=0
(이후 아무 이벤트 없음 - PLAYBACK_RESTART 없음!)
```

## MPV 로그 핵심 타임라인
- 9.801: loadfile 명령 실행 (flags=replace)
- 9.849: "Starting playback..."
- 9.854: audio: spdif_eac3 패스스루 디코더 선택 (E-AC3, 192kHz stereo spdif)
- 9.854-9.864: WASAPI exclusive로 spdif-eac3/spdif-ac3 시도 → **모두 unsupported**
  (사용자 기기: Hidizs S9 USB DAC - 패스스루 미지원 2ch DAC)
- 9.864: "Failed to initialize audio driver 'wasapi'"
- 9.864: "Falling back to PCM output." → eac3 일반 디코더 다시 오픈
- 9.865: VO reconfig 완료 (1920x1080 cuda)
- 10.069: "mpv_render_context_render() not being called or stuck." ← 렌더 콜백 문제도 존재
- 10.070: "first video frame after restart shown"
- 10.070: pause=no 설정됨
- **이후 10.070 ~ 20.4초까지 아무 진행 없음. time-pos 진행 없음. 오디오 AO 재시도 없음!**
- 로그에 "Trying audio driver"가 딱 한 번만 나옴 → PCM 폴백 후 AO 초기화가 다시 수행되지 않음

## 근본 원인
1. **spdif(패스스루) 실패 → PCM 폴백 과정에서 mpv가 새 AO 초기화를 하지 못하고 멈춤.**
   - "Falling back to PCM output" 후 eac3 디코더는 다시 열었지만 두 번째 `Trying audio driver` 로그가 없음
   - mpv는 오디오 드라이버 없이 대기 상태 → video-sync=audio 이므로 오디오 클럭 없이는 영상도 진행 안 됨
2. 방향키(seek)를 누르면 재생 재시작 경로에서 AO 초기화가 다시 트리거되어 그때부터 정상 재생됨.
3. 탐색기 연결로 열 때 정상인 이유: 타이밍/초기 상태 차이로 AO 협상이 성공했거나 폴백이 정상 완료된 것.

## 부차 문제
- `mpv_render_context_render() not being called or stuck` → 렌더 업데이트 콜백 지연도 존재
- `after creating texture: OpenGL error INVALID_ENUM` 반복 → FBO 포맷 문제 (Qt 기본 FBO와 rgba16f 협상)

## 수정 방향
1. **audio-spdif 사용 중 AO 실패 시의 mpv 내부 폴백 버그 회피:**
   - `audio-exclusive=yes` + spdif 조합에서 장치가 패스스루 미지원이면 실패
   - 해결책 A: spdif 실패 감지 후 `audio-spdif=""` 로 클리어하고 재로드
   - 해결책 B: AO 협상을 미리 확인 - 초기에는 spdif 끄고, 패스스루 지원 장치일 때만 활성화
   - 해결책 C: mpv 이벤트 루프에서 AO 실패 로그 감지 시 `ao-reload` 명령 실행
2. video-sync=audio → 오디오가 없으면 클럭이 없음. audio 실패 시에도 영상 진행되도록 처리 필요.
