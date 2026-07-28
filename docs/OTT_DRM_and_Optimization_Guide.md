# 소리누리 플레이어 기술 분석 보고서: OTT DRM 한계 및 시작속도 최적화 가이드

본 문서는 소리누리 플레이어 개발 과정에서 제기된 OTT(넷플릭스 등) 스트리밍 재생의 기술적 한계와, Qt 기반 포터블 애플리케이션의 시작속도 최적화 원리에 대해 상세히 분석한 기술 문서입니다.

---

## 1. OTT 플랫폼의 기술적 제약: DRM과 Widevine

넷플릭스, 디즈니+, 아마존 프라임 비디오 등 주요 OTT 서비스의 콘텐츠를 서드파티 플레이어(소리누리, VLC, 팟플레이어 등)에서 재생하는 것은 현재의 기술 및 법적 구조상 불가능에 가깝습니다. 그 핵심 이유는 **DRM(Digital Rights Management)** 기술의 폐쇄성에 있습니다.

### 1.1 Widevine DRM의 구조와 보안 레벨

대부분의 웹 기반 스트리밍 서비스는 구글이 개발한 **Widevine DRM**을 사용합니다. Widevine은 보안 수준에 따라 L1, L2, L3 세 가지 레벨로 나뉩니다 [1].

| 보안 레벨 | 복호화 방식 | 해상도 제한 | 오디오 제한 | 지원 환경 |
|:---:|:---|:---:|:---:|:---|
| **L1** | 하드웨어 (TEE/TrustZone) | 4K UHD | **Dolby Atmos / 5.1** | 인증된 기기, 공식 앱, Edge 브라우저 |
| **L2** | 소프트웨어 + 커스텀 하드웨어 | 1080p | 스테레오 (2.0) | 일부 구형 기기 |
| **L3** | 전면 소프트웨어 기반 | 480p ~ 720p | 스테레오 (2.0) | Chrome, Firefox, 일반 안드로이드 기기 |

### 1.2 왜 PC 브라우저에서는 스테레오만 나오는가?

Windows PC 환경에서 Chrome이나 Firefox 브라우저로 넷플릭스를 시청할 때 5.1 서라운드나 Dolby Atmos가 출력되지 않고 스테레오만 나오는 이유는 **소프트웨어 기반인 Widevine L3 환경으로 강제 다운그레이드되기 때문**입니다 [2]. 

고음질 오디오(Dolby Digital+, Atmos)와 고화질 비디오(4K)는 콘텐츠 복제 방지를 위해 **반드시 하드웨어 수준의 보안(Widevine L1 또는 Microsoft PlayReady)이 보장된 환경에서만 스트리밍**됩니다. Windows 환경에서 이를 만족하는 것은 마이크로소프트의 PlayReady DRM이 내장된 **Edge 브라우저**와 **Windows Store용 공식 넷플릭스 앱**뿐입니다 [3].

### 1.3 커스텀 플레이어(소리누리)의 한계

소리누리(Qt + libmpv)와 같은 오픈소스 기반 미디어 플레이어는 다음과 같은 이유로 넷플릭스 재생이 불가능합니다.

1. **CDM(Content Decryption Module) 부재**: Widevine 모듈은 구글의 라이선스 계약을 맺은 브라우저 벤더(Chrome, Firefox, Edge 등)에게만 바이너리 형태로 제공됩니다.
2. **TEE(신뢰 실행 환경) 접근 불가**: L1 레벨 복호화는 CPU 내의 격리된 보안 영역에서 이루어지며, 일반 애플리케이션은 이 메모리 영역에 접근할 수 없습니다.
3. **복호화 키 추출 불가**: 영상/음성 스트림을 다운로드하더라도, 이를 풀기 위한 암호화 키를 얻을 수 없습니다.

> **결론**: 넷플릭스의 고음질(Dolby Atmos, 5.1) 오디오를 패스스루로 앰프에 전달하려면 Windows 공식 앱이나 Edge 브라우저를 사용하는 것 외에는 기술적인 대안이 없습니다.

---

## 2. 유튜브 5.1 서라운드 재생 구조 (yt-dlp 연동)

반면, 유튜브(YouTube)는 유료 구매 콘텐츠를 제외한 일반 영상에 DRM을 적용하지 않습니다. 따라서 영상과 음성 스트림 주소만 알아내면 MPV 엔진에서 직접 스트리밍이 가능합니다.

### 2.1 유튜브의 오디오 포맷

유튜브는 최근 영상 제작자가 5.1 서라운드 오디오를 업로드할 경우, 이를 **Dolby Digital Plus (E-AC3)** 포맷으로 인코딩하여 스트리밍합니다 [4]. 

### 2.2 소리누리(v1.1)의 유튜브 5.1 구현 방식

소리누리 v1.1에서는 `yt-dlp`라는 오픈소스 스트림 추출 도구를 내장하여 이 기능을 구현했습니다.

1. **스트림 분석**: 사용자가 유튜브 URL을 입력하면, MPV 내부에서 `yt-dlp`가 백그라운드로 실행되어 해당 영상이 제공하는 모든 비디오/오디오 스트림 목록을 가져옵니다.
2. **포맷 선택**: 소리누리는 `ytdl-format="bestvideo+bestaudio/best"` 옵션을 통해, 사용 가능한 오디오 스트림 중 가장 품질이 높고 채널 수가 많은 스트림(주로 256kbps 이상의 E-AC3 5.1)을 자동으로 선택합니다.
3. **오디오 패스스루**: MPV는 다운로드되는 E-AC3 스트림을 디코딩하지 않고, 설정된 `audio-spdif=ac3,eac3` 옵션에 따라 WASAPI를 통해 오디오 리시버(사운드바, AV 앰프)로 직접 전달(Passthrough)합니다.

이를 통해 브라우저를 거치지 않고 완벽한 5.1 채널 서라운드 감상이 가능해집니다.

---

## 3. Qt 포터블 애플리케이션 시작속도 최적화

초기 소리누리 포터블 버전(ZIP)을 실행할 때 화면이 나타나기까지 수 초의 지연이 발생하는 문제가 있었습니다. 이는 Qt 프레임워크의 플러그인 로딩 메커니즘과 Windows의 파일 I/O 병목이 결합되어 발생한 현상입니다.

### 3.1 느린 시작의 원인

`windeployqt` 도구로 패키징된 Qt 애플리케이션은 실행 파일(Sorinuri.exe) 주변에 수많은 DLL과 플러그인 폴더(`platforms`, `styles`, `tls` 등)를 배치합니다. 

애플리케이션이 시작될 때 `QApplication` 생성자는 다음 작업을 수행합니다 [5]:
1. 시스템의 기본 플러그인 경로 탐색
2. 환경 변수(`QT_PLUGIN_PATH`) 탐색
3. 레지스트리에 등록된 Qt 설치 경로 탐색
4. 실행 파일이 위치한 디렉토리의 모든 하위 폴더 탐색

이 과정에서 수십 개의 DLL 파일을 열고 헤더를 읽어 플러그인 여부를 확인(LoadLibrary)하므로, SSD가 아닌 환경이나 백신 프로그램이 작동 중인 환경에서는 심각한 I/O 지연이 발생합니다.

### 3.2 최적화 방법 (소리누리 적용 완료)

이 문제를 해결하기 위해 소리누리 v1.1에는 두 가지 핵심 최적화가 적용되었습니다.

#### A. `qt.conf`를 통한 탐색 경로 고정

실행 파일과 같은 위치에 `qt.conf` 텍스트 파일을 생성하여, Qt가 불필요한 시스템 디렉토리나 레지스트리를 뒤지지 않도록 탐색 경로를 강제했습니다.

```ini
[Paths]
Prefix = .
Plugins = plugins
Libraries = .
```
이 설정으로 Qt는 오직 명시된 경로에서만 플러그인을 로드하므로 수많은 파일 시스템 접근 호출(Syscall)이 생략됩니다.

#### B. OpenGL 컨텍스트 사전 초기화 지연 방지

`QOpenGLWidget`을 사용하는 애플리케이션은 첫 위젯이 화면에 나타날 때 Windows의 WGL(Windows Graphics Library) 서브시스템과 통신하여 픽셀 포맷을 협상합니다. 이 과정이 메인 이벤트 루프 진입 직전에 발생하면 UI 렌더링이 멈춘 것처럼 보입니다.

이를 방지하기 위해 `main()` 함수 최상단, `QApplication` 인스턴스가 생성되기도 전에 `QSurfaceFormat::setDefaultFormat()`을 호출하여 OpenGL 버퍼 및 VSync 설정을 미리 확정했습니다. 이를 통해 창이 생성될 때 픽셀 포맷 협상 과정이 생략되어 즉각적으로 화면이 나타납니다.

---

## References
[1] "Spatial Audio, Dolby Atmos, and Widevine DRM," GetLibation Docs. Available: https://getlibation.com/docs/advanced/spatial-audio
[2] "Widevine/DRM causes OS instability," Microsoft Q&A. Available: https://learn.microsoft.com/en-us/answers/questions/4321070/widevine-drm-causes-os-instability
[3] "Netflix Atmos on Windows 10," Reddit r/htpc. Available: https://www.reddit.com/r/htpc/comments/hfi3gk/netflix_atmos_on_windows_10_june_2020/
[4] "Does YouTube stream Dolby Atmos content or any surround format?," AVS Forum. Available: https://www.avsforum.com/threads/does-youtube-not-asking-about-youtube-tv-at-this-point-stream-dolby-atmos-content-or-any-suuround-format.3249129/
[5] "How can I improve the startup time of Qt5 programs?," Stack Overflow. Available: https://stackoverflow.com/questions/35564370/how-can-i-improve-the-startup-time-of-qt5-programs
