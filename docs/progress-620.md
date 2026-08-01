# v6.2.0 작업 진행 상태

## 완료된 수정 (아직 커밋 안 됨)
1. **단축키 포커스 버그 수정** (MainWindow.cpp)
   - setupUI에 `setFocusPolicy(Qt::StrongFocus)` 추가
   - MpvWidget에 `setFocusPolicy(Qt::ClickFocus)` 추가
   - eventFilter에 MouseButtonPress 시 `setFocus()` 추가
   
2. **단축키 전면 수정** (MainWindow.cpp)
   - `S` → 정지(stop), `Ctrl+S` → 화면 캡처 (기존: S가 스크린샷이었음)
   - `V` → 자막 트랙 전환 추가 (누락됨)
   - `R` → 반복 모드 토글 추가 (누락됨)
   - `X` → 셔플 토글 추가 (누락됨)
   - 볼륨 변경 시 OSD 표시 추가
   - `>/<` 속도 변경 시 OSD 표시 추가
   - `T` 항상 위에 OSD 표시 추가

## 남은 작업
3. ShortcutOverlay 도움말 업데이트 (실제 동작과 일치하도록)
4. 차별화 기능 기획서 작성
5. bump_version 6.2.0 → 커밋 → 빌드 → 배포

## 단축키 최종 정리 (수정 후)
| 키 | 동작 |
|---|---|
| Space/Enter | 재생/일시정지 |
| ← / → | 5초 뒤로/앞으로 |
| Shift+← / → | 60초 뒤로/앞으로 |
| Ctrl+← / → | 10초 뒤로/앞으로 |
| ↑ / ↓ | 볼륨 +5% / -5% |
| PageUp/Down | 5분 뒤로/앞으로 |
| Home/End | 처음/끝으로 |
| M | 음소거 토글 |
| N | 다음 파일 |
| [ / ] | 이전/다음 챕터 |
| Ctrl+O | 파일 열기 |
| F / F11 | 전체화면 토글 |
| Esc | 전체화면 해제 |
| T | 항상 위에 토글 |
| V | 자막 트랙 전환 |
| W / E | 자막 크기 +/- |
| J / K | 자막 앞/뒤 이동 |
| Z / Shift+Z | 자막 딜레이 ±100ms |
| D / Shift+D | 오디오 딜레이 ±100ms |
| A | A-B 반복 A점 설정 |
| B | A-B 반복 B점 설정 |
| Ctrl+A | A-B 반복 해제 |
| R | 반복 모드 토글 |
| X | 셔플 토글 |
| S | 정지 |
| Ctrl+S | 화면 캡처 |
| C | 화면 캡처 |
| + / = | 재생속도 +0.1x |
| - | 재생속도 -0.1x |
| 0 | 재생속도 1.0x |
| > / < | 재생속도 ±0.25x |
| I | 재생 정보 표시 |
| P | 전문 기능 패널 토글 |
| ? | 단축키 도움말 |
