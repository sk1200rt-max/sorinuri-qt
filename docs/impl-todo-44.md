# v4.4.0 전체 기능 적용 작업 목록

## 1순위: 안정성
- [ ] A1. 렌더링 INVALID_ENUM 정리: MpvWidget FBO 포맷 힌트 전달 (MPV_RENDER_PARAM_OPENGL_FBO에 internal_format 지정하지 않아 rgba16f 협상 에러 발생 → fbo.internal_format = GL_RGBA8 지정)
- [ ] A2. 오디오 장치 사전 감지: loadFile 전 audio-device-list 파싱, 현재 기본 장치가 HDMI/SPDIF가 아니면 spdif 미리 해제 → 실패-복구 사이클 제거
- [ ] A3. 오디오 장치 선택 UI 실제 구현: SettingsDialog에서 audio-device-list를 실제 파싱해 콤보 구성 (현재 하드코딩 더미), 선택 시 mpv audio-device 적용+저장
- [ ] A4. video-sync 자동화: 오디오 없는 파일이면 video-sync=display-resample로 전환(선택적) — 위험하므로 audio 트랙 없을 때만

## 2순위: HiFi 음악 모드
- [ ] B1. 내장 앨범 아트 추출: mpv 트랙리스트에서 albumart 트랙 확인 + video 트랙(mjpeg attached picture) 렌더, 또는 taglib 없이 mpv screenshot-raw 사용은 복잡 → 파일에서 직접 ID3/FLAC picture 파싱(간단 파서)
- [ ] B2. 가사 동기화: LyricsWidget 이미 존재 (LRC + 인터넷 검색) → MainWindow에 연결 확인
- [ ] B3. EQ 프리셋: HiFiEngine + EqPanel 이미 존재 → MusicWidget에 연결 확인
- [ ] B4. 갭리스: gapless-audio=yes 음악 모드 진입 시 항상 적용
- [ ] B5. 비트퍼펙트 표시: audio-out-params(samplerate/format/channels) + exclusive 여부 MusicWidget에 실시간 표시

## 3순위: 고급 기능
- [ ] C1. 이어보기: 종료/파일전환 시 위치 저장(QSettings), 파일 열 때 5초 이상 & 95% 미만이면 이어보기 (자동, 팝업 없이 OSD 알림)
- [ ] C2. 최근 파일 목록: 파일 메뉴/컨트롤바에 최근 10개
- [ ] C3. 스마트 챕터/북마크: ChapterWidget 이미 존재 → 연결 상태 확인
- [ ] C4. 유튜브 5.1 안정화: ytdl-format 이미 설정됨 → 유지
- [ ] C5. OTT/AI 자막/AI 업스케일: OttWidget/WhisperWidget/UpscaleWidget 이미 존재 → 연결 확인만

## 결정사항
- taglib 의존성 추가 대신 간단 자체 파서로 앨범아트 추출 (ID3v2 APIC + FLAC METADATA_BLOCK_PICTURE)
- 이어보기는 사용자 지침("사용자에게 작업 요청 최소화")에 따라 자동+OSD 안내 방식
