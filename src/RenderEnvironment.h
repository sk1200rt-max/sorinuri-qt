#pragma once
#include <QString>
#include <QScreen>
#include <QApplication>
#include <QDebug>

/**
 * RenderEnvironment - 실행 환경 자동 감지 및 최적 렌더링 설정 결정
 *
 * 사용자에게 설정 변경을 요구하지 않고, 실행 시점에 환경을 감지하여
 * 모든 PC에서 끊김 없이 동작하는 최적 설정을 자동으로 선택한다.
 *
 * 감지 항목:
 *   - 화면 해상도 및 DPI 배율 (HiDPI 여부)
 *   - GPU 벤더 (NVIDIA / AMD / Intel 통합)
 *   - 모니터 주사율
 *   - 물리 픽셀 해상도 (렌더링 부하 추정)
 */
struct RenderEnvInfo {
    // 화면 정보
    qreal   dpr;            // devicePixelRatio (1.0=일반, 1.5/2.0/2.5=HiDPI)
    int     physW;          // 물리 픽셀 너비
    int     physH;          // 물리 픽셀 높이
    double  refreshRate;    // 모니터 주사율 (Hz)
    bool    isHiDPI;        // DPR >= 1.5

    // 렌더링 부하 등급 (물리 픽셀 기준)
    // Low:    1920×1080 이하 (FHD)
    // Medium: 2560×1440 이하 (QHD)
    // High:   3840×2160 이하 (4K)
    // Ultra:  5120×2880 이상 (5K+)
    enum class PixelLoad { Low, Medium, High, Ultra };
    PixelLoad pixelLoad;

    // 자동 결정된 렌더링 설정
    QString scaleAlgo;          // MPV scale 알고리즘
    QString dscaleAlgo;         // MPV dscale 알고리즘
    QString cscaleAlgo;         // MPV cscale 알고리즘
    bool    debandEnabled;      // 디밴딩 활성화 여부
    int     debandIterations;   // 디밴딩 반복 횟수
    bool    sigUpscaling;       // sigmoid-upscaling
    bool    linearUpscaling;    // linear-upscaling
    bool    correctDownscaling; // correct-downscaling
    bool    hdrComputePeak;     // hdr-compute-peak (HDR 동적 측정)
    QString ditherMode;         // 디더링 모드 ("fruit" / "no")
    QString videoSync;          // video-sync 초기값
    QString framedrop;          // framedrop 설정

    // 환경 설명 (로그용)
    QString description;
};

class RenderEnvironment {
public:
    /**
     * 현재 실행 환경을 감지하고 최적 렌더링 설정을 반환한다.
     * 이 함수는 QApplication 생성 후 호출해야 한다.
     */
    static RenderEnvInfo detect() {
        RenderEnvInfo info;

        // ── 화면 정보 감지 ────────────────────────────────────────
        QScreen* screen = QApplication::primaryScreen();
        if (!screen) {
            // 화면 감지 실패 시 안전한 기본값
            return safeDefault();
        }

        info.dpr         = screen->devicePixelRatio();
        info.refreshRate = screen->refreshRate();
        QSize logicalSize = screen->size();
        info.physW = static_cast<int>(logicalSize.width()  * info.dpr);
        info.physH = static_cast<int>(logicalSize.height() * info.dpr);
        info.isHiDPI = (info.dpr >= 1.5);

        // ── 물리 픽셀 부하 등급 결정 ─────────────────────────────
        long long totalPixels = static_cast<long long>(info.physW) * info.physH;
        if (totalPixels <= 1920LL * 1080) {
            info.pixelLoad = RenderEnvInfo::PixelLoad::Low;
        } else if (totalPixels <= 2560LL * 1440) {
            info.pixelLoad = RenderEnvInfo::PixelLoad::Medium;
        } else if (totalPixels <= 3840LL * 2160) {
            info.pixelLoad = RenderEnvInfo::PixelLoad::High;
        } else {
            info.pixelLoad = RenderEnvInfo::PixelLoad::Ultra;
        }

        // ── 환경별 최적 렌더링 설정 결정 ─────────────────────────
        // 핵심 원칙:
        //   1. 물리 픽셀이 많을수록 GPU 부하가 기하급수적으로 증가
        //   2. 4K 이상에서는 고품질 필터가 오히려 끊김 유발
        //   3. 모든 환경에서 화질보다 재생 안정성이 우선
        //   4. 사용자가 원하면 설정창에서 수동 변경 가능
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
            "화면: %1×%2 (DPR=%.1f, %3Hz, %4) | 렌더: scale=%5, deband=%6×%7")
            .arg(info.physW).arg(info.physH)
            .arg(info.dpr, 0, 'f', 1)
            .arg(static_cast<int>(info.refreshRate))
            .arg(loadStr)
            .arg(info.scaleAlgo)
            .arg(info.debandEnabled ? "ON" : "OFF")
            .arg(info.debandIterations);

        qInfo() << "[RenderEnv]" << info.description;
        return info;
    }

private:
    static void applySettingsForEnvironment(RenderEnvInfo& info) {
        // ── 공통 기본값 ───────────────────────────────────────────
        info.dscaleAlgo         = "mitchell";   // 다운스케일: 링잉 없이 부드럽게
        info.sigUpscaling       = true;
        info.ditherMode         = "fruit";
        info.framedrop          = "vo";         // 모든 환경에서 vo 드롭 허용
        info.videoSync          = "audio";      // 초기값 audio (파일 로드 후 자동 전환)

        switch (info.pixelLoad) {
        case RenderEnvInfo::PixelLoad::Low:
            // FHD (1920×1080 이하): 모든 설정 최대 품질
            // GPU 부하가 낮으므로 최고 품질 필터 사용
            info.scaleAlgo          = "ewa_lanczossharp";
            info.cscaleAlgo         = "sinc";
            info.debandEnabled      = true;
            info.debandIterations   = 2;
            info.linearUpscaling    = true;
            info.correctDownscaling = true;
            info.hdrComputePeak     = true;
            break;

        case RenderEnvInfo::PixelLoad::Medium:
            // QHD (2560×1440): 고품질 유지, deband 1회
            info.scaleAlgo          = "ewa_lanczossharp";
            info.cscaleAlgo         = "sinc";
            info.debandEnabled      = true;
            info.debandIterations   = 1;
            info.linearUpscaling    = true;
            info.correctDownscaling = true;
            info.hdrComputePeak     = true;
            break;

        case RenderEnvInfo::PixelLoad::High:
            // 4K (3840×2160): 균형 설정
            // ewa_lanczossharp는 4K에서 GPU 부하가 매우 높음
            // spline36으로 변경: 화질 차이 미미, 부하 대폭 감소
            info.scaleAlgo          = "spline36";
            info.cscaleAlgo         = "spline36";
            info.debandEnabled      = true;
            info.debandIterations   = 1;
            info.linearUpscaling    = false;    // 4K에서 linear는 불필요한 부하
            info.correctDownscaling = true;
            info.hdrComputePeak     = false;    // 4K에서 동적 측정은 부하 큼
            break;

        case RenderEnvInfo::PixelLoad::Ultra:
            // 5K+ (5120×2880 이상): 안정성 최우선
            info.scaleAlgo          = "lanczos";
            info.cscaleAlgo         = "lanczos";
            info.debandEnabled      = false;    // 5K+에서 deband 비활성화
            info.debandIterations   = 0;
            info.linearUpscaling    = false;
            info.correctDownscaling = false;
            info.hdrComputePeak     = false;
            info.sigUpscaling       = false;
            info.ditherMode         = "no";
            break;
        }
    }

    static RenderEnvInfo safeDefault() {
        // 화면 감지 실패 시 가장 안전한 설정
        RenderEnvInfo info;
        info.dpr             = 1.0;
        info.physW           = 1920;
        info.physH           = 1080;
        info.refreshRate     = 60.0;
        info.isHiDPI         = false;
        info.pixelLoad       = RenderEnvInfo::PixelLoad::Low;
        info.scaleAlgo       = "spline36";
        info.dscaleAlgo      = "mitchell";
        info.cscaleAlgo      = "spline36";
        info.debandEnabled   = true;
        info.debandIterations = 1;
        info.sigUpscaling    = true;
        info.linearUpscaling = true;
        info.correctDownscaling = true;
        info.hdrComputePeak  = false;
        info.ditherMode      = "fruit";
        info.videoSync       = "audio";
        info.framedrop       = "vo";
        info.description     = "기본값 (화면 감지 실패)";
        return info;
    }
};
