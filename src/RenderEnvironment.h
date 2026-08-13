#pragma once
#include <QString>
#include <QScreen>
#include <QApplication>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QDebug>
#include <mpv/client.h>
#if defined(Q_OS_WIN)
#include <windows.h>  // GetSystemPowerStatus, SYSTEM_POWER_STATUS
#endif

/**
 * RenderEnvironment - 실행 환경 자동 감지 및 최적 렌더링 설정 결정
 *
 * 사용자에게 설정 변경을 요구하지 않고, 실행 시점에 환경을 감지하여
 * 모든 PC에서 끊김 없이 동작하는 최적 설정을 자동으로 선택한다.
 *
 * 감지 항목:
 *   - 화면 해상도 및 DPI 배율 (HiDPI 여부)
 *   - GPU 벤더 및 등급 (NVIDIA RTX/GTX / AMD RDNA / Intel Arc/UHD)
 *   - 모니터 주사율 (Hz)
 *   - 물리 픽셀 해상도 (렌더링 부하 추정)
 */

// ── GPU 벤더 분류 ─────────────────────────────────────────────────
enum class GpuVendor {
    Unknown,
    NvidiaRTX,      // RTX 20/30/40 시리즈 (Turing/Ampere/Ada) - 최고 성능
    NvidiaGTX,      // GTX 900~1600 시리즈 (Maxwell~Turing) - 중상급
    NvidiaLegacy,   // GTX 700 이하 - 구형
    AmdRDNA2Plus,   // RX 6000/7000 시리즈 (RDNA2/3) - 최고 성능
    AmdRDNA1,       // RX 5000 시리즈 (RDNA1) - 중상급
    AmdGCN,         // RX 400~590 시리즈 (GCN) - 중급
    IntelArc,       // Intel Arc A시리즈 - 중급
    IntelIris,      // Intel Iris Xe / UHD 620~770 - 통합 GPU
    IntelHD,        // Intel HD Graphics - 구형 통합 GPU
};

// ── GPU 성능 등급 ─────────────────────────────────────────────────
enum class GpuTier {
    Ultra,      // RTX 4080+, RX 7900 XT+ - 최고 성능
    High,       // RTX 3070+, RX 6700 XT+ - 고성능
    Medium,     // GTX 1070+, RX 580+ - 중급
    Low,        // GTX 960~, RX 470~, Intel Arc - 저중급
    Integrated, // Intel UHD/Iris, AMD APU - 통합 GPU
};

struct RenderEnvInfo {
    // 화면 정보
    qreal   dpr;            // devicePixelRatio
    int     physW;          // 물리 픽셀 너비
    int     physH;          // 물리 픽셀 높이
    double  refreshRate;    // 모니터 주사율 (Hz)
    bool    isHiDPI;        // DPR >= 1.5

    // GPU 정보
    GpuVendor gpuVendor;    // GPU 벤더 분류
    GpuTier   gpuTier;      // GPU 성능 등급
    QString   gpuRenderer;  // GL_RENDERER 문자열 (로그용)
    QString   gpuVendorStr; // GL_VENDOR 문자열 (로그용)

    // 물리 픽셀 부하 등급
    enum class PixelLoad { Low, Medium, High, Ultra };
    PixelLoad pixelLoad;

    // 자동 결정된 렌더링 설정
    QString scaleAlgo;
    QString dscaleAlgo;
    QString cscaleAlgo;
    bool    debandEnabled;
    int     debandIterations;
    bool    sigUpscaling;
    bool    linearUpscaling;
    bool    correctDownscaling;
    bool    hdrComputePeak;
    QString ditherMode;
    QString videoSync;
    QString framedrop;

    // 확장 환경 정보
    int     vramMb       = 0;      // VRAM 용량 (MB), 0=감지 불가
    bool    hdrEnabled   = false;  // Windows HDR 활성 여부
    bool    gpuNextReady = false;  // gpu-next 사용 가능 여부
    int     lavcThreads  = 0;      // 최적 디코딩 스레드 수 (0=자동)

    // 환경 설명 (로그용)
    QString description;
    // 노트북/전원 상태 (노트북 전용 최적화에 사용)
    bool    isLaptop     = false;  // 배터리 장착 여부 (노트북 감지)
    bool    isOnBattery  = false;  // 현재 배터리로 동작 중 (AC 미연결)
    bool    isDualGpu    = false;  // 듀얼 GPU 환경 (Optimus/Hybrid)
};

class RenderEnvironment {
public:
    // 전체 GPU/화면 탐지 없이 노트북 여부만 확인하는 경량 시작 경로.
    // 첫 프레임 전에는 GetSystemPowerStatus 한 번만 호출해 중복 환경 탐지를 피한다.
    static bool hasBattery() {
#if defined(Q_OS_WIN)
        SYSTEM_POWER_STATUS ps{};
        return GetSystemPowerStatus(&ps) && ps.BatteryFlag != 128 && ps.BatteryFlag != 255;
#else
        return false;
#endif
    }

    /**
     * 현재 실행 환경을 감지하고 최적 렌더링 설정을 반환한다.
     * QApplication 생성 후, OpenGL 컨텍스트 초기화 후 호출해야 한다.
     * OpenGL 컨텍스트가 없으면 화면 정보만으로 결정한다.
     */
    static RenderEnvInfo detect() {
        RenderEnvInfo info;

        // ── 화면 정보 감지 ────────────────────────────────────────
        QScreen* screen = QApplication::primaryScreen();
        if (!screen) return safeDefault();

        info.dpr         = screen->devicePixelRatio();
        info.refreshRate = screen->refreshRate();
        QSize logicalSize = screen->size();
        info.physW = static_cast<int>(logicalSize.width()  * info.dpr);
        info.physH = static_cast<int>(logicalSize.height() * info.dpr);
        info.isHiDPI = (info.dpr >= 1.5);

        // ── 물리 픽셀 부하 등급 ───────────────────────────────────
        long long totalPixels = static_cast<long long>(info.physW) * info.physH;
        if      (totalPixels <= 1920LL * 1080) info.pixelLoad = RenderEnvInfo::PixelLoad::Low;
        else if (totalPixels <= 2560LL * 1440) info.pixelLoad = RenderEnvInfo::PixelLoad::Medium;
        else if (totalPixels <= 3840LL * 2160) info.pixelLoad = RenderEnvInfo::PixelLoad::High;
        else                                   info.pixelLoad = RenderEnvInfo::PixelLoad::Ultra;

        // ── GPU 벤더 감지 (OpenGL 컨텍스트 필요) ─────────────────
        // initializeGL() 이후에 호출되면 GL_RENDERER/GL_VENDOR 읽기 가능
        // 컨텍스트 없으면 Unknown으로 처리 (화면 해상도만으로 결정)
        info.gpuVendor    = GpuVendor::Unknown;
        info.gpuTier      = GpuTier::Medium;  // 안전한 기본값
        info.gpuRenderer  = "Unknown";
        info.gpuVendorStr = "Unknown";

        QOpenGLContext* ctx = QOpenGLContext::currentContext();
        if (ctx) {
            QOpenGLFunctions* f = ctx->functions();
            if (f) {
                const char* renderer = reinterpret_cast<const char*>(
                    f->glGetString(GL_RENDERER));
                const char* vendor = reinterpret_cast<const char*>(
                    f->glGetString(GL_VENDOR));
                if (renderer) info.gpuRenderer  = QString::fromUtf8(renderer);
                if (vendor)   info.gpuVendorStr = QString::fromUtf8(vendor);
                classifyGpu(info);
            }
        }

        // ── 환경별 최적 렌더링 설정 결정 ─────────────────────────
        applySettingsForEnvironment(info);

        // ── 로그 설명 생성 ────────────────────────────────────────
        QString loadStr;
        switch (info.pixelLoad) {
        case RenderEnvInfo::PixelLoad::Low:    loadStr = "FHD"; break;
        case RenderEnvInfo::PixelLoad::Medium: loadStr = "QHD"; break;
        case RenderEnvInfo::PixelLoad::High:   loadStr = "4K";  break;
        case RenderEnvInfo::PixelLoad::Ultra:  loadStr = "5K+"; break;
        }
        info.description = QString(
            "화면: %1×%2 (DPR=%3, %4Hz, %5) | GPU: %6 [%7] | 렌더: scale=%8, deband=%9×%10")
            .arg(info.physW).arg(info.physH)
            .arg(info.dpr, 0, 'f', 1)
            .arg(static_cast<int>(info.refreshRate))
            .arg(loadStr)
            .arg(info.gpuRenderer)
            .arg(gpuTierName(info.gpuTier))
            .arg(info.scaleAlgo)
            .arg(info.debandEnabled ? "ON" : "OFF")
            .arg(info.debandIterations);

        // ── 확장 환경 정보 감지 ──────────────────────────────────
        info.vramMb       = detectVramMb();
        info.hdrEnabled   = detectHdrEnabled(screen);
        info.gpuNextReady = canUseGpuNext(info.gpuVendor, info.gpuTier);
        info.lavcThreads  = optimalLavcThreads(info.pixelLoad, info.gpuTier);
        // ── 노트북/배터리 상태 감지 ────────────────────────────────
        // Windows GetSystemPowerStatus API로 배터리 여부 및 충전 상태 확인
        // isLaptop: 배터리가 장착된 기기 = 노트북
        // isOnBattery: AC 연결 안 됨 = 배터리 모드 (성능 제한 상태)
        detectPowerStatus(info);
        // 듀얼 GPU 환경: 노트북에서 Intel 통합 GPU가 현재 렌더링 중이면
        // NvOptimusEnablement 선언에도 불구하고 통합 GPU로 돌아가는 경우 감지
        if (info.isLaptop && info.gpuTier == GpuTier::Integrated) {
            info.isDualGpu = true;  // 노트북 + 통합 GPU = 듀얼 GPU 환경 가능성 높음
            qInfo() << "[RenderEnv] 노트북 + 통합 GPU 감지 → Optimus/Hybrid 환경 가능성";
        }

        // VRAM 기반 GPU 등급 보정
        // GPU 이름만으로는 VRAM 용량을 알 수 없음 (GTX 1070 4GB vs 8GB 등)
        // VRAM이 충분하면 등급 상향 허용
        if (info.vramMb >= 8192 && info.gpuTier == GpuTier::Medium) {
            info.gpuTier = GpuTier::High;
            qInfo() << "[RenderEnv] VRAM 8GB+ 감지 → GPU 등급 Medium→High 상향";
            // 등급 상향에 따라 설정 재적용
            applySettingsForEnvironment(info);
        } else if (info.vramMb > 0 && info.vramMb < 2048 &&
                   info.gpuTier <= GpuTier::Medium) {
            // VRAM 2GB 미만: 고품질 필터 제한
            info.gpuTier = GpuTier::Low;
            qInfo() << "[RenderEnv] VRAM 2GB 미만 감지 → GPU 등급 Low 강등";
            applySettingsForEnvironment(info);
        }

        qInfo() << "[RenderEnv]" << info.description;
        if (info.vramMb > 0)
            qInfo() << "[RenderEnv] VRAM:" << info.vramMb << "MB"
                    << "| HDR:" << (info.hdrEnabled ? "ON" : "OFF")
                    << "| gpu-next:" << (info.gpuNextReady ? "가능" : "불가")
                    << "| lavc-threads:" << (info.lavcThreads == 0 ? "자동" : QString::number(info.lavcThreads));
        return info;
    }

    /**
     * 영상 해상도와 화면 해상도를 비교하여 업스케일 알고리즘을 최적화한다.
     * FILE_LOADED 이벤트에서 video-params/w, video-params/h 확인 후 호출.
     *
     * 핵심 원칙:
     *   - 영상 해상도 >= 화면 해상도: 다운스케일만 필요 → scale 부하 최소화
     *   - 영상 해상도 << 화면 해상도: 업스케일 효과 큼 → 고품질 알고리즘 적용
     */
    static void optimizeScaleForContent(mpv_handle* mpv,
                                        int videoW, int videoH,
                                        const RenderEnvInfo& env)
    {
        if (!mpv || videoW <= 0 || videoH <= 0) return;

        double scaleRatioW = static_cast<double>(env.physW) / videoW;
        double scaleRatioH = static_cast<double>(env.physH) / videoH;
        double scaleRatio  = qMax(scaleRatioW, scaleRatioH);

        // ── 업스케일 비율에 따른 알고리즘 선택 ───────────────────
        // 비율 < 1.0: 다운스케일 (4K 영상을 FHD 화면에서 재생 등)
        //   → scale 알고리즘 불필요, dscale만 최적화
        // 비율 1.0~1.5: 소폭 업스케일 (1080p → 1440p 등)
        //   → 중간 품질 알고리즘으로 충분
        // 비율 > 1.5: 대폭 업스케일 (480p → 4K 등)
        //   → 고품질 알고리즘 효과 극대화

        QString optimalScale;
        if (scaleRatio <= 1.0) {
            // 다운스케일: scale 알고리즘이 거의 영향 없음 → 부하 최소화
            optimalScale = "bilinear";
            qInfo() << "[RenderEnv] 다운스케일 감지 (ratio=" << scaleRatio
                    << ") → scale=bilinear (부하 최소화)";
        } else if (scaleRatio <= 1.5) {
            // 소폭 업스케일: spline36으로 충분
            optimalScale = "spline36";
            qInfo() << "[RenderEnv] 소폭 업스케일 (ratio=" << scaleRatio
                    << ") → scale=spline36";
        } else if (scaleRatio <= 3.0) {
            // 중간 업스케일: GPU 등급에 따라 선택
            if (env.gpuTier <= GpuTier::Medium) {
                optimalScale = "ewa_lanczossharp";
                qInfo() << "[RenderEnv] 중간 업스케일 (ratio=" << scaleRatio
                        << ") → scale=ewa_lanczossharp (고성능 GPU)";
            } else {
                optimalScale = "spline36";
                qInfo() << "[RenderEnv] 중간 업스케일 (ratio=" << scaleRatio
                        << ") → scale=spline36 (중급 GPU)";
            }
        } else {
            // 대폭 업스케일 (4배 이상): 고품질 알고리즘 효과 극대화
            // GPU 등급 무관하게 최고 품질 적용 (업스케일 효과가 매우 큼)
            optimalScale = "ewa_lanczossharp";
            qInfo() << "[RenderEnv] 대폭 업스케일 (ratio=" << scaleRatio
                    << ") → scale=ewa_lanczossharp (효과 극대화)";
        }

        mpv_set_property_string(mpv, "scale", optimalScale.toUtf8().constData());
    }

    /**
     * 모니터 주사율과 영상 FPS의 관계를 분석하여 최적 video-sync를 선택한다.
     * 단순 60fps 기준이 아닌, 배수 관계를 고려한 정밀 동기화.
     *
     * 핵심 원칙:
     *   - 정수 배수 관계: display-resample이 완벽하게 동작
     *   - 비정수 배수 (예: 60Hz에서 24fps = 2.5배): 저더 발생 가능
     *   - 120Hz에서 24fps = 5배: 완벽한 3:2 풀다운 → display-resample 최적
     */
    static QString selectVideoSync(double fps, double refreshRate) {
        if (fps <= 0 || refreshRate <= 0) return "audio";

        // 배수 관계 계산
        double ratio = refreshRate / fps;

        // 정수 배수 여부 확인 (오차 허용: ±0.05)
        double nearestInt = qRound(ratio);
        bool isIntegerMultiple = (qAbs(ratio - nearestInt) < 0.05) && (nearestInt >= 1.0);

        // 고프레임 영상 (60fps 초과): audio 모드가 안정적
        if (fps > 60.0) {
            qInfo() << "[RenderEnv] video-sync=audio (고프레임 fps=" << fps << ")";
            return "audio";
        }

        if (isIntegerMultiple) {
            // 정수 배수: display-resample 완벽 동작
            qInfo() << "[RenderEnv] video-sync=display-resample"
                    << "(fps=" << fps << ", refresh=" << refreshRate
                    << ", ratio=" << ratio << " ≈ " << nearestInt << "배)";
            return "display-resample";
        } else {
            // 비정수 배수: display-resample에서 저더 발생 가능
            // 예: 60Hz에서 24fps (ratio=2.5) → 3:2 풀다운 패턴 불완전
            // 120Hz 이상이면 비정수여도 display-resample이 더 좋음
            if (refreshRate >= 120.0) {
                qInfo() << "[RenderEnv] video-sync=display-resample"
                        << "(fps=" << fps << ", refresh=" << refreshRate
                        << ", ratio=" << ratio << " 비정수이나 120Hz+)";
                return "display-resample";
            } else {
                qInfo() << "[RenderEnv] video-sync=audio"
                        << "(fps=" << fps << ", refresh=" << refreshRate
                        << ", ratio=" << ratio << " 비정수 60Hz → audio 안전)";
                return "audio";
            }
        }
    }

    /**
     * 코덱에 따른 최적 하드웨어 디코딩 방식을 반환한다.
     * GPU 벤더와 코덱 조합으로 가장 안정적인 hwdec를 선택.
     */
    static QString selectHwdec(const QString& codec, GpuVendor vendor, GpuTier tier) {
        // ── 코덱별 hwdec 선택 기준 ────────────────────────────────
        // H.264: 모든 GPU에서 안정적 → d3d11va
        // H.265/HEVC: GTX 900+ 지원 → d3d11va (구형은 소프트웨어)
        // AV1: RTX 30+ / RX 6000+ 지원 → d3d11va (구형은 소프트웨어)
        // VP9: 대부분 지원 → d3d11va
        // MPEG-2: 레거시 → dxva2 (호환성 높음)
        // VC-1: 레거시 → dxva2

        QString codecLower = codec.toLower();

        // AV1: 구형 GPU에서 소프트웨어 디코딩이 더 안정적
        if (codecLower.contains("av1") || codecLower.contains("av01")) {
            if (vendor == GpuVendor::NvidiaRTX ||
                vendor == GpuVendor::AmdRDNA2Plus) {
                return "d3d11va";  // RTX 30+, RX 6000+: AV1 하드웨어 지원
            } else {
                return "no";  // 구형 GPU: AV1 소프트웨어 디코딩
            }
        }

        // H.265/HEVC: GPU 세대별 안전 처리
        // - Intel HD (Haswell~Skylake): HEVC 드라이버 버그로 시스템 프리즈 발생 → 소프트웨어
        // - Intel Iris/UHD (Kaby Lake+): d3d11va 직접 접근 시 Optimus 충돌 가능 → copy 방식
        // - NVIDIA/AMD: d3d11va 직접 사용 (안정적)
        if (codecLower.contains("hevc") || codecLower.contains("h265") ||
            codecLower.contains("h.265")) {
            if (tier == GpuTier::Integrated && vendor == GpuVendor::IntelHD) {
                return "no";  // 구형 Intel HD (Haswell~Skylake): HEVC 소프트웨어 강제
            }
            if (tier == GpuTier::Integrated &&
                (vendor == GpuVendor::IntelIris)) {
                return "d3d11va-copy";  // Intel Iris/UHD: copy 방식으로 Optimus 충돌 방지
            }
            return "d3d11va";
        }

        // MPEG-2, VC-1: 레거시 코덱 → dxva2 호환성 우선
        if (codecLower.contains("mpeg2") || codecLower.contains("mpeg-2") ||
            codecLower.contains("vc1")   || codecLower.contains("vc-1")   ||
            codecLower.contains("wmv3")) {
            return "dxva2";
        }

        // H.264, VP9, VP8: 모든 GPU에서 안정적
        if (codecLower.contains("h264") || codecLower.contains("h.264") ||
            codecLower.contains("avc")  || codecLower.contains("vp9")   ||
            codecLower.contains("vp8")) {
            return "d3d11va";
        }

        // 기타 코덱: auto-safe (실패 시 소프트웨어 폴백)
        return "auto-safe";
    }

    // GPU 등급 이름 반환 (로그용)
    static QString gpuTierName(GpuTier tier) {
        switch (tier) {
        case GpuTier::Ultra:      return "Ultra (RTX 4080+)";
        case GpuTier::High:       return "High (RTX 3070+)";
        case GpuTier::Medium:     return "Medium (GTX 1070+)";
        case GpuTier::Low:        return "Low (GTX 960+)";
        case GpuTier::Integrated: return "Integrated GPU";
        }
        return "Unknown";
    }

    // ── VRAM 용량 감지 ────────────────────────────────────────────
    // GL_NVX_gpu_memory_info (NVIDIA) 또는 GL_ATI_meminfo (AMD) 확장 사용
    // 컨텍스트 없거나 확장 미지원 시 0 반환 (안전 기본값 사용)
    static int detectVramMb() {
        QOpenGLContext* ctx = QOpenGLContext::currentContext();
        if (!ctx) return 0;

        // NVIDIA: GL_NVX_gpu_memory_info
        // GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX = 0x9048
        if (ctx->hasExtension("GL_NVX_gpu_memory_info")) {
            QOpenGLFunctions* f = ctx->functions();
            if (f) {
                GLint totalKb = 0;
                f->glGetIntegerv(0x9048, &totalKb);
                if (totalKb > 0) {
                    int vramMb = totalKb / 1024;
                    qInfo() << "[RenderEnv] NVIDIA VRAM:" << vramMb << "MB";
                    return vramMb;
                }
            }
        }

        // AMD: GL_ATI_meminfo
        // GL_TEXTURE_FREE_MEMORY_ATI = 0x87FC
        if (ctx->hasExtension("GL_ATI_meminfo")) {
            QOpenGLFunctions* f = ctx->functions();
            if (f) {
                GLint memInfo[4] = {0};
                f->glGetIntegerv(0x87FC, memInfo);
                if (memInfo[0] > 0) {
                    int vramMb = memInfo[0] / 1024;
                    qInfo() << "[RenderEnv] AMD VRAM (free):" << vramMb << "MB";
                    return vramMb;  // 여유 VRAM (전체보다 작을 수 있음)
                }
            }
        }

        qInfo() << "[RenderEnv] VRAM 감지 불가 (확장 미지원)";
        return 0;
    }

    // ── HDR 디스플레이 감지 (Windows HDR 활성 여부) ───────────────
    // QScreen::hdrEnabled()는 Qt 6.8에서 추가됨 (현재 Qt 6.7.3 빌드 → 폴백 사용)
    static bool detectHdrEnabled(QScreen* screen = nullptr) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        // Qt 6.8+: QScreen::hdrEnabled() 사용 (HDR 디스플레이 자동 감지)
        if (!screen) screen = QApplication::primaryScreen();
        if (screen) {
            bool hdr = screen->hdrEnabled();
            qInfo() << "[HDR] QScreen::hdrEnabled():" << hdr
                    << "| Screen:" << screen->name();
            return hdr;
        }
        return false;
#elif defined(Q_OS_WIN)
        // Qt 6.7 이하 폴백: Windows DISPLAYCONFIG API (Qt 6.8 업그레이드 시 자동 활성화)
        Q_UNUSED(screen)
        return false;
#else
        Q_UNUSED(screen)
        return false;
#endif
    }

    // ── gpu-next 자동 전환 조건 판단 ─────────────────────────────
    // GPU 등급과 OpenGL 버전을 확인하여 gpu-next 사용 가능 여부 판단
    // NVIDIA RTX / AMD RDNA2+ / Intel Arc에서 안정적
    static bool canUseGpuNext(GpuVendor vendor, GpuTier tier) {
        // 통합 GPU: gpu-next 불필요 (bilinear 사용 중)
        if (tier == GpuTier::Integrated) return false;

        // NVIDIA RTX (Turing+): Vulkan 드라이버 안정적
        if (vendor == GpuVendor::NvidiaRTX) return true;

        // NVIDIA GTX: OpenGL 경로가 더 안정적
        if (vendor == GpuVendor::NvidiaGTX ||
            vendor == GpuVendor::NvidiaLegacy) return false;

        // AMD RDNA2+: Vulkan 드라이버 안정적
        if (vendor == GpuVendor::AmdRDNA2Plus) return true;

        // AMD RDNA1/GCN: 아직 불안정 케이스 있음
        if (vendor == GpuVendor::AmdRDNA1 ||
            vendor == GpuVendor::AmdGCN) return false;

        // Intel Arc: 드라이버 개선 중, 조건부 허용
        if (vendor == GpuVendor::IntelArc) return false;  // 아직 비활성

        return false;
    }

    // ── 디코딩 스레드 수 GPU 부하 연동 최적화 ────────────────────
    // GPU 렌더링 부하가 높은 환경에서 CPU 스레드를 제한하여
    // CPU-GPU 메모리 대역폭 경합 감소
    static int optimalLavcThreads(RenderEnvInfo::PixelLoad pixelLoad, GpuTier gpuTier) {
        if (gpuTier == GpuTier::Integrated) {
            // 통합 GPU: CPU와 메모리 공유 → 스레드 제한
            return 2;
        }
        switch (pixelLoad) {
        case RenderEnvInfo::PixelLoad::Low:    return 0;  // FHD: 자동 (제한 없음)
        case RenderEnvInfo::PixelLoad::Medium: return 0;  // QHD: 자동
        case RenderEnvInfo::PixelLoad::High:   return 4;  // 4K: 4스레드 제한 (대역폭 경합 감소)
        case RenderEnvInfo::PixelLoad::Ultra:  return 4;  // 5K+: 4스레드 제한
        }
        return 0;
    }


private:
    // ── GPU 벤더 및 등급 분류 ─────────────────────────────────────
    static void classifyGpu(RenderEnvInfo& info) {
        QString renderer = info.gpuRenderer.toLower();
        QString vendor   = info.gpuVendorStr.toLower();

        if (vendor.contains("nvidia") || renderer.contains("nvidia") ||
            renderer.contains("geforce") || renderer.contains("quadro")) {
            classifyNvidia(info, renderer);
        } else if (vendor.contains("ati") || vendor.contains("amd") ||
                   renderer.contains("radeon") || renderer.contains("amd")) {
            classifyAmd(info, renderer);
        } else if (vendor.contains("intel") || renderer.contains("intel")) {
            classifyIntel(info, renderer);
        } else {
            info.gpuVendor = GpuVendor::Unknown;
            info.gpuTier   = GpuTier::Medium;  // 안전한 기본값
        }
    }

    static void classifyNvidia(RenderEnvInfo& info, const QString& renderer) {
        // RTX 시리즈 감지 (20/30/40 시리즈)
        if (renderer.contains("rtx")) {
            info.gpuVendor = GpuVendor::NvidiaRTX;
            // RTX 4080+: Ultra, RTX 3070+: High, RTX 2060+: Medium
            if (renderer.contains("rtx 40") || renderer.contains("rtx 50")) {
                info.gpuTier = GpuTier::Ultra;
            } else if (renderer.contains("rtx 30") ||
                       (renderer.contains("rtx 20") && !renderer.contains("rtx 2060"))) {
                info.gpuTier = GpuTier::High;
            } else {
                info.gpuTier = GpuTier::Medium;
            }
        }
        // GTX 시리즈
        else if (renderer.contains("gtx")) {
            info.gpuVendor = GpuVendor::NvidiaGTX;
            // GTX 1080 Ti / 1080 / 1070: Medium
            if (renderer.contains("gtx 1080") || renderer.contains("gtx 1070") ||
                renderer.contains("gtx 1660") || renderer.contains("gtx 1650")) {
                info.gpuTier = GpuTier::Medium;
            }
            // GTX 1060 / 970 / 980: Low-Medium
            else if (renderer.contains("gtx 1060") || renderer.contains("gtx 970") ||
                     renderer.contains("gtx 980")  || renderer.contains("gtx 960")) {
                info.gpuTier = GpuTier::Low;
            }
            // GTX 900 이하: Legacy
            else {
                info.gpuVendor = GpuVendor::NvidiaLegacy;
                info.gpuTier   = GpuTier::Low;
            }
        }
        // Tesla / Titan / 기타 NVIDIA
        else {
            info.gpuVendor = GpuVendor::NvidiaGTX;
            info.gpuTier   = GpuTier::Medium;
        }
    }

    static void classifyAmd(RenderEnvInfo& info, const QString& renderer) {
        // RX 7000 시리즈 (RDNA3)
        if (renderer.contains("rx 7")) {
            info.gpuVendor = GpuVendor::AmdRDNA2Plus;
            info.gpuTier   = renderer.contains("rx 7900") ? GpuTier::Ultra : GpuTier::High;
        }
        // RX 6000 시리즈 (RDNA2)
        else if (renderer.contains("rx 6")) {
            info.gpuVendor = GpuVendor::AmdRDNA2Plus;
            info.gpuTier   = (renderer.contains("rx 6700") ||
                              renderer.contains("rx 6800") ||
                              renderer.contains("rx 6900")) ? GpuTier::High : GpuTier::Medium;
        }
        // RX 5000 시리즈 (RDNA1)
        else if (renderer.contains("rx 5")) {
            info.gpuVendor = GpuVendor::AmdRDNA1;
            info.gpuTier   = GpuTier::Medium;
        }
        // RX 400/500/590 시리즈 (GCN)
        else if (renderer.contains("rx 4") || renderer.contains("rx 5") ||
                 renderer.contains("rx 580") || renderer.contains("rx 570")) {
            info.gpuVendor = GpuVendor::AmdGCN;
            info.gpuTier   = GpuTier::Low;
        }
        // 기타 AMD
        else {
            info.gpuVendor = GpuVendor::AmdGCN;
            info.gpuTier   = GpuTier::Low;
        }
    }

    static void classifyIntel(RenderEnvInfo& info, const QString& renderer) {
        // Intel Arc (독립 GPU)
        if (renderer.contains("arc")) {
            info.gpuVendor = GpuVendor::IntelArc;
            info.gpuTier   = GpuTier::Low;
        }
        // Intel Iris Xe / UHD 750+ (Tiger Lake+)
        else if (renderer.contains("iris xe") || renderer.contains("iris(r) xe") ||
                 renderer.contains("uhd 770")  || renderer.contains("uhd 750")) {
            info.gpuVendor = GpuVendor::IntelIris;
            info.gpuTier   = GpuTier::Integrated;
        }
        // Intel UHD 600~730 / Iris Plus
        else if (renderer.contains("uhd") || renderer.contains("iris plus") ||
                 renderer.contains("iris(r) plus")) {
            info.gpuVendor = GpuVendor::IntelIris;
            info.gpuTier   = GpuTier::Integrated;
        }
        // Intel HD Graphics (구형)
        else {
            info.gpuVendor = GpuVendor::IntelHD;
            info.gpuTier   = GpuTier::Integrated;
        }
    }

    // ── 환경별 최적 렌더링 설정 결정 ─────────────────────────────
    static void applySettingsForEnvironment(RenderEnvInfo& info) {
        // ── 공통 기본값 ───────────────────────────────────────────
        info.dscaleAlgo   = "mitchell";
        info.sigUpscaling = true;
        info.ditherMode   = "fruit";
        info.framedrop    = "vo";
        info.videoSync    = "audio";

        // ── GPU 등급 + 화면 해상도 조합으로 최적 설정 결정 ───────
        // 통합 GPU는 해상도에 관계없이 Eco 설정 강제
        if (info.gpuTier == GpuTier::Integrated) {
            applyIntegratedGpuSettings(info);
            return;
        }

        // 독립 GPU: 화면 해상도 × GPU 등급 조합
        switch (info.pixelLoad) {
        case RenderEnvInfo::PixelLoad::Low:
            // FHD: GPU 등급에 따라 최대 품질
            if (info.gpuTier <= GpuTier::High) {
                // Ultra/High GPU + FHD: 최고 품질
                info.scaleAlgo          = "ewa_lanczossharp";
                info.cscaleAlgo         = "sinc";
                info.debandEnabled      = true;
                info.debandIterations   = 2;
                info.linearUpscaling    = true;
                info.correctDownscaling = true;
                info.hdrComputePeak     = true;
            } else if (info.gpuTier == GpuTier::Medium) {
                // Medium GPU + FHD: 고품질
                info.scaleAlgo          = "ewa_lanczossharp";
                info.cscaleAlgo         = "sinc";
                info.debandEnabled      = true;
                info.debandIterations   = 1;
                info.linearUpscaling    = true;
                info.correctDownscaling = true;
                info.hdrComputePeak     = true;
            } else {
                // Low GPU + FHD: 균형
                info.scaleAlgo          = "spline36";
                info.cscaleAlgo         = "spline36";
                info.debandEnabled      = true;
                info.debandIterations   = 1;
                info.linearUpscaling    = false;
                info.correctDownscaling = true;
                info.hdrComputePeak     = false;
            }
            break;

        case RenderEnvInfo::PixelLoad::Medium:
            // QHD: GPU 등급에 따라 조정
            if (info.gpuTier <= GpuTier::High) {
                info.scaleAlgo          = "ewa_lanczossharp";
                info.cscaleAlgo         = "sinc";
                info.debandEnabled      = true;
                info.debandIterations   = 1;
                info.linearUpscaling    = true;
                info.correctDownscaling = true;
                info.hdrComputePeak     = true;
            } else {
                // Medium/Low GPU + QHD: 균형
                info.scaleAlgo          = "spline36";
                info.cscaleAlgo         = "spline36";
                info.debandEnabled      = true;
                info.debandIterations   = 1;
                info.linearUpscaling    = false;
                info.correctDownscaling = true;
                info.hdrComputePeak     = false;
            }
            break;

        case RenderEnvInfo::PixelLoad::High:
            // 4K: Ultra/High GPU만 고품질 허용
            if (info.gpuTier == GpuTier::Ultra) {
                // RTX 4080+: 4K에서도 고품질 가능
                info.scaleAlgo          = "ewa_lanczossharp";
                info.cscaleAlgo         = "sinc";
                info.debandEnabled      = true;
                info.debandIterations   = 1;
                info.linearUpscaling    = false;
                info.correctDownscaling = true;
                info.hdrComputePeak     = true;
            } else if (info.gpuTier == GpuTier::High) {
                // RTX 3070+: 4K 균형
                info.scaleAlgo          = "spline36";
                info.cscaleAlgo         = "spline36";
                info.debandEnabled      = true;
                info.debandIterations   = 1;
                info.linearUpscaling    = false;
                info.correctDownscaling = true;
                info.hdrComputePeak     = false;
            } else {
                // Medium/Low GPU + 4K: 안정성 우선 (GTX 1070 등)
                info.scaleAlgo          = "spline36";
                info.cscaleAlgo         = "spline36";
                info.debandEnabled      = true;
                info.debandIterations   = 1;
                info.linearUpscaling    = false;
                info.correctDownscaling = false;
                info.hdrComputePeak     = false;
            }
            break;

        case RenderEnvInfo::PixelLoad::Ultra:
            // 5K+: 안정성 최우선 (GPU 등급 무관)
            info.scaleAlgo          = "lanczos";
            info.cscaleAlgo         = "lanczos";
            info.debandEnabled      = false;
            info.debandIterations   = 0;
            info.linearUpscaling    = false;
            info.correctDownscaling = false;
            info.hdrComputePeak     = false;
            info.sigUpscaling       = false;
            info.ditherMode         = "no";
            break;
        }
    }

    static void applyIntegratedGpuSettings(RenderEnvInfo& info) {
        // 통합 GPU: 해상도에 관계없이 최소 부하 설정
        // Intel UHD/Iris, AMD APU 등
        info.scaleAlgo          = "bilinear";   // 최소 부하
        info.cscaleAlgo         = "bilinear";
        info.dscaleAlgo         = "bilinear";
        info.debandEnabled      = false;        // 통합 GPU에서 deband 비활성화
        info.debandIterations   = 0;
        info.sigUpscaling       = false;
        info.linearUpscaling    = false;
        info.correctDownscaling = false;
        info.hdrComputePeak     = false;
        info.ditherMode         = "no";
        qInfo() << "[RenderEnv] 통합 GPU 감지 → 최소 부하 설정 적용";
    }

    // ── 배터리/전원 상태 감지 (Windows GetSystemPowerStatus) ──────────
    // 노트북 감지: 배터리가 장착된 기기 = 노트북
    // 배터리 모드: AC 연결 안 됨 = 성능 제한 상태 (끊김 유발 주원)
    static void detectPowerStatus(RenderEnvInfo& info) {
#if defined(Q_OS_WIN)
        SYSTEM_POWER_STATUS ps;
        if (GetSystemPowerStatus(&ps)) {
            // ACLineStatus: 0=배터리, 1=AC, 255=알 수 없음
            // BatteryFlag: 128=배터리 없음, 255=알 수 없음
            bool hasBattery = (ps.BatteryFlag != 128) && (ps.BatteryFlag != 255);
            info.isLaptop    = hasBattery;
            info.isOnBattery = hasBattery && (ps.ACLineStatus == 0);
            qInfo() << "[RenderEnv] 전원 상태:"
                    << (hasBattery ? "노트북" : "데스크탑")
                    << "| AC:" << (ps.ACLineStatus == 1 ? "연결" : "미연결")
                    << "| 배터리:" << (int)ps.BatteryLifePercent << "%";
        }
#else
        Q_UNUSED(info);
#endif
    }

    static RenderEnvInfo safeDefault() {
        RenderEnvInfo info;
        info.dpr              = 1.0;
        info.physW            = 1920;
        info.physH            = 1080;
        info.refreshRate      = 60.0;
        info.isHiDPI          = false;
        info.gpuVendor        = GpuVendor::Unknown;
        info.gpuTier          = GpuTier::Medium;
        info.gpuRenderer      = "Unknown";
        info.gpuVendorStr     = "Unknown";
        info.pixelLoad        = RenderEnvInfo::PixelLoad::Low;
        info.scaleAlgo        = "spline36";
        info.dscaleAlgo       = "mitchell";
        info.cscaleAlgo       = "spline36";
        info.debandEnabled    = true;
        info.debandIterations = 1;
        info.sigUpscaling     = true;
        info.linearUpscaling  = false;
        info.correctDownscaling = true;
        info.hdrComputePeak   = false;
        info.ditherMode       = "fruit";
        info.videoSync        = "audio";
        info.framedrop        = "vo";
        info.description      = "기본값 (화면 감지 실패)";
        return info;
    }
};
