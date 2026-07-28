# 소리누리 (Sorinuri) — Qt6 + libmpv

**전문 미디어 플레이어** — DTS-HD, TrueHD, Dolby Atmos 완벽 패스스루 지원

## 기술 스택

- **Qt 6.7** — UI 프레임워크
- **libmpv** — 미디어 재생 엔진 (shinchiro Windows 빌드)
- **MSVC 2022** — Windows 빌드 컴파일러
- **CMake 3.20+** — 빌드 시스템

## 지원 오디오 포맷

| 포맷 | 패스스루 | 설명 |
|------|---------|------|
| DTS-HD Master Audio | ✅ | HDMI 비트스트림 |
| Dolby TrueHD / Atmos | ✅ | HDMI 비트스트림 |
| DTS:X | ✅ | HDMI 비트스트림 |
| DTS Core | ✅ | S/PDIF 또는 HDMI |
| Dolby Digital (AC3) | ✅ | S/PDIF 또는 HDMI |
| Dolby Digital Plus (E-AC3) | ✅ | HDMI |
| PCM Stereo / 5.1 / 7.1 | ✅ | 디코딩 후 출력 |

## 지원 비디오 포맷

MKV, MP4, AVI, MOV, TS, M2TS, WMV, FLV, WebM 등 libmpv가 지원하는 모든 포맷

## 빌드

### 요구사항
- Qt 6.7+ (MSVC 2019 x64)
- CMake 3.20+
- Visual Studio 2022
- libmpv (shinchiro 빌드)

### 빌드 방법
```bash
# libmpv 다운로드 후 deps/libmpv/에 배치
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

## 다운로드

[GitHub Releases](https://github.com/sorinuri/sorinuri-qt/releases) 또는
[sorinuri.com](https://sorinuri.com)
