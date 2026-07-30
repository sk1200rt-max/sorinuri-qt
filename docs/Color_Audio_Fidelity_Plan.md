# 소리누리: 제작사 의도 그대로의 색감과 음향 구현 기획안

## 1. 현재 현황 분석

현재 `MpvCore.cpp`를 점검한 결과, 소리누리는 기본적인 재생은 가능하지만 **제작사가 의도한 화질과 음질을 100% 끌어내기 위한 세부 설정이 누락**되어 있습니다.

### 1.1 화질(Video) 현황
- `vo=libmpv` (렌더링 API)와 `hwdec=auto` (하드웨어 디코딩)만 설정되어 있습니다.
- **문제점**: 
  - HDR 영상을 일반 SDR 모니터에서 볼 때 물 빠진 색감(Washed out)이 나타납니다 (HDR 톤매핑 부재).
  - 1080p 영상을 4K 모니터에서 볼 때 기본 스케일러(Bilinear)가 적용되어 화질이 뭉개집니다.
  - 애니메이션이나 어두운 장면에서 등고선 현상(Color Banding)이 발생합니다.
  - 제작사가 의도한 DCI-P3, BT.2020 색공간이 모니터의 색공간에 맞게 변환되지 않습니다 (Color Management 부재).

### 1.2 음질(Audio) 현황
- `ao=wasapi`, `audio-spdif=ac3,eac3,dts,dts-hd,truehd` 설정으로 패스스루의 기반은 마련되어 있습니다.
- **문제점**:
  - `audio-exclusive=yes`가 명시적으로 설정되지 않아, WASAPI Shared 모드로 동작할 위험이 있습니다 (Windows 믹서를 거치면서 리샘플링 발생 가능성).
  - PCM 음원(FLAC, WAV) 재생 시 샘플레이트가 고정되지 않아 OS 믹서에서 리샘플링이 일어날 수 있습니다 (Bit-Perfect 실패).

---

## 2. 개선 기획안: 제작사 의도 그대로의 재현

전문가 수준의 미디어 플레이어 로드맵(v3.5~v4.5)을 기반으로, 화질과 음질을 극대화하는 설정을 `MpvCore.cpp`에 하드코딩하여 모든 사용자가 설정 없이도 최고의 경험을 누리게 합니다.

### 2.1 화질 극대화 (Video Fidelity)

MPV의 강력한 `gpu-next` 렌더러와 고품질 셰이더를 활용합니다.

| 설정 항목 | 적용 값 | 효과 및 이유 |
| :--- | :--- | :--- |
| **비디오 출력** | `vo=gpu-next` | 차세대 MPV 렌더러. Dolby Vision 지원, 향상된 HDR 톤매핑, 더 정확한 색상 연산 제공. |
| **업스케일링** | `scale=ewa_lanczossharp` <br> `dscale=mitchell` <br> `cscale=sinc` | 1080p → 4K 업스케일링 시 현존 최고의 선명도 유지. 크로마 업스케일링 최적화. |
| **HDR 톤매핑** | `tone-mapping=bt.2446a` <br> `hdr-compute-peak=yes` | HDR 영상을 SDR 모니터에서 재생할 때, 가장 자연스럽고 명암비가 살아있는 최신 톤매핑 알고리즘. |
| **색상 관리** | `target-colorspace-hint=yes` | 모니터의 색공간(ICC 프로파일 또는 EDID)을 읽어와 영상의 색공간(BT.709/BT.2020)을 정확하게 변환. |
| **디밴딩** | `deband=yes` <br> `deband-iterations=2` | 어두운 장면이나 애니메이션의 등고선(Banding) 노이즈 제거. |
| **디더링** | `dither-depth=auto` <br> `dither=fruit` | 8bit/10bit 패널에 맞춰 디더링을 적용하여 부드러운 그라데이션 표현. |

### 2.2 음질 극대화 (Audio Fidelity)

Windows 오디오 스택을 완전히 우회하여 원음 그대로 DAC에 전달합니다 (Bit-Perfect).

| 설정 항목 | 적용 값 | 효과 및 이유 |
| :--- | :--- | :--- |
| **출력 API** | `ao=wasapi` | Windows Audio Session API 사용. |
| **독점 모드** | `audio-exclusive=yes` | **핵심 설정**. Windows 믹서(볼륨 조절, 시스템음 믹싱, 강제 리샘플링)를 완전히 우회하여 오디오 하드웨어 통제권 독점. |
| **샘플레이트** | `audio-samplerate=0` (기본값) | 원본 음원의 샘플레이트(예: 44.1kHz, 192kHz)를 그대로 DAC에 전달 (Bit-Perfect). |
| **패스스루** | `audio-spdif=ac3,eac3,dts,dts-hd,truehd` | 돌비 애트모스, DTS:X 등을 디코딩하지 않고 리시버/사운드바로 원본 비트스트림 전송. |

---

## 3. 구현 계획

1. `MpvCore.cpp`의 `initialize()` 함수 수정:
   - 기존 `vo=libmpv` 설정은 유지하되, 내부 렌더링 옵션을 `gpu-next` 수준으로 끌어올리는 속성 추가. (libmpv를 사용할 때 `vo=gpu` 또는 `vo=gpu-next`를 직접 설정하면 창 생성 문제가 생길 수 있으므로, `gpu-api=opengl`과 함께 고품질 렌더링 속성을 주입).
   - 화질 최적화 옵션 (`scale`, `cscale`, `dscale`, `deband`, `dither`, `tone-mapping` 등) 하드코딩.
   - 음질 최적화 옵션 (`audio-exclusive=yes`) 추가.
2. 컴파일 및 빌드 후 4K 모니터 및 HDR 영상, 고해상도 FLAC 음원으로 테스트.
