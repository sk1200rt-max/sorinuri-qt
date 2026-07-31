# 소리누리 (Sorinuri) 차세대 기능 기획 제안서

본 기획서는 현재 소리누리(v4.4.0)의 구현 현황을 바탕으로, 팟플레이어(PotPlayer), MPC-HC(madVR), JRiver Media Center 등 업계 최고 수준의 경쟁 미디어 플레이어들이 제공하는 고급 기능들을 분석하여, 소리누리가 차세대 하이엔드 플레이어로 도약하기 위한 추가 기능 로드맵을 제안합니다.

---

## 1. 현재 소리누리(v4.4.0) 구현 현황 요약

현재 소리누리는 libmpv 엔진을 기반으로 Qt 프레임워크를 활용하여 현대적이고 깔끔한 UI를 갖춘 미디어 플레이어입니다. 최근 v4.4.0 업데이트를 통해 다음과 같은 핵심 기능들이 구현되었습니다.

| 구분 | 구현 완료 기능 | 비고 |
| --- | --- | --- |
| **코어/재생** | 하드웨어 디코딩(DXVA2, D3D11VA, NVDEC), 영상/음악 모드 자동 전환, 이어보기, 최근 파일 목록 | libmpv의 강력한 재생 능력 활용 |
| **오디오/HiFi** | WASAPI Exclusive 모드, 오디오 장치 스마트 감지, 오디오 패스스루(비트스트림), 갭리스 재생, 비트퍼펙트 상태 실시간 표시, 10밴드 파라메트릭 EQ, 내장 앨범 아트 추출 | 오디오파일(Audiophile) 지향 기능 확보 |
| **비디오/화질** | 고급 업스케일링(EWA Lanczos Sharp 등), HDR 톤매핑(BT.2390 등), 디밴딩 필터, 화면 필터(밝기/대비 등), 스크린샷 캡처 | madVR에 준하는 화질 옵션 제공 |
| **편의/부가** | A-B 구간 반복, 오디오/자막 싱크 조절, 재생 속도 조절, 미니 플레이어, 단축키 오버레이 | ProFeaturesWidget을 통한 통합 제공 |
| **혁신 기능** | yt-dlp 연동 (유튜브 5.1 서라운드), Whisper AI 실시간 자막, AI 업스케일링 셰이더, 스마트 챕터 | 타 플레이어와 차별화되는 AI 연동 기능 |

현재 소리누리는 "경량화된 MPC-HC의 성능"과 "JRiver의 오디오파일 기능", 그리고 "AI 기술 연동"이라는 독보적인 포지션을 구축하고 있습니다. 그러나 팟플레이어나 JRiver가 제공하는 **'미디어 라이브러리 관리'**나 **'초고급 오디오/비디오 처리'** 영역에서는 아직 확장할 여지가 있습니다.

---

## 2. 경쟁 플레이어 주요 기능 분석

소리누리의 발전 방향을 설정하기 위해 주요 경쟁 플레이어들의 특장점을 분석했습니다.

### 2.1. 팟플레이어 (PotPlayer)
팟플레이어는 전 세계적으로 가장 많은 기능을 내장한 플레이어로 평가받고 있습니다. [1]
*   **미디어 처리:** 360도 VR 비디오 지원, 3D 영상 재생, 화면 캡처 및 녹화 기능. [2]
*   **자막:** 강력한 자막 렌더링, 자막 온라인 검색, 실시간 자막 번역, 자막 싱크 편집기 내장.
*   **라이브러리/방송:** 내장 재생목록 관리, 방송 시청 및 송출 기능.

### 2.2. MPC-HC / MPC-BE (with madVR)
가볍고 군더더기 없는 성능으로 사랑받으며, 특히 홈씨어터 PC(HTPC) 환경에서 madVR 렌더러와 결합하여 최고의 화질을 제공합니다. [3]
*   **화질 최적화:** madVR을 통한 동적 HDR 톤매핑(Dynamic Tone Mapping), 디스플레이 캘리브레이션 3D LUT 적용. [4]
*   **오디오:** LAV Filters 연동을 통한 완벽한 비트스트림 패스스루.

### 2.3. JRiver Media Center
오디오파일과 하이엔드 홈씨어터 사용자를 위한 상용 미디어 센터입니다. [5]
*   **오디오 처리:** 64비트 오디오 엔진, 완벽한 비트퍼펙트 출력, VST/VST3 플러그인 지원. [6]
*   **룸 코렉션:** 컨볼루션(Convolution) 필터를 이용한 실내 음향 보정(Room Correction). [7]
*   **라이브러리:** 수만 곡 이상의 음원과 영상을 관리하는 강력한 데이터베이스 기반 라이브러리.

---

## 3. 소리누리 추가 기능 기획 제안 (v4.5 ~ v5.0 로드맵)

경쟁사 분석을 바탕으로, 소리누리가 '모던 하이엔드 플레이어'로 확고히 자리 잡기 위해 다음과 같은 기능 추가를 제안합니다.

### 1단계: 시청 경험 및 편의성 극대화 (v4.5)

현재 갖춰진 강력한 재생 엔진 위에, 사용자가 콘텐츠를 소비하는 경험을 더욱 매끄럽게 만드는 기능들입니다.

| 기능명 | 상세 내용 | 기대 효과 |
| --- | --- | --- |
| **자동 자막 다운로드 및 번역** | OpenSubtitles API를 연동하여 영상 파일 이름 해시 기반으로 자막을 자동 검색 및 다운로드. (기존 Whisper AI 자막은 연산 자원을 소모하므로, 일반 영상은 API 다운로드를 우선 적용) | 사용자 편의성 대폭 향상 (팟플레이어 수준의 자막 편의성) |
| **스마트 미디어 라이브러리 (Lite)** | 지정된 폴더(음악/비디오)를 스캔하여 섬네일 그리드 형태로 보여주는 내부 탐색기 기능. (SQLite 기반 가벼운 인덱싱) | 단순 파일 재생기를 넘어선 미디어 센터로의 진화 |
| **모션 스무딩 (프레임 보간)** | 24fps 영화를 60fps로 부드럽게 재생하는 기능. (SVP/RIFE 셰이더 연동 또는 libmpv의 `interpolation` 옵션 활성화) | 화질에 민감한 사용자 만족도 충족 |

### 2단계: 하이엔드 오디오/비디오 전문가 기능 (v4.6)

JRiver나 madVR 사용자를 소리누리로 흡수하기 위한 초고급 기능입니다.

| 기능명 | 상세 내용 | 기대 효과 |
| --- | --- | --- |
| **VST/VST3 플러그인 호스트** | 오디오 체인에 상용 VST 플러그인(예: FabFilter, iZotope 등)을 적용할 수 있는 기능. | 오디오파일 및 음악 제작자들의 필수 요구사항 충족 |
| **컨볼루션(Convolution) 필터 적용** | REW(Room EQ Wizard) 등에서 생성한 임펄스 응답(IR) WAV 파일을 로드하여 룸 코렉션(Room Correction) 수행. | JRiver를 대체할 수 있는 하이엔드 오디오 기능 확보 |
| **3D LUT 디스플레이 캘리브레이션** | 캘리브레이션 장비로 생성한 3D LUT 파일(.cube)을 로드하여 전문가 수준의 정확한 색상 재현. | 영상 전문가 및 HTPC 사용자 유입 |

### 3단계: 차세대 미디어 및 네트워크 통합 (v5.0)

로컬 재생을 넘어 네트워크와 차세대 포맷을 지원하는 단계입니다.

| 기능명 | 상세 내용 | 기대 효과 |
| --- | --- | --- |
| **네트워크 스트리밍 (SMB/DLNA)** | 개인 NAS(SMB)나 로컬 네트워크의 DLNA 서버에 접속하여 미디어를 직접 탐색하고 재생. | 현대 사용자의 미디어 소비 패턴(NAS 중심) 완벽 대응 |
| **360도 VR 비디오 재생** | 마우스 드래그로 시점을 이동할 수 있는 360도 영상 재생 지원. (libmpv 셰이더 또는 OpenGL 매핑 활용) | 최신 미디어 포맷 지원 |
| **크롬캐스트 / AirPlay 송출** | 소리누리에서 재생 중인 영상/음악을 스마트 TV나 오디오 기기로 무선 캐스팅. | 거실 환경(Living Room) 장악력 강화 |

---

## 4. 구현 우선순위 및 결론

위 제안된 기능 중 단기적으로 가장 임팩트가 크고 구현이 용이한 기능은 다음과 같습니다.

1.  **자동 자막 다운로드 (OpenSubtitles API 연동):** 한국 사용자들이 가장 많이 요구하는 편의 기능입니다.
2.  **스마트 미디어 라이브러리 (Lite):** UI 프레임워크(Qt)의 장점을 살려 세련된 그리드 뷰를 제공하면 시각적 만족도가 매우 높습니다.
3.  **모션 스무딩 (프레임 보간):** libmpv의 내장 설정(`interpolation=yes`, `tscale=oversample` 등)만으로도 즉시 훌륭한 결과를 낼 수 있습니다.

소리누리는 이미 v4.4.0을 통해 팟플레이어의 가벼움과 JRiver의 오디오 출력 품질을 동시에 갖춘 훌륭한 토대를 마련했습니다. 제안해 드린 기능 중 우선적으로 원하시는 방향이 있다면, 해당 기능의 세부 구현 계획을 세우고 바로 개발에 착수할 수 있습니다.

---

### References
[1] "PotPlayer vs. VLC vs. MPV." Reddit, r/potplayer. https://www.reddit.com/r/potplayer/comments/1supw92/potplayer_vs_vlc_vs_mpv/
[2] "Potplayer for Windows - Features and Review." YouTube. https://www.youtube.com/watch?v=zM5-YwkOS_E
[3] "PotPlayer Extensive Guide For Best Video Quality." Linus Tech Tips. https://linustechtips.com/topic/924659-potplayer-extensive-guide-for-best-video-quality/
[4] "MadVR - HDR Settings." AVS Forum. https://www.avsforum.com/threads/madvr-hdr-settings.3127778/
[5] "JRiver Media Center - Still Unmatched After All These Years." Audiophile Style. https://audiophilestyle.com/ca/immersive/jriver-media-center-still-unmatched-after-all-these-years-r1341/
[6] "Audiophile." JRiver. https://jriver.com/audiophile.html
[7] "List of Applications That Support Convolution Filters." Audiophile Style. https://audiophilestyle.com/forums/topic/58821-list-of-applications-that-support-convolution-filters/
