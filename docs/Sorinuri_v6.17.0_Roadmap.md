# 소리누리 v6.17.0 기획 로드맵: AI 및 클라우드 연동 강화
**작성자:** 가온 Communication / **버전:** 1.0 / **날짜:** 2026년 8월

---

## 1. 개요
가온 Communication의 하이엔드 미디어 플레이어 **소리누리(Sorinuri)**는 v6.16.0까지 오디오/비디오 재생 엔진의 완벽성과 Windows 시스템(SMTC, 단축키) 연동을 고도화했습니다. 다음 메이저 업데이트인 **v6.17.0**에서는 최신 기술 동향인 **온디바이스 AI(Local AI)와 클라우드 미디어 스트리밍**을 결합하여, 단순한 재생 도구를 넘어 '지능형 미디어 허브'로 진화하는 것을 목표로 합니다.

본 문서는 최신 2026년 미디어 플레이어 트렌드 조사와 소리누리 Qt6 코드베이스 분석을 바탕으로 기획된 v6.17.0 신규 기능 아이디어와 우선순위를 제시합니다.

---

## 2. 신규 AI 기능 기획 (Local AI 중심)

클라우드 API에 의존하지 않고 사용자의 로컬 환경(CPU/GPU)에서 직접 구동되는 온디바이스 AI 기능을 도입하여 개인정보 보호와 오프라인 사용성을 보장합니다.

### 2.1. AI 자동 장면 전환 감지 (Scene Detection)
* **개요:** 긴 동영상 파일(영화, 강의, 공연 등)을 분석하여 장면이 바뀌는 지점을 자동으로 찾아내어 챕터(Chapter) 마커를 생성합니다.
* **기술 동향:** PySceneDetect [1] 또는 Davinci Resolve의 Scene Cut Detection과 유사한 기능입니다.
* **구현 방안:**
  * 기존 `ChapterWidget`(`src/ChapterWidget.h/.cpp`)의 UI를 재활용합니다.
  * MPV의 비디오 프레임을 캡처하거나, 백그라운드에서 번들된 `ffmpeg.exe`를 사용하여 장면 전환(Scene Change) 필터(`-vf select='gt(scene,0.4)'`)를 실행합니다.
  * 추출된 타임코드를 기반으로 `Chapter` 구조체 배열을 생성하여 UI에 표시합니다.
* **기대 효과:** 사용자가 수동으로 탐색할 필요 없이 의미 있는 장면 단위로 영상을 건너뛸 수 있습니다.

### 2.2. AI 오디오 감정 및 장르 자동 태깅 (Auto-Tagging)
* **개요:** 로컬 음악 파일의 오디오 파형을 분석하여 BPM, 분위기(Mood), 악기 구성, 장르를 자동으로 태깅하고 데이터베이스에 저장합니다.
* **기술 동향:** 2026년 음악 카탈로그 관리는 Essentia.js [2]와 같은 오디오 분석 라이브러리나 경량화된 트랜스포머 모델을 사용한 자동 태깅이 표준으로 자리 잡고 있습니다 [3].
* **구현 방안:**
  * 기존 `MediaLibraryWidget`(`src/MediaLibraryWidget.h/.cpp`)의 스캔 프로세스에 분석 단계를 추가합니다.
  * 오픈소스 C++ 오디오 분석 라이브러리인 **Essentia** [4]를 정적 링크하거나, 경량화된 ONNX 모델을 로드하여 백그라운드 스레드에서 음악을 분석합니다.
  * 분석 결과를 SQLite DB(`DbManager` 구현 필요)에 저장하고, UI에서 '분위기별 필터링' 기능을 제공합니다.
* **기대 효과:** 메타데이터(ID3 태그)가 없는 파일도 소리가 가진 특성만으로 자동 분류되어 스마트한 재생목록 관리가 가능해집니다.

### 2.3. Whisper 기반 로컬 음성 명령 제어 (Voice Control)
* **개요:** "다음 곡 틀어줘", "10초 뒤로 가", "볼륨 50으로 맞춰" 등의 음성 명령을 로컬에서 인식하여 플레이어를 제어합니다.
* **기술 동향:** OpenAI Whisper [5] 모델을 로컬(Llama.cpp 또는 Whisper.cpp)에서 구동하여 인터넷 연결 없이 빠르고 안전하게 음성을 인식하는 기술이 대중화되었습니다 [6].
* **구현 방안:**
  * 기존 자막 생성용으로 사용 중인 `WhisperWidget`(`src/WhisperWidget.h/.cpp`)의 인프라(Whisper.cpp)를 재활용합니다.
  * 마이크 입력을 캡처하여 소형 Whisper 모델(tiny/base)로 텍스트로 변환한 후, 정규식 또는 경량 NLP 모델을 통해 `MainWindow`의 재생 제어 슬롯(`playNext`, `seek`, `setVolume`)을 호출합니다.
* **기대 효과:** 키보드나 마우스를 사용할 수 없는 상황(원거리 시청 등)에서 핸즈프리 제어를 제공합니다.

---

## 3. 클라우드 연동 강화 기획

로컬 파일 재생의 한계를 넘어, 사용자가 보유한 다양한 클라우드 스토리지를 소리누리 안에서 하나의 라이브러리처럼 관리하고 스트리밍합니다.

### 3.1. WebDAV 기반 Nextcloud / 개인 NAS 스트리밍
* **개요:** 사용자의 Nextcloud, ownCloud 또는 개인 NAS(Synology 등)를 WebDAV 프로토콜로 연결하여 미디어를 직접 스트리밍합니다.
* **기술 동향:** 로컬 스토리지의 한계로 인해 개인 클라우드에서 직접 미디어를 스트리밍하는 수요가 꾸준히 증가하고 있습니다 [7].
* **구현 방안:**
  * 기존 `NetworkBrowserWidget`(`src/NetworkBrowserWidget.h/.cpp`)에 'WebDAV / Nextcloud' 탭을 추가합니다.
  * Qt의 `QNetworkAccessManager`를 사용하여 WebDAV `PROPFIND` 요청을 보내 디렉토리 구조를 파싱합니다.
  * 선택된 파일의 URL을 MPV 코어에 직접 전달하여 재생합니다 (MPV는 내부적으로 HTTP/WebDAV 스트리밍을 완벽히 지원함).
* **기대 효과:** 대용량 미디어 파일을 로컬에 다운로드할 필요 없이 개인 서버에서 즉시 재생할 수 있습니다.

### 3.2. 상용 클라우드 드라이브 연동 (OneDrive, Google Drive)
* **개요:** Microsoft OneDrive, Google Drive, Dropbox 등에 저장된 미디어 파일을 소리누리에서 탐색하고 재생합니다.
* **기술 동향:** CloudPlayer와 같은 앱들이 여러 클라우드를 통합하여 하나의 음악 라이브러리로 제공하는 기능을 선도하고 있습니다 [8].
* **구현 방안:**
  * OAuth 2.0 인증 흐름을 구현하여 사용자 계정에 로그인합니다 (로컬 콜백 서버 또는 Qt WebEngine 사용).
  * 각 클라우드의 REST API (Microsoft Graph API, Google Drive API)를 호출하여 파일 목록을 가져옵니다.
  * 파일의 임시 다운로드 링크(Download URL)를 추출하여 MPV로 스트리밍합니다.
* **기대 효과:** 보편적으로 사용하는 상용 클라우드 서비스와의 연동으로 사용자 접근성이 크게 향상됩니다.

### 3.3. Last.fm 양방향 스크로블링 및 클라우드 재생목록 동기화
* **개요:** 재생 기록을 Last.fm에 기록(Scrobbling)할 뿐만 아니라, Last.fm의 추천/좋아요 데이터를 가져와 클라우드 재생목록을 구성합니다.
* **기술 동향:** 음악 감상 이력을 추적하는 Scrobbling은 여전히 활발히 사용되며 [9], 이를 기반으로 한 맞춤형 플레이리스트 생성이 중요해졌습니다.
* **구현 방안:**
  * `ScrobbleManager` 클래스를 신설하여 Last.fm API 2.0 [10] 규격에 맞춰 `track.updateNowPlaying` 및 `track.scrobble`을 호출합니다.
  * `PlaylistWidget`(`src/PlaylistWidget.h/.cpp`)을 확장하여, Last.fm 계정 연동 시 사용자의 'Loved Tracks'를 클라우드 재생목록으로 불러오는 기능을 추가합니다.
* **기대 효과:** 로컬 플레이어임에도 불구하고 소셜 음악 서비스와 같은 사용자 경험을 제공합니다.

---

## 4. 로드맵 및 구현 우선순위

코드베이스의 재사용성과 사용자 체감 효과를 고려한 개발 우선순위입니다.

| 우선순위 | 기능명 | 연관 모듈/클래스 | 구현 난이도 | 체감 효과 |
|:---:|:---|:---|:---:|:---:|
| **1 (★★★)** | **AI 자동 장면 전환 감지** | `ChapterWidget`, `ffmpeg` | 중간 | 높음 |
| **2 (★★★)** | **WebDAV / Nextcloud 연동** | `NetworkBrowserWidget`, `QNetworkAccessManager` | 낮음 | 매우 높음 |
| **3 (★★)** | **Last.fm 스크로블링** | 신규 `ScrobbleManager`, `QNetworkAccessManager` | 낮음 | 중간 |
| **4 (★★)** | **AI 오디오 감정/장르 자동 태깅** | `MediaLibraryWidget`, `Essentia` C++ 라이브러리 | 높음 | 높음 |
| **5 (★)** | **상용 클라우드 (OneDrive/Google Drive)** | 신규 `CloudDriveManager`, OAuth 2.0 | 높음 | 높음 |
| **6 (★)** | **Whisper 기반 로컬 음성 제어** | `WhisperWidget`, `MainWindow` | 중간 | 중간 |

### 개발 전략 제언
1. **v6.17.0 마일스톤:** 우선순위 1~3번(장면 감지, WebDAV, Last.fm)을 핵심 기능으로 선정하여 단기 개발에 착수합니다. 기존 코드(`ChapterWidget`, `NetworkBrowserWidget`)를 확장하는 방식이므로 안정적인 통합이 가능합니다.
2. **v6.18.0 이후 마일스톤:** 오디오 분석(Essentia 통합) 및 OAuth 인증이 필요한 상용 클라우드 연동은 외부 라이브러리 종속성이 커지므로, 충분한 기술 검토와 테스트를 거쳐 다음 버전에 도입하는 것을 권장합니다.

---

## 5. References

[1] PySceneDetect. "PySceneDetect: Video Cut Detection and Analysis Tool." https://www.scenedetect.com/  
[2] "Audio and Music Analysis on the Web using Essentia.js." Transactions of the International Society for Music Information Retrieval. https://transactions.ismir.net/articles/10.5334/tismir.111  
[3] Cyanite.ai. "AI Auto-Tagging for Music Catalogs [Guide 2026]." https://cyanite.ai/blog/ai-auto-tagging-music-catalogs/  
[4] MTG/essentia. "Essentia: C++ library for audio and music analysis." GitHub. https://github.com/MTG/essentia  
[5] OpenAI. "Introducing Whisper." https://openai.com/index/whisper/  
[6] "Run Whisper Locally 2026: Free Offline Speech-to-Text." Local AI Master. https://localaimaster.com/blog/whisper-local-speech-to-text  
[7] Nextcloud Community. "Accessing Nextcloud files using WebDAV." https://docs.nextcloud.com/server/stable/user_manual/en/files/access_webdav.html  
[8] doubleTwist. "CloudPlayer™ cloud & offline." Google Play. https://play.google.com/store/apps/details?id=com.doubleTwist.cloudPlayer  
[9] Fabian Voith. "Reliable mobile Last.fm scrobbling, including Cloud streaming." https://fabian-voith.de/2023/11/26/reliable-mobile-last-fm-scrobbling-including-cloud-streaming/  
[10] Last.fm. "Scrobbling 2.0 Documentation - API Docs." https://www.last.fm/api/scrobbling  
