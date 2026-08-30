#include "MpvCore.h"
#include "RenderEnvironment.h"
#include <QDebug>
#include <QSettings>
#include <QSet>
#include <QThread>
#include <QMetaObject>
#include <QFile>
#include <QDateTime>
#include <QTextStream>
#include <locale.h>
#include <cstring>
#include <stdexcept>

// 진단용 앱 로그 (기본 비활성화 - 사용자 PC에 파일을 남기지 않음)
// 문제 진단 시에만 SORINURI_DEBUG 환경변수를 설정하면 활성화됨
static bool debugLogEnabled() {
    static const bool enabled = qEnvironmentVariableIsSet("SORINURI_DEBUG");
    return enabled;
}
static void appLog(const QString& msg) {
    if (!debugLogEnabled()) return;
    static QFile f("C:/Users/Public/sorinuri_app.log");
    if (!f.isOpen()) f.open(QIODevice::Append | QIODevice::Text);
    QTextStream ts(&f);
    ts << QDateTime::currentDateTime().toString("HH:mm:ss.zzz") << " " << msg << "\n";
    ts.flush();
}

// ─── 헬퍼 함수 ──────────────────────────────────────────────────────
static void check_error(int status) {
    if (status < 0)
        qWarning() << "[MPV] Error:" << mpv_error_string(status);
}

static int channelCountFromLayout(const QString& layout) {
    const QString value = layout.trimmed().toLower();
    if (value == "mono") return 1;
    if (value == "stereo") return 2;
    if (value.contains("7.1")) return 8;
    if (value.contains("6.1")) return 7;
    if (value.contains("5.1")) return 6;
    bool ok = false;
    const int count = value.toInt(&ok);
    return ok ? count : 0;
}

static QString channelLayoutLabel(int channels) {
    switch (channels) {
    case 1: return QStringLiteral("1.0");
    case 2: return QStringLiteral("2.0");
    case 6: return QStringLiteral("5.1");
    case 7: return QStringLiteral("6.1");
    case 8: return QStringLiteral("7.1");
    default: return channels > 0 ? QString::number(channels) + QStringLiteral("ch") : QString();
    }
}

// audio-out-params는 디코더 원본이 아니라 실제 오디오 API로 전달되는 값이다.
// UI에는 이 값을 우선 표시해 원본 6ch/HDMI PCM 2.0 같은 혼동을 막는다.
static QString actualAudioOutput(mpv_handle* mpv, int& channelCount, int& sampleRate) {
    int64_t outputChannels = 0;
    int64_t outputRate = 0;
    mpv_get_property(mpv, "audio-out-params/channel-count", MPV_FORMAT_INT64, &outputChannels);
    mpv_get_property(mpv, "audio-out-params/samplerate", MPV_FORMAT_INT64, &outputRate);

    char* formatRaw = mpv_get_property_string(mpv, "audio-out-params/format");
    char* layoutRaw = mpv_get_property_string(mpv, "audio-out-params/hr-channels");
    const QString format = formatRaw ? QString::fromUtf8(formatRaw) : QString();
    const QString layout = layoutRaw ? QString::fromUtf8(layoutRaw) : QString();
    if (formatRaw) mpv_free(formatRaw);
    if (layoutRaw) mpv_free(layoutRaw);

    if (outputChannels > 0) channelCount = static_cast<int>(outputChannels);
    else if (!layout.isEmpty()) channelCount = channelCountFromLayout(layout);
    if (outputRate > 0) sampleRate = static_cast<int>(outputRate);

    if (format.isEmpty()) return {};
    const bool bitstream = format.contains("spdif", Qt::CaseInsensitive) ||
                           format.contains("bitstream", Qt::CaseInsensitive);
    if (bitstream) return QStringLiteral("BITSTREAM");
    const QString displayLayout = channelLayoutLabel(channelCount);
    return displayLayout.isEmpty() ? QStringLiteral("PCM")
                                   : QStringLiteral("PCM ") + displayLayout;
}

MpvCore::MpvCore(QObject* parent) : QObject(parent) {
    setlocale(LC_NUMERIC, "C");

    // libmpv 객체는 initialize()에서 생성한다. Windows에서는 libmpv-2.dll을
    // delay-load하므로, 이 생성자를 첫 창 표시 전에 호출해도 미디어 DLL 적재와
    // GPU/WASAPI 초기화가 앞당겨지지 않는다.

    // mpv_set_wakeup_callback()이 새 이벤트를 Qt 이벤트 루프에 전달한다.
    // 별도 16ms polling timer를 두지 않아 유휴 상태의 지속 wake-up을 피한다.
}

MpvCore::~MpvCore() {
    shutdown();
}

void MpvCore::shutdown() {
    if (shuttingDown_) return;
    shuttingDown_ = true;

    if (spectrumTimer_) spectrumTimer_->stop();
    if (frameDropTimer_) frameDropTimer_->stop();

    if (!mpv_) {
        initialized_ = false;
        return;
    }

    // 종료 직전에 ao-reload·audio-exclusive 같은 비동기 재협상 요청을
    // 추가하지 않는다. 비동기 요청은 종료와 실행 순서가 보장되지 않으므로,
    // mpv_terminate_destroy()가 재생 코어·모든 client·WASAPI 출력 장치의
    // 종료를 완료할 때까지 기다리는 단일 동기 경로로 사용한다.
    mpv_set_wakeup_callback(mpv_, nullptr, nullptr);
    mpv_terminate_destroy(mpv_);
    mpv_ = nullptr;
    initialized_ = false;
    qInfo() << "[MPV] 종료 완료: libmpv/WASAPI 출력 장치 해제";
}

bool MpvCore::initialize(WId windowId) {
    if (initialized_) return true;

    // 첫 호출 시에만 libmpv 객체를 만든다. 이 지점은 MpvWidget의 첫 프레임 뒤
    // 지연 초기화 경로에서 호출되어, 실행 파일 로더가 UI 표시를 막지 않게 한다.
    if (!mpv_) {
        mpv_ = mpv_create();
        if (!mpv_) {
            qCritical() << "[MPV] mpv_create 실패";
            return false;
        }
    }

    // ── mpv_initialize 이전에 설정해야 하는 옵션 ──────────────────
    if (windowId != 0) {
        // 전통적 --wid 모드 (사용 안 함)
        int64_t wid = static_cast<int64_t>(windowId);
        check_error(mpv_set_option(mpv_, "wid", MPV_FORMAT_INT64, &wid));
        check_error(mpv_set_option_string(mpv_, "vo", "gpu"));
    } else {
        // render API 모드 (QOpenGLWidget + mpv_render_context)
        // vo=libmpv: MPV가 자체 윈도우 생성 안 함 (절대 변경 금지)
        check_error(mpv_set_option_string(mpv_, "vo", "libmpv"));
        // gpu-api: D3D11 (기본) / D3D12 (libplacebo, gpu-next 효과)
        // QSettings에서 저장된 gpu-api 값 적용 (다음 실행 시 적용 방식)
        {
            QSettings s("Sorinuri", "SorinuriPlayer");
            QString gpuApi = s.value("video/gpu_api", "d3d11").toString();
            check_error(mpv_set_option_string(mpv_, "gpu-api", gpuApi.toUtf8().constData()));
            qInfo() << "[MPV] gpu-api:" << gpuApi;
            // D3D12 선택 시 libplacebo 렌더러 활성화 (4K GPU 부하 20~30% 감소)
            if (gpuApi == "d3d12") {
                // libplacebo 고품질 설정 (gpu-next 효과)
                check_error(mpv_set_option_string(mpv_, "scale",  "ewa_lanczossharp"));
                check_error(mpv_set_option_string(mpv_, "dscale", "mitchell"));
                qInfo() << "[MPV] libplacebo 렌더러 활성화 (D3D12)";
            }
        }
    }

    // ── 진단용 MPV 내부 로그 파일: 기본 비활성화 ──────────────────
    // SORINURI_DEBUG 환경변수 설정 시에만 로그 파일 생성 (문제 진단용)
    if (qEnvironmentVariableIsSet("SORINURI_DEBUG")) {
        check_error(mpv_set_option_string(mpv_, "log-file", "C:/Users/Public/sorinuri_mpv.log"));
    }
    // error 레벨 이상을 이벤트로 수신 → AO 실패 감지에 사용 (필수, 삭제 금지)
    // 이는 메모리 내 이벤트 수신이며 파일을 생성하지 않음
    mpv_request_log_messages(mpv_, "error");

    check_error(mpv_set_option_string(mpv_, "osc",        "no"));
    check_error(mpv_set_option_string(mpv_, "idle",       "yes"));
    check_error(mpv_set_option_string(mpv_, "keep-open",  "always"));
    // 파일 로드 시 자동 재생: pause 초기값을 no로 설정
    // keep-open=always는 재생 완료 후 마지막 프레임에서 멈춰지만
    // 새 파일 로드 시 pause 상태를 유지하지 않음
    check_error(mpv_set_option_string(mpv_, "pause",      "no"));

    // ── 오디오: WASAPI 독점·HDMI 장치·패스스루 (mpv_initialize 전 적용) ──
    // DD+/DTS 비트스트림은 AO가 처음 열릴 때의 장치·독점·audio-spdif 조합으로
    // 결정된다. 초기화 뒤에 값을 나누어 적용하면 기본 2.0 장치로 먼저 협상된 뒤
    // PCM 폴백이 유지될 수 있으므로, 저장된 사용자 정책을 AO 생성 전에 한 번에 적용한다.
    {
        QSettings s("Sorinuri", "SorinuriPlayer");
        QString savedDevice = s.value("audio/device", "auto").toString().trimmed();
        if (savedDevice.isEmpty()) savedDevice = QStringLiteral("auto");
        const bool savedExclusive = s.value("audio/exclusive", true).toBool();
        passthroughEnabled_ = s.value("audio/passthrough", true).toBool();

        QStringList codecs;
        if (s.value("audio/pt_ac3", true).toBool()) codecs << QStringLiteral("ac3");
        if (s.value("audio/pt_eac3", true).toBool()) codecs << QStringLiteral("eac3");
        if (s.value("audio/pt_dts", true).toBool()) codecs << QStringLiteral("dts");
        if (s.value("audio/pt_dtshd", true).toBool()) codecs << QStringLiteral("dts-hd");
        if (s.value("audio/pt_truehd", true).toBool()) codecs << QStringLiteral("truehd");
        spdifCodecs_ = codecs.join(',');

        check_error(mpv_set_option_string(mpv_, "ao", "wasapi"));
        check_error(mpv_set_option_string(mpv_, "audio-device",
            savedDevice.toUtf8().constData()));
        check_error(mpv_set_option_string(mpv_, "audio-exclusive",
            savedExclusive ? "yes" : "no"));
        check_error(mpv_set_option_string(mpv_, "audio-spdif",
            passthroughEnabled_ ? spdifCodecs_.toUtf8().constData() : ""));
        qInfo() << "[MPV] WASAPI 초기 정책: device=" << savedDevice
                << "exclusive=" << savedExclusive
                << "passthrough=" << passthroughEnabled_
                << "codecs=" << spdifCodecs_;
    }

    // ── mpv_initialize ────────────────────────────────────────────
    int ret = mpv_initialize(mpv_);
    if (ret < 0) {
        qCritical() << "[MPV] mpv_initialize 실패:" << mpv_error_string(ret);
        return false;
    }

    // ══════════════════════════════════════════════════════════════════
    // 화질/음질 최적화 원칙 (이 주석을 절대 삭제하지 말 것)
    // 목표: 제작사가 의도한 색감과 음향을 100% 그대로 재현
    // 모든 설정은 이 목표를 위해 신중하게 선택되었음.
    // ══════════════════════════════════════════════════════════════════
    // ── VSync 제어 원칙 ─────────────────────────────────────────────────────────
    // Qt(QOpenGLWidget)이 swapInterval=1로 VSync를 제어함.
    // MPV는 opengl-swapinterval=0으로 자체 VSync를 비활성화.
    // 이중 VSync 충돌 시 4K HiDPI에서 프레임 타이밍 불규칙 발생.
    // → MPV VSync 비활성화, Qt만 VSync 사용 = 이중 VSync 제거.
    check_error(mpv_set_property_string(mpv_, "opengl-swapinterval", "0"));

    // ── D3D11 동기화 간격: 기본값 사용 (설정 제거) ────────────────────
    // mpv GitHub 이슈 #15196 (kasper93): d3d11-sync-interval 명시적 설정이 display sync를 깨뜨림
    // 해결책: 설정을 제거하고 MPV 기본값 사용 (MPV가 디스플레이 주사율에 맞춰 자동 조정)

    // ── 노트북 감지 및 display-resample 금지 ───────────────────────
    // mpv GitHub 이슈 #15196: Windows 11 24H2 + 최신 NVIDIA/AMD 드라이버에서
    // display-resample이 주기적 끔김을 유발함 (WDDM 3.2 호환성 문제)
    // 노트북 감지 시 (AC 연결 포함) video-sync=audio 강제
    // 데스크톱은 applyVideoSyncByFps()에서 자동 선택
    {
        // 전체 GPU/화면 탐지는 바로 아래에서 한 번만 수행한다. 여기서는
        // 시작 지연을 줄이기 위해 전원 API만 사용해 노트북 여부를 표시한다.
        isLaptop_ = RenderEnvironment::hasBattery();
        if (isLaptop_) {
            qInfo() << "[MPV] 노트북 감지 → isLaptop_=true (영상 로드 후 video-sync 자동 선택)";
        }
    }


    // ── 하드웨어 디코딩 ──────────────────────────────────────────────
    // auto-safe: NVIDIA/AMD/Intel GPU 자동 감지, 실패 시 소프트웨어로 폴백
    check_error(mpv_set_property_string(mpv_, "hwdec", "auto-safe"));

    // ── 프레임 드롭 설정 (노트북 GPU 끊김 방지) ──────────────────────────
    // vo: GPU가 디코딩보다 느릴 때 프레임 스킵 허용 → 끊김 대신 일부 프레임 스킵
    // 노트북 통합 GPU(Intel Iris/UHD)에서 가장 효과적
    check_error(mpv_set_property_string(mpv_, "framedrop", "vo"));
    // 디코딩 스레드: 0=자동 (코어 수 기준 자동 선택)
    check_error(mpv_set_property_string(mpv_, "vd-lavc-threads", "0"));

    // ── 비디오 동기화 ────────────────────────────────────────────────
    // audio: 초기값은 audio 모드 (안전한 기본값)
    // 파일 로드 후 applyVideoSyncByFps()가 FPS에 따라 자동 전환:
    //   - 60fps 이하: display-resample (저더 제거, 24fps 영화 최적)
    //   - 60fps 초과: audio (고프레임 게임 영상 안정성)
    // Qt swapInterval=1 + opengl-swapinterval=0 조합으로 이중 VSync 없음
    check_error(mpv_set_property_string(mpv_, "video-sync", "audio"));

    // ── 영상 크기/비율 ────────────────────────────────────────────────
    // keepaspect=yes: 비율 유지 (기본값)
    // keepaspect-window=no: FBO(위젯) 크기에 맞게 영상을 꽉 채움
    //   → 16:9 영상이 16:9 창에서 좌우 여백 없이 꽉 채워짐
    //   → 4:3 영상은 위아래 레터박스(정상 동작)
    check_error(mpv_set_property_string(mpv_, "keepaspect",        "yes"));
    check_error(mpv_set_property_string(mpv_, "keepaspect-window", "no"));

    // ══════════════════════════════════════════════════════════════════
    // 자동 적응형 렌더링 설정 (RenderEnvironment)
    // 실행 시점에 화면 해상도/DPI를 감지하여 최적 설정 자동 선택
    // 사용자 개입 없이 모든 PC에서 끊김 없이 동작하도록 설계됨
    //
    // 환경별 자동 선택 기준:
    //   FHD (≤1920×1080)  : ewa_lanczossharp + deband×2 (최고 화질)
    //   QHD (≤2560×1440)  : ewa_lanczossharp + deband×1
    //   4K  (≤3840×2160)  : spline36 + deband×1 (GTX 1070 등 4K 환경 최적)
    //   5K+ (>3840×2160)  : lanczos + deband OFF (안정성 최우선)
    // ══════════════════════════════════════════════════════════════════
    {
        RenderEnvInfo env = RenderEnvironment::detect();
        renderEnv_ = env;  // 나중에 참조할 수 있도록 저장

        // ── 업스케일링 알고리즘 (환경별 자동 선택) ───────────────
        check_error(mpv_set_property_string(mpv_, "scale",
            env.scaleAlgo.toUtf8().constData()));
        check_error(mpv_set_property_string(mpv_, "dscale",
            env.dscaleAlgo.toUtf8().constData()));
        check_error(mpv_set_property_string(mpv_, "cscale",
            env.cscaleAlgo.toUtf8().constData()));

        // ── 시그모이드 업스케일링 ─────────────────────────────────
        check_error(mpv_set_property_string(mpv_, "sigmoid-upscaling",
            env.sigUpscaling ? "yes" : "no"));

        // ── 링잉 아티팩트 억제 (antiring) ────────────────────────
        // GPU 부하 없이 고대비 경계 흰 테두리 현상 제거
        // 5K+에서는 비활성화 (렌더링 파이프라인 단순화)
        const char* antiringVal = (env.pixelLoad == RenderEnvInfo::PixelLoad::Ultra)
                                  ? "0.0" : "0.7";
        check_error(mpv_set_property_string(mpv_, "scale-antiring",  antiringVal));
        check_error(mpv_set_property_string(mpv_, "cscale-antiring", antiringVal));

        // ── HDR 톤매핑 ────────────────────────────────────────────
        // HDR 모니터 감지 시 tone-mapping=auto (패스스루), SDR 모니터는 bt.2446a
        const bool hdrDisplay = env.hdrEnabled;
        const char* toneMapping = hdrDisplay ? "auto" : "bt.2446a";
        check_error(mpv_set_property_string(mpv_, "tone-mapping", toneMapping));
        qInfo() << "[MPV] HDR 디스플레이:" << (hdrDisplay ? "ON → tone-mapping=auto" : "OFF → tone-mapping=bt.2446a");
        // hdr-compute-peak: 동적 밝기 측정 (4K 이상에서는 부하로 비활성화)
        check_error(mpv_set_property_string(mpv_, "hdr-compute-peak",
            env.hdrComputePeak ? "yes" : "no"));

        // ── 색상 관리 ─────────────────────────────────────────────
        // target-colorspace-hint: 모든 환경에서 활성화 (부하 없음)
        check_error(mpv_set_property_string(mpv_, "target-colorspace-hint", "yes"));
        // Dolby Vision 동적 메타데이터: Windows 11 + D3D11 환경에서 활성화
        // vo=gpu-next + d3d11 조합 시 Dolby Vision Profile 5/8 지원
        // 구형 Intel GPU 제외 (드라이버 호환성 문제)
        if (env.gpuVendor != GpuVendor::IntelHD &&
            env.gpuVendor != GpuVendor::IntelIris) {
            check_error(mpv_set_property_string(mpv_, "tone-mapping-mode", "auto"));
        }
        // HDR 디스플레이에서 target-peak 자동 설정 (디스플레이 최대 밝기 활용)
        if (hdrDisplay) {
            check_error(mpv_set_property_string(mpv_, "target-peak", "auto"));
        }
        // ICC 프로파일 자동 적용 (캘리브레이션 모니터 지원, 일반 모니터 무영향)
        check_error(mpv_set_property_string(mpv_, "icc-profile-auto", "yes"));
        check_error(mpv_set_property_string(mpv_, "icc-intent", "relative-colorimetric"));
        // 선형 광학 연산 (환경별 자동 선택)
        check_error(mpv_set_property_string(mpv_, "linear-upscaling",
            env.linearUpscaling ? "yes" : "no"));
        check_error(mpv_set_property_string(mpv_, "correct-downscaling",
            env.correctDownscaling ? "yes" : "no"));

        // ── 디밴딩 (환경별 자동 선택) ────────────────────────────
        check_error(mpv_set_property_string(mpv_, "deband",
            env.debandEnabled ? "yes" : "no"));
        if (env.debandEnabled) {
            check_error(mpv_set_property_string(mpv_, "deband-iterations",
                QByteArray::number(env.debandIterations).constData()));
            check_error(mpv_set_property_string(mpv_, "deband-threshold", "48"));
            check_error(mpv_set_property_string(mpv_, "deband-range",     "16"));
            check_error(mpv_set_property_string(mpv_, "deband-grain",     "24"));
        }

        // ── 디더링 ────────────────────────────────────────────────
        check_error(mpv_set_property_string(mpv_, "dither-depth", "auto"));
        check_error(mpv_set_property_string(mpv_, "dither",
            env.ditherMode.toUtf8().constData()));
    }

    // 오디오 초기화 실패 시 영상은 계속 재생 (null 오디오 폴백)
    check_error(mpv_set_property_string(mpv_, "audio-fallback-to-null", "yes"));
    // HDMI 멀티채널 정책: mpv가 오디오 원본 레이아웃을 우선해 현재 WASAPI
    // 엔드포인트와 협상하도록 auto를 사용한다. 고정 레이아웃 목록은 장치 재초기화
    // 뒤 Windows 오디오 API가 2.0을 선택하는 사례가 있어 사용하지 않는다.
    // 7.1 리시버에서는 7.1 원본은 7.1, 5.1 원본은 5.1, 스테레오 원본은
    // stereo로 유지되어 원본 채널을 임의 확장하지 않는다.
    check_error(mpv_set_property_string(mpv_, "audio-channels", "auto"));
    // AAC/AC-3/DTS 디코더의 선행 다운믹스를 차단하고 항상 원본 레이아웃을
    // mpv/WASAPI 협상 단계로 전달한다. PCM 2.0 폴백 시에도 LFE 혼합 차이를
    // 만드는 코덱별 다운믹스 경로를 사용하지 않는다.
    check_error(mpv_set_property_string(mpv_, "ad-lavc-downmix", "no"));
    // mpv 기본값(no)을 유지한다. 이 옵션을 강제로 yes로 바꾸면 실제 stereo
    // 폴백 경로에서 레벨 행렬이 달라져 리시버의 저음 관리와 겹칠 수 있다.
    // 정상 5.1/7.1·비트스트림 정책에는 영향을 주지 않는다.
    check_error(mpv_set_property_string(mpv_, "audio-normalize-downmix", "no"));

    // 자막 우선순위: 언어 메타데이터가 있는 경우 한국어를 먼저 선택한다.
    // 메타데이터가 없는 외부 SMI/SRT는 sub-auto=fuzzy가 파일명 기준으로 탐색한다.
    check_error(mpv_set_property_string(mpv_, "slang", "ko,kor,korean"));
    check_error(mpv_set_property_string(mpv_, "sub-auto", "fuzzy"));
    // 설정 화면에서 저장한 자막 스타일을 첫 재생 전에도 적용한다.
    applyStoredSubtitleStyle();

    // 오디오 필터 초기화
    check_error(mpv_set_property_string(mpv_, "af", ""));

    // 네트워크/NAS 드라이브 지원
    check_error(mpv_set_property_string(mpv_, "demuxer-readahead-secs", "2"));
    check_error(mpv_set_property_string(mpv_, "cache",        "yes"));
    check_error(mpv_set_property_string(mpv_, "cache-secs",   "10"));

    // ── yt-dlp 연동 (유튜브 5.1 서라운드 지원) ────────────────────
    // ytdl=yes: MPV가 yt-dlp를 통해 스트리밍 URL 처리
    check_error(mpv_set_property_string(mpv_, "ytdl", "yes"));
    // 최고 품질 비디오 + 최고 품질 오디오 (5.1 E-AC3 포함)
    // bestaudio: 유튜브 5.1 Dolby Digital+ (E-AC3) 스트림 자동 선택
    check_error(mpv_set_property_string(mpv_, "ytdl-format",
        "bestvideo[ext=mp4]+bestaudio[ext=m4a]/bestvideo+bestaudio/best"));
    // 스트리밍 버퍼 증가 (유튜브 고품질 스트림)
    check_error(mpv_set_property_string(mpv_, "demuxer-max-bytes",       "150MiB"));
    check_error(mpv_set_property_string(mpv_, "demuxer-max-back-bytes",  "75MiB"));

    // 관찰할 속성 등록
    mpv_observe_property(mpv_, 0, "time-pos",        MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv_, 0, "duration",        MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv_, 0, "pause",           MPV_FORMAT_FLAG);
    mpv_observe_property(mpv_, 0, "volume",          MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv_, 0, "mute",            MPV_FORMAT_FLAG);
    mpv_observe_property(mpv_, 0, "track-list",      MPV_FORMAT_NODE);
    mpv_observe_property(mpv_, 0, "audio-codec-name",MPV_FORMAT_STRING);
    mpv_observe_property(mpv_, 0, "audio-channels",  MPV_FORMAT_STRING);
    mpv_observe_property(mpv_, 0, "audio-samplerate",MPV_FORMAT_INT64);
    mpv_observe_property(mpv_, 0, "audio-out-params", MPV_FORMAT_NODE);
    mpv_observe_property(mpv_, 0, "audio-out-detected-device", MPV_FORMAT_STRING);
    mpv_observe_property(mpv_, 0, "video-params/w",  MPV_FORMAT_INT64);
    mpv_observe_property(mpv_, 0, "video-params/h",  MPV_FORMAT_INT64);
    mpv_observe_property(mpv_, 0, "container-fps",   MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv_, 0, "video-codec",     MPV_FORMAT_STRING);

    // wakeup callback은 mpv 내부 스레드에서 호출될 수 있으므로 Qt 메인 루프로
    // 큐잉한다. 이벤트가 도착할 때만 mpv_wait_event(..., 0)을 비워낸다.
    mpv_set_wakeup_callback(mpv_, wakeupCallback, this);

    initialized_ = true;
    qInfo() << "[MPV] 초기화 완료 WID:" << windowId;
    return true;
}

void MpvCore::wakeupCallback(void* ctx) {
    MpvCore* self = static_cast<MpvCore*>(ctx);
    // Qt 메인 스레드에서 즉시 이벤트 처리
    QTimer::singleShot(0, self, &MpvCore::onMpvEvents);
}

void MpvCore::onMpvEvents() {
    while (mpv_) {
        mpv_event* event = mpv_wait_event(mpv_, 0);
        if (!event || event->event_id == MPV_EVENT_NONE) break;
        handleEvent(event);
    }
}

void MpvCore::handleEvent(mpv_event* event) {
    switch (event->event_id) {
    case MPV_EVENT_START_FILE:
        appLog("EVENT START_FILE");
        break;

    case MPV_EVENT_FILE_LOADED: {
        char* rawPath = mpv_get_property_string(mpv_, "path");
        QString path = rawPath ? QString::fromUtf8(rawPath) : QString();
        mpv_free(rawPath);
        qInfo() << "[MPV] FILE_LOADED:" << path;
        appLog(QString("EVENT FILE_LOADED: %1").arg(path));
        // 파일별 MPV 옵션이 새 세션에서 초기화될 수 있으므로 저장된 자막 스타일을
        // 즉시와 트랙 준비 뒤에 한 번 더 적용한다.
        applyStoredSubtitleStyle();
        QTimer::singleShot(150, this, [this]() {
            if (mpv_ && initialized_)
                applyStoredSubtitleStyle();
        });
        // pause 해제: 동기 + 비동기 두 번 실행으로 확실히 재생 시작
        int pauseFlag = 0;
        mpv_set_property(mpv_, "pause", MPV_FORMAT_FLAG, &pauseFlag);  // 동기
        const char* pauseArgs[] = { "set", "pause", "no", nullptr };
        mpv_command_async(mpv_, 0, pauseArgs);  // 비동기 백업
        emit fileLoaded(path);
        // 파일 로드 후 200ms 뒤 주요 속성 강제 갱신
        // (observe 이벤트가 누락될 수 있으므로 직접 조회)
        QTimer::singleShot(300, this, [this, path]() {
            if (!mpv_ || !initialized_) return;
            // duration
            double dur = 0;
            if (mpv_get_property(mpv_, "duration", MPV_FORMAT_DOUBLE, &dur) == 0 && dur > 0)
                emit durationChanged(dur);
            // 오디오 포맷
            char* codec    = mpv_get_property_string(mpv_, "audio-codec-name");
            char* channels = mpv_get_property_string(mpv_, "audio-channels");
            int64_t sr = 0;
            mpv_get_property(mpv_, "audio-samplerate", MPV_FORMAT_INT64, &sr);
            if (codec) {
                QString codecStr   = QString::fromUtf8(codec);
                QString channelStr = channels ? QString::fromUtf8(channels) : "";
                mpv_free(codec); mpv_free(channels);
                int ch = channelCountFromLayout(channelStr);
                int outputRate = static_cast<int>(sr);
                const QString outStr = actualAudioOutput(mpv_, ch, outputRate);
                emit audioFormatChanged(codecStr, ch, outputRate, outStr);
            } else {
                mpv_free(channels);
            }
            // 트랙 목록
            emit tracksChanged();
            qInfo() << "[MPV] 속성 강제 갱신 완료 - duration:" << dur;
        });
        // ── 재생 워치독 (추가 안전장치) ────────────────────────────
        // 파일 로드 2초 후에도 time-pos가 진행되지 않으면 (pause=no임에도)
        // AO 멈춤 상태로 판단하고 spdif 해제 + ao-reload로 강제 복구.
        QTimer::singleShot(2000, this, [this]() {
            if (!mpv_ || !initialized_) return;
            int paused = 1;
            mpv_get_property(mpv_, "pause", MPV_FORMAT_FLAG, &paused);
            double pos = -1;
            mpv_get_property(mpv_, "time-pos", MPV_FORMAT_DOUBLE, &pos);
            int coreIdle = 0;
            mpv_get_property(mpv_, "core-idle", MPV_FORMAT_FLAG, &coreIdle);
            appLog(QString("워치독: pause=%1 time-pos=%2 core-idle=%3")
                       .arg(paused).arg(pos).arg(coreIdle));
            // pause 아닌데 재생이 멈춤(core-idle) + 위치 0 부근 → AO 멈춤.
            // HDMI/AVR에서는 DD+ 비트스트림을 해제하지 않고 현 정책 그대로 재협상한다.
            // 비-HDMI 장치에서만 PCM 복구를 위해 passthrough를 해제한다.
            if (!paused && coreIdle && pos < 0.5) {
                const bool passthroughCapable = deviceLikelySupportsPassthrough();
                appLog(passthroughCapable
                    ? "워치독: HDMI/AVR 추정 → SPDIF 유지 + ao-reload 복구"
                    : "워치독: 비패스스루 장치 추정 → SPDIF 해제 + ao-reload 복구");
                qWarning() << "[MPV] 워치독: 재생 멈춤 →"
                           << (passthroughCapable ? "비트스트림 유지" : "PCM 복구")
                           << "+ ao-reload";
                if (!passthroughCapable)
                    mpv_set_property_string(mpv_, "audio-spdif", "");
                const char* reloadArgs[] = { "ao-reload", nullptr };
                mpv_command_async(mpv_, 0, reloadArgs);
                int pf = 0;
                mpv_set_property_async(mpv_, 0, "pause", MPV_FORMAT_FLAG, &pf);
            }
        });
        break;
    }

    case MPV_EVENT_PLAYBACK_RESTART:
        qInfo() << "[MPV] PLAYBACK_RESTART - 재생 시작";
        appLog("EVENT PLAYBACK_RESTART");
        emit playbackStarted();
        // 재생 시작 시 프레임 드롭 모니터링 타이머 시작
        if (frameDropTimer_) {
            prevFrameDropCount_ = 0;
            dropEventCount_     = 0;
            normalEventCount_   = 0;
            qualityDegraded_    = false;
            frameDropTimer_->start();
        }
        break;

    case MPV_EVENT_END_FILE: {
        auto* ef = reinterpret_cast<mpv_event_end_file*>(event->data);
        // 재생 종료 시 프레임 드롭 모니터링 타이머 정지 + 카운터 초기화
        if (frameDropTimer_ && frameDropTimer_->isActive()) {
            frameDropTimer_->stop();
            qInfo() << "[MPV] 재생 종료 → frameDropTimer 정지";
        }
        prevFrameDropCount_ = 0;
        dropEventCount_     = 0;
        normalEventCount_   = 0;
        qualityDegraded_    = false;
        if (ef->reason == MPV_END_FILE_REASON_EOF) {
            emit playbackEnded();
        } else if (ef->reason == MPV_END_FILE_REASON_ERROR) {
            QString errMsg = QString::fromUtf8(mpv_error_string(ef->error));
            qWarning() << "[MPV] END_FILE error:" << errMsg;
            // WASAPI Exclusive 실패 시 자동 폴백: Exclusive 해제 후 현재 파일 재로드
            char* aoVal = mpv_get_property_string(mpv_, "audio-exclusive");
            bool wasExclusive = aoVal && QString::fromUtf8(aoVal) == "yes";
            mpv_free(aoVal);
            if (wasExclusive) {
                qWarning() << "[MPV] WASAPI Exclusive 실패 → 공유 모드로 자동 전환";
                mpv_set_property_string(mpv_, "audio-exclusive", "no");
                // 현재 파일 경로 저장 후 재로드
                char* currentPath = mpv_get_property_string(mpv_, "path");
                if (currentPath) {
                    QString path = QString::fromUtf8(currentPath);
                    mpv_free(currentPath);
                    QTimer::singleShot(100, this, [this, path]() {
                        if (!initialized_) return;
                        const QByteArray pathBytes = path.toUtf8();
                        const char* args[] = { "loadfile", pathBytes.constData(), nullptr };
                        mpv_command_async(mpv_, 0, args);
                    });
                    emit errorOccurred("WASAPI Exclusive 실패 - 공유 모드로 자동 전환");
                } else {
                    emit errorOccurred(errMsg);
                }
            } else {
                emit errorOccurred(errMsg);
            }
        }
        break;
    }

    case MPV_EVENT_SEEK: {
        // seek 직후 MPV가 프레임을 대량 드롭하는 것을 오탐 방지
        // prevFrameDropCount_를 현재 값으로 리셋 → seek 전후 드롭 비교 초기화
        if (initialized_) {
            int64_t dropCount = 0;
            mpv_get_property(mpv_, "frame-drop-count", MPV_FORMAT_INT64, &dropCount);
            prevFrameDropCount_ = static_cast<int>(dropCount);
            dropEventCount_ = 0;  // seek 후 드롭 카운터 리셋 (오탐 방지)
            qInfo() << "[MPV] seek 감지 → 프레임 드롭 카운터 리셋";
        }
        break;
    }

    case MPV_EVENT_PROPERTY_CHANGE:
        handlePropertyChange(reinterpret_cast<mpv_event_property*>(event->data));
        break;

    case MPV_EVENT_LOG_MESSAGE: {
        auto* msg = reinterpret_cast<mpv_event_log_message*>(event->data);
        if (msg->log_level <= MPV_LOG_LEVEL_WARN)
            qWarning() << "[MPV]" << msg->prefix << msg->text;

        // ── 근본 수정: 오디오 드라이버 초기화 실패 자동 복구 ──────────
        // 패스스루(spdif) 미지원 장치(USB DAC 등)에서 WASAPI Exclusive
        // + spdif 조합이 실패하면 MPV가 "Falling back to PCM output"만
        // 선언하고 AO 재초기화를 하지 않고 멈춤 (video-sync=audio일 때
        // 오디오 클럭이 없어 영상도 첫 프레임에서 정지 = 자동재생 안됨 버그).
        // → AO 실패 로그 감지 시 spdif 해제 후 ao-reload로 즉시 복구.
        //   패스스루 지원 장치(AV리시버)는 이 경로를 타지 않으므로
        //   비트스트림 패스스루/음질에 영향 없음.
        if (msg->log_level <= MPV_LOG_LEVEL_ERROR &&
            strstr(msg->text, "Failed to initialize audio driver")) {
            const bool passthroughCapable = deviceLikelySupportsPassthrough();
            appLog(passthroughCapable
                ? "AO 초기화 실패 감지 → HDMI/AVR SPDIF 유지 복구"
                : "AO 초기화 실패 감지 → 비패스스루 PCM 복구");
            qWarning() << "[MPV] AO 실패 감지 →"
                       << (passthroughCapable ? "SPDIF 유지" : "SPDIF 해제")
                       << "후 ao-reload";
            // HDMI/AVR 후보에서 오류가 났다고 DD+/DTS 정책을 즉시 지우지 않는다.
            // 해당 장치가 아닌 경우에만 PCM 디코딩 폴백을 허용한다.
            if (!passthroughCapable)
                mpv_set_property_string(mpv_, "audio-spdif", "");
            // Exclusive 실패 가능성 대비: 재시도는 Exclusive 유지,
            // ao-reload로 AO 재초기화 (장치가 Exclusive PCM은 지원)
            const char* reloadArgs[] = { "ao-reload", nullptr };
            mpv_command_async(mpv_, 0, reloadArgs);
            // 3) 재생 상태 보장
            int pf = 0;
            mpv_set_property_async(mpv_, 0, "pause", MPV_FORMAT_FLAG, &pf);
        }
        break;
    }

    default:
        break;
    }
}

void MpvCore::handlePropertyChange(mpv_event_property* prop) {
    QString name = QString::fromUtf8(prop->name);

    if (name == "time-pos" && prop->format == MPV_FORMAT_DOUBLE) {
        static int tpLogCount = 0;
        if (tpLogCount < 5) { appLog(QString("PROP time-pos=%1").arg(*reinterpret_cast<double*>(prop->data))); tpLogCount++; }
        emit positionChanged(*reinterpret_cast<double*>(prop->data));
    }
    else if (name == "duration" && prop->format == MPV_FORMAT_DOUBLE) {
        emit durationChanged(*reinterpret_cast<double*>(prop->data));
    }
    else if (name == "pause" && prop->format == MPV_FORMAT_FLAG) {
        bool paused = *reinterpret_cast<int*>(prop->data);
        appLog(QString("PROP pause=%1").arg(paused ? "yes" : "no"));
        if (paused) emit playbackPaused();
        else        emit playbackStarted();
    }
    else if (name == "volume" && prop->format == MPV_FORMAT_DOUBLE) {
        emit volumeChanged(static_cast<int>(*reinterpret_cast<double*>(prop->data)));
    }
    else if (name == "track-list") {
        emit tracksChanged();
    }
    else if (name == "audio-codec-name" || name == "audio-channels" ||
             name == "audio-samplerate" || name == "audio-out-params") {
        // 오디오 포맷 정보 업데이트
        char* codec = mpv_get_property_string(mpv_, "audio-codec-name");
        char* channels = mpv_get_property_string(mpv_, "audio-channels");
        int64_t samplerate = 0;
        mpv_get_property(mpv_, "audio-samplerate", MPV_FORMAT_INT64, &samplerate);

        QString codecStr   = codec    ? QString::fromUtf8(codec)    : "";
        QString channelStr = channels ? QString::fromUtf8(channels) : "";
        mpv_free(codec);
        mpv_free(channels);

        // 원본 채널 수는 폴백으로만 사용하고, UI에는 audio-out-params의 실제 출력 채널을 우선 표시한다.
        int channelCount = channelCountFromLayout(channelStr);
        int outputRate = static_cast<int>(samplerate);
        QString outputStr = actualAudioOutput(mpv_, channelCount, outputRate);
        if (outputStr.isEmpty()) {
            char* ao = mpv_get_property_string(mpv_, "current-ao");
            outputStr = ao ? QString::fromUtf8(ao) : QStringLiteral("출력 협상 중");
            mpv_free(ao);
        }

        appLog(QString("AUDIO actual-output=%1 channels=%2 rate=%3 source-layout=%4")
                   .arg(outputStr).arg(channelCount).arg(outputRate).arg(channelStr));
        emit audioFormatChanged(codecStr, channelCount, outputRate, outputStr);
    }
    else if (name == "video-params/w" || name == "video-params/h" ||
             name == "container-fps" || name == "video-codec") {
        // 비디오 정보 업데이트
        int64_t w = 0, h = 0;
        mpv_get_property(mpv_, "video-params/w", MPV_FORMAT_INT64, &w);
        mpv_get_property(mpv_, "video-params/h", MPV_FORMAT_INT64, &h);
        double fps = 0;
        mpv_get_property(mpv_, "container-fps", MPV_FORMAT_DOUBLE, &fps);
        char* vcodec = mpv_get_property_string(mpv_, "video-codec");
        QString vcodecStr = vcodec ? QString::fromUtf8(vcodec) : "";
        mpv_free(vcodec);
        if (w > 0 && h > 0) {
            emit videoInfoChanged(static_cast<int>(w), static_cast<int>(h), fps, vcodecStr);

            // FPS + 모니터 주사율 기반 정밀 video-sync 자동 전환
            if (fps > 0) applyVideoSyncByFps(fps);

            // 영상 해상도 기반 업스케일 알고리즘 최적화
            // (4K 영상을 FHD 화면에서 재생 시 불필요한 업스케일 제거 등)
            optimizeScaleForContent(static_cast<int>(w), static_cast<int>(h));

            // 코덱별 최적 hwdec 자동 선택
            // (AV1: 구형 GPU에서 소프트웨어, MPEG-2: dxva2 등)
            if (!vcodecStr.isEmpty()) applyOptimalHwdec(vcodecStr);
        }
    }
}

// ─── 재생 제어 ────────────────────────────────────────────────────
QVariantList MpvCore::audioDeviceList() const {
    QVariantList result;
    if (!initialized_) return result;
    QVariant list = getProperty("audio-device-list");
    for (const QVariant& item : list.toList()) {
        QVariantMap m = item.toMap();
        if (!m.isEmpty()) result << m;  // {name, description}
    }
    return result;
}

// 현재 장치가 패스스루(비트스트림)를 지원할 가능성 판단 (사전 감지)
// HDMI/SPDIF/리시버 계열이면 패스스루 유지, USB DAC/스피커 등은 해제
bool MpvCore::deviceLikelySupportsPassthrough() const {
    if (!initialized_) return true;  // 불확실하면 기존 동작 유지 (실패 시 자동복구 있음)
    QString selected;
    {
        char* dev = mpv_get_property_string(mpv_, "audio-device");
        selected = dev ? QString::fromUtf8(dev) : QStringLiteral("auto");
        mpv_free(dev);
    }
    // audio-device=auto인 경우 목록의 첫 번째 장치를 추정하면 노트북 스피커를
    // 잘못 선택할 수 있다. 실제 AO가 선택한 endpoint를 우선 사용해야 HDMI AVR의
    // 비트스트림 복구가 PCM 폴백으로 바뀌지 않는다.
    QString detected;
    char* detectedRaw = mpv_get_property_string(mpv_, "audio-out-detected-device");
    if (detectedRaw) {
        detected = QString::fromUtf8(detectedRaw);
        mpv_free(detectedRaw);
    }
    const QString effectiveDevice = selected != "auto" ? selected : detected;
    QString desc;
    const QVariantList devices = audioDeviceList();
    if (!effectiveDevice.isEmpty()) {
        for (const QVariant& item : devices) {
            QVariantMap m = item.toMap();
            if (m.value("name").toString() == effectiveDevice) {
                desc = m.value("description").toString();
                break;
            }
        }
    }
    if (desc.isEmpty()) return true;  // 판단 불가 → 기존 동작
    const QString d = desc.toLower();
    static const QStringList ptHints = {
        "hdmi", "digital", "spdif", "s/pdif", "optical", "receiver",
        "avr", "denon", "yamaha", "onkyo", "marantz", "pioneer",
        "nvidia high definition", "amd high definition", "intel display"
    };
    for (const QString& h : ptHints)
        if (d.contains(h)) return true;
    qInfo() << "[MPV] 패스스루 미지원 추정 장치:" << desc;
    return false;
}

void MpvCore::loadFile(const QString& path, bool append) {
    if (!initialized_) {
        qWarning() << "[MPV] loadFile 호출되었지만 초기화 안됨!";
        return;
    }
    qInfo() << "[MPV] loadFile:" << path << "| mode:" << (append ? "append" : "replace");
    appLog(QString("loadFile: %1 mode=%2").arg(path, append ? "append" : "replace"));
    // ── 오디오 장치 사전 감지 ────────────────────────────────────────
    // 패스스루 미지원 장치면 미리 spdif 해제 → 실패-복구 사이클 없이
    // 처음부터 올바른 모드로 시작 (첫 소리까지 시간 단축).
    // 패스스루 지원 장치(HDMI/리시버)는 spdif 유지 → 비트스트림 그대로.
    // 판단 실패 시에도 AO 실패 자동복구(ao-reload)가 백업으로 동작.
    // audio-spdif는 initialize()에서 한 번만 설정
    // loadFile()에서 재설정하면 장치 감지 실패 시 DD+/DTS 패스스루가 차단됨
    // 패스스루 실패 시 워치독가 ao-reload로 자동복구
    // loadfile 전에 pause=no 먼저 설정 → keep-open=yes 환경에서
    // 새 파일 로드 시 이전 pause 상태를 유지하지 않고 즐시 재생
    int pauseFlag = 0;
    mpv_set_property(mpv_, "pause", MPV_FORMAT_FLAG, &pauseFlag);
    const char* mode = append ? "append-play" : "replace";
    QByteArray pathBytes = path.toUtf8();
    const char* args[] = { "loadfile", pathBytes.constData(), mode, nullptr };
    int ret = mpv_command_async(mpv_, 0, args);
    qInfo() << "[MPV] mpv_command_async result:" << ret;
}

void MpvCore::play() {
    if (!initialized_) return;
    int flag = 0;
    mpv_set_property_async(mpv_, 0, "pause", MPV_FORMAT_FLAG, &flag);
}

void MpvCore::pause() {
    if (!initialized_) return;
    int flag = 1;
    mpv_set_property_async(mpv_, 0, "pause", MPV_FORMAT_FLAG, &flag);
}

void MpvCore::togglePause() {
    if (!initialized_) return;
    const char* args[] = { "cycle", "pause", nullptr };
    mpv_command_async(mpv_, 0, args);
}

void MpvCore::stop() {
    if (!initialized_) return;
    const char* args[] = { "stop", nullptr };
    mpv_command_async(mpv_, 0, args);
    emit playbackStopped();
}

void MpvCore::seek(double seconds, bool relative) {
    if (!initialized_) return;
    QString mode = relative ? "relative" : "absolute";
    QByteArray secsStr = QString::number(seconds, 'f', 3).toUtf8();
    QByteArray modeStr = mode.toUtf8();
    const char* args[] = { "seek", secsStr.constData(), modeStr.constData(), nullptr };
    mpv_command_async(mpv_, 0, args);
}

void MpvCore::seekPercent(double percent) {
    if (!initialized_) return;
    QByteArray pctStr = QString::number(percent, 'f', 3).toUtf8();
    const char* args[] = { "seek", pctStr.constData(), "absolute-percent", nullptr };
    mpv_command_async(mpv_, 0, args);
}

// ─── 속성 설정 ────────────────────────────────────────────────────
void MpvCore::setVolume(int vol) {
    if (!initialized_) return;
    double v = vol;
    mpv_set_property_async(mpv_, 0, "volume", MPV_FORMAT_DOUBLE, &v);
}

void MpvCore::setMuted(bool muted) {
    if (!initialized_) return;
    int flag = muted ? 1 : 0;
    mpv_set_property_async(mpv_, 0, "mute", MPV_FORMAT_FLAG, &flag);
}

void MpvCore::setSpeed(double speed) {
    if (!initialized_) return;
    mpv_set_property_async(mpv_, 0, "speed", MPV_FORMAT_DOUBLE, &speed);
}

void MpvCore::applyStoredSubtitleStyle() {
    if (!mpv_) return;

    QSettings settings("Sorinuri", "SorinuriPlayer");
    const QString font = settings.value("subtitle/font", "맑은 고딕").toString();
    const int size = qBound(12, settings.value("subtitle/size", 36).toInt(), 96);
    const bool bold = settings.value("subtitle/bold", false).toBool();
    const bool shadow = settings.value("subtitle/shadow", true).toBool();
    const int colorIndex = settings.value("subtitle/color", 0).toInt();
    static const QStringList colors = {
        QStringLiteral("#FFFFFFFF"), QStringLiteral("#FFFFFF00"),
        QStringLiteral("#FF87CEEB"), QStringLiteral("#FF90EE90"),
    };

    check_error(mpv_set_property_string(mpv_, "sub-font", font.toUtf8().constData()));
    check_error(mpv_set_property_string(mpv_, "sub-font-size", QByteArray::number(size).constData()));
    check_error(mpv_set_property_string(mpv_, "sub-bold", bold ? "yes" : "no"));
    check_error(mpv_set_property_string(mpv_, "sub-shadow-offset", shadow ? "2" : "0"));
    const QString color = colors.value(colorIndex, colors.first());
    check_error(mpv_set_property_string(mpv_, "sub-color", color.toUtf8().constData()));
    check_error(mpv_set_property_string(mpv_, "sub-auto",
        settings.value("subtitle/auto_load", true).toBool() ? "fuzzy" : "no"));
    appLog(QString("자막 스타일 적용: font=%1 size=%2").arg(font).arg(size));
}

void MpvCore::restoreAudioOutputAfterDeviceChange() {
    if (!initialized_) return;

    QSettings settings("Sorinuri", "SorinuriPlayer");
    const QString device = settings.value("audio/device", "auto").toString();
    const bool exclusive = settings.value("audio/exclusive", true).toBool();
    const bool passthrough = settings.value("audio/passthrough", true).toBool();
    QStringList codecs;
    if (settings.value("audio/pt_ac3", true).toBool()) codecs << "ac3";
    if (settings.value("audio/pt_eac3", true).toBool()) codecs << "eac3";
    if (settings.value("audio/pt_dts", true).toBool()) codecs << "dts";
    if (settings.value("audio/pt_dtshd", true).toBool()) codecs << "dts-hd";
    if (settings.value("audio/pt_truehd", true).toBool()) codecs << "truehd";

    // ao-reload는 실험적 명령이므로, 호출 전 현재 사용자가 선택한 장치·독점 모드·
    // 원본 채널 협상·패스스루 정책을 먼저 모두 복원한다.
    mpv_set_property_string(mpv_, "audio-device", device.toUtf8().constData());
    mpv_set_property_string(mpv_, "audio-exclusive", exclusive ? "yes" : "no");
    mpv_set_property_string(mpv_, "audio-channels", "auto");
    mpv_set_property_string(mpv_, "ad-lavc-downmix", "no");
    // 초기 정책과 동일하게 mpv 기본 다운믹스 정규화(no)를 유지한다.
    mpv_set_property_string(mpv_, "audio-normalize-downmix", "no");
    spdifCodecs_ = codecs.join(',');
    passthroughEnabled_ = passthrough;
    mpv_set_property_string(mpv_, "audio-spdif",
        passthrough ? spdifCodecs_.toUtf8().constData() : "");

    const char* reloadArgs[] = { "ao-reload", nullptr };
    mpv_command_async(mpv_, 0, reloadArgs);
    appLog(QString("오디오 출력 복구: device=%1 exclusive=%2 channels=auto passthrough=%3 codecs=%4")
               .arg(device).arg(exclusive).arg(passthrough).arg(spdifCodecs_));
    qInfo() << "[MPV] 절전/장치 변경 후 오디오 정책 재협상:"
            << device << "exclusive=" << exclusive << "passthrough=" << passthrough;
}

void MpvCore::setAudioDevice(const QString& device) {
    if (!initialized_) return;
    mpv_set_property_string(mpv_, "audio-device", device.toUtf8().constData());
}

void MpvCore::setAudioExclusive(bool exclusive) {
    if (!initialized_) return;
    mpv_set_property_string(mpv_, "audio-exclusive",
        exclusive ? "yes" : "no");
}

void MpvCore::setAudioPassthrough(bool passthrough) {
    passthroughEnabled_ = passthrough;
    if (!initialized_) return;
    if (passthrough) {
        mpv_set_property_string(mpv_, "audio-spdif",
            spdifCodecs_.toUtf8().constData());
    } else {
        mpv_set_property_string(mpv_, "audio-spdif", "");
    }
}

void MpvCore::setSpdifCodecs(const QStringList& codecs) {
    spdifCodecs_ = codecs.join(',');
    if (!initialized_) return;
    mpv_set_property_string(mpv_, "audio-spdif",
        spdifCodecs_.toUtf8().constData());
}

void MpvCore::setHwdec(const QString& method) {
    if (!initialized_) return;
    mpv_set_property_string(mpv_, "hwdec", method.toUtf8().constData());
}

void MpvCore::setVideoOutput(const QString& vo) {
    Q_UNUSED(vo)
    // libmpv 렌더 컨텍스트는 initialize()에서 vo=libmpv로 한 번만 구성한다.
    // 재생 중 vo를 gpu/gpu-next 등으로 바꾸면 MPV가 자체 비디오 창을 생성한다.
    qWarning() << "[MPV] 런타임 vo 변경 차단: libmpv 렌더 컨텍스트는 고정";
}

void MpvCore::setSubtitleTrack(int id) {
    if (!initialized_) return;
    int64_t v = id;
    mpv_set_property_async(mpv_, 0, "sid", MPV_FORMAT_INT64, &v);
}

void MpvCore::setAudioTrack(int id) {
    if (!initialized_) return;
    int64_t v = id;
    mpv_set_property_async(mpv_, 0, "aid", MPV_FORMAT_INT64, &v);
}

// ─── 상태 조회 ────────────────────────────────────────────────────
double MpvCore::duration() const {
    if (!initialized_) return 0;
    double v = 0;
    mpv_get_property(mpv_, "duration", MPV_FORMAT_DOUBLE, &v);
    return v;
}

double MpvCore::position() const {
    if (!initialized_) return 0;
    double v = 0;
    mpv_get_property(mpv_, "time-pos", MPV_FORMAT_DOUBLE, &v);
    return v;
}

int MpvCore::volume() const {
    if (!initialized_) return 100;
    double v = 100;
    mpv_get_property(mpv_, "volume", MPV_FORMAT_DOUBLE, &v);
    return static_cast<int>(v);
}

bool MpvCore::isPaused() const {
    if (!initialized_) return true;
    int flag = 1;
    mpv_get_property(mpv_, "pause", MPV_FORMAT_FLAG, &flag);
    return flag != 0;
}

bool MpvCore::isMuted() const {
    if (!initialized_) return false;
    int flag = 0;
    mpv_get_property(mpv_, "mute", MPV_FORMAT_FLAG, &flag);
    return flag != 0;
}

QString MpvCore::currentFile() const {
    if (!initialized_) return {};
    char* path = mpv_get_property_string(mpv_, "path");
    if (!path) return {};
    QString result = QString::fromUtf8(path);
    mpv_free(path);
    return result;
}

void MpvCore::setProperty(const QString& name, const QVariant& value) {
    if (!initialized_) return;

    // libmpv 렌더 컨텍스트가 생성된 뒤 출력 백엔드/그래픽 API를 바꾸면
    // MPV가 외부 렌더 타깃을 잃고 독립 비디오 창을 만들 수 있다.
    // 이 속성들은 initialize()에서만 옵션으로 설정하며 UI·사이드 탭에서는 절대 변경하지 않는다.
    static const QSet<QString> rendererLockedProperties = {
        QStringLiteral("vo"), QStringLiteral("gpu-api"),
        QStringLiteral("gpu-context"), QStringLiteral("wid"),
        QStringLiteral("force-window")
    };
    if (rendererLockedProperties.contains(name)) {
        qWarning() << "[MPV] 런타임 렌더러 속성 변경 차단:" << name;
        return;
    }

    if (value.typeId() == QMetaType::QString) {
        mpv_set_property_string(mpv_, name.toUtf8().constData(),
            value.toString().toUtf8().constData());
    } else if (value.typeId() == QMetaType::Double || value.typeId() == QMetaType::Float) {
        double v = value.toDouble();
        mpv_set_property(mpv_, name.toUtf8().constData(), MPV_FORMAT_DOUBLE, &v);
    } else if (value.typeId() == QMetaType::Bool) {
        // Bool: MPV_FORMAT_FLAG (0/1)
        int v = value.toBool() ? 1 : 0;
        mpv_set_property(mpv_, name.toUtf8().constData(), MPV_FORMAT_FLAG, &v);
    } else if (value.typeId() == QMetaType::Int || value.typeId() == QMetaType::LongLong
               || value.typeId() == QMetaType::UInt || value.typeId() == QMetaType::ULongLong) {
        // Int: MPV_FORMAT_INT64 — sid/aid/vid 등 트랙 ID는 정수로 전달
        // MPV_FORMAT_FLAG로 전달하면 0/1만 인식되어 트랙 선택이 잘못됨
        int64_t v = static_cast<int64_t>(value.toLongLong());
        mpv_set_property(mpv_, name.toUtf8().constData(), MPV_FORMAT_INT64, &v);
    }
}

// MPV_NODE를 QVariant로 재귀 변환
static QVariant nodeToVariant(const mpv_node& node) {
    switch (node.format) {
    case MPV_FORMAT_STRING:
        return QString::fromUtf8(node.u.string);
    case MPV_FORMAT_FLAG:
        return (bool)node.u.flag;
    case MPV_FORMAT_INT64:
        return (qlonglong)node.u.int64;
    case MPV_FORMAT_DOUBLE:
        return node.u.double_;
    case MPV_FORMAT_NODE_ARRAY: {
        QVariantList list;
        for (int i = 0; i < node.u.list->num; ++i)
            list << nodeToVariant(node.u.list->values[i]);
        return list;
    }
    case MPV_FORMAT_NODE_MAP: {
        QVariantMap map;
        for (int i = 0; i < node.u.list->num; ++i)
            map[QString::fromUtf8(node.u.list->keys[i])] =
                nodeToVariant(node.u.list->values[i]);
        return map;
    }
    default:
        return {};
    }
}

QVariant MpvCore::getProperty(const QString& name) const {
    if (!initialized_) return {};
    mpv_node node;
    int err = mpv_get_property(mpv_, name.toUtf8().constData(), MPV_FORMAT_NODE, &node);
    if (err < 0) {
        // NODE 실패 시 문자열로 폴백
        char* val = mpv_get_property_string(mpv_, name.toUtf8().constData());
        if (!val) return {};
        QVariant result = QString::fromUtf8(val);
        mpv_free(val);
        return result;
    }
    QVariant result = nodeToVariant(node);
    mpv_free_node_contents(&node);
    return result;
}

void MpvCore::command(const QStringList& args) {
    if (!initialized_) return;
    std::vector<QByteArray> byteArrays;
    std::vector<const char*> cargs;
    for (const auto& a : args) {
        byteArrays.push_back(a.toUtf8());
        cargs.push_back(byteArrays.back().constData());
    }
    cargs.push_back(nullptr);
    mpv_command_async(mpv_, 0, cargs.data());
}

QVariantList MpvCore::audioTracks() const {
    QVariantList result;
    if (!initialized_) return result;
    const QVariantList tracks = getProperty("track-list").toList();
    for (const QVariant& item : tracks) {
        const QVariantMap track = item.toMap();
        if (track.value("type").toString() == "audio")
            result.append(track);
    }
    return result;
}

QVariantList MpvCore::videoTracks() const {
    QVariantList result;
    if (!initialized_) return result;
    const QVariantList tracks = getProperty("track-list").toList();
    for (const QVariant& item : tracks) {
        const QVariantMap track = item.toMap();
        if (track.value("type").toString() == "video")
            result.append(track);
    }
    return result;
}

QVariantList MpvCore::subtitleTracks() const {
    QVariantList result;
    if (!initialized_) return result;
    const QVariantList tracks = getProperty("track-list").toList();
    for (const QVariant& item : tracks) {
        const QVariantMap track = item.toMap();
        if (track.value("type").toString() == "sub")
            result.append(track);
    }
    return result;
}

void MpvCore::setMotionSmoothing(bool enabled) {
    if (!initialized_) return;
    if (enabled) {
        // 프레임 보간 활성화: oversample 방식 (가장 안정적, 아티팩트 없음)
        mpv_set_property_string(mpv_, "interpolation", "yes");
        mpv_set_property_string(mpv_, "tscale", "oversample");
        // ── 노트북 환경: display-resample 금지 ──────────────────────
        // mpv GitHub #15196: Windows 11 24H2 + NVIDIA Optimus에서
        // display-resample이 주기적 끊김을 유발함 (WDDM 3.2 호환성 문제)
        // 노트북에서는 interpolation+tscale만 활성화하고 video-sync는 audio 유지
        if (!isLaptop_) {
            mpv_set_property_string(mpv_, "video-sync", "display-resample");
        } else {
            // 노트북: display-resample 대신 audio 유지 (끊김 방지)
            // interpolation=yes + video-sync=audio 조합으로 보간 효과는 유지
            mpv_set_property_string(mpv_, "video-sync", "audio");
            qInfo() << "[MPV] 모션 스무딩: 노트북 → display-resample 차단, audio 유지";
        }
    } else {
        // 모션 스무딩 비활성화: 원래 audio sync 모드로 복원
        mpv_set_property_string(mpv_, "interpolation", "no");
        mpv_set_property_string(mpv_, "video-sync", "audio");
    }
}

// ══════════════════════════════════════════════════════════════════
// GPU 렌더링 최적화 함수들
// ══════════════════════════════════════════════════════════════════

// ── 성능 프로파일 적용 ─────────────────────────────────────────────
// GPU 성능에 따라 렌더링 품질을 자동 또는 수동으로 조정
// 기존 기능(모션 스무딩, 패스스루 등)에 영향 없음
void MpvCore::applyRenderProfile(RenderProfile profile) {
    if (!initialized_) return;
    renderProfile_ = profile;

    switch (profile) {
    case RenderProfile::Eco:
        // 절전 모드: 노트북 통합 GPU (Intel Iris/UHD) 최적화
        // 화질보다 재생 안정성 우선
        mpv_set_property_string(mpv_, "scale",             "bilinear");
        mpv_set_property_string(mpv_, "dscale",            "bilinear");
        mpv_set_property_string(mpv_, "cscale",            "bilinear");
        mpv_set_property_string(mpv_, "sigmoid-upscaling", "no");
        mpv_set_property_string(mpv_, "scale-antiring",    "0.0");
        mpv_set_property_string(mpv_, "cscale-antiring",   "0.0");
        mpv_set_property_string(mpv_, "deband",            "no");
        mpv_set_property_string(mpv_, "dither",            "no");
        mpv_set_property_string(mpv_, "correct-downscaling", "no");
        mpv_set_property_string(mpv_, "linear-upscaling",  "no");
        mpv_set_property_string(mpv_, "hdr-compute-peak",  "no");
        mpv_set_property_string(mpv_, "keepaspect",        "yes");
        mpv_set_property_string(mpv_, "keepaspect-window", "no");
        qInfo() << "[MPV] 렌더 프로파일: Eco (절전 - 통합 GPU 최적화)";
        break;

    case RenderProfile::Balanced:
        // 균형 모드: 중급 GPU (GTX 1060 / RX 580 수준)
        // 화질과 성능의 균형
        mpv_set_property_string(mpv_, "scale",             "spline36");
        mpv_set_property_string(mpv_, "dscale",            "mitchell");
        mpv_set_property_string(mpv_, "cscale",            "spline36");
        mpv_set_property_string(mpv_, "sigmoid-upscaling", "yes");
        mpv_set_property_string(mpv_, "scale-antiring",    "0.5");
        mpv_set_property_string(mpv_, "cscale-antiring",   "0.5");
        mpv_set_property_string(mpv_, "deband",            "yes");
        mpv_set_property_string(mpv_, "deband-iterations", "1");
        mpv_set_property_string(mpv_, "dither",            "fruit");
        mpv_set_property_string(mpv_, "correct-downscaling", "yes");
        mpv_set_property_string(mpv_, "linear-upscaling",  "yes");
        mpv_set_property_string(mpv_, "hdr-compute-peak",  "yes");
        mpv_set_property_string(mpv_, "keepaspect",        "yes");
        mpv_set_property_string(mpv_, "keepaspect-window", "no");
        qInfo() << "[MPV] 렌더 프로파일: Balanced (균형 - 중급 GPU)";
        break;

    case RenderProfile::Quality:
        // 화질 모드: 고급 GPU (RTX 3060 / RX 6700 수준) ← 기본값
        // 현재 기본 설정과 동일
        mpv_set_property_string(mpv_, "scale",             "ewa_lanczossharp");
        mpv_set_property_string(mpv_, "dscale",            "mitchell");
        mpv_set_property_string(mpv_, "cscale",            "sinc");
        mpv_set_property_string(mpv_, "sigmoid-upscaling", "yes");
        mpv_set_property_string(mpv_, "scale-antiring",    "0.7");
        mpv_set_property_string(mpv_, "cscale-antiring",   "0.7");
        mpv_set_property_string(mpv_, "deband",            "yes");
        mpv_set_property_string(mpv_, "deband-iterations", "1");
        mpv_set_property_string(mpv_, "dither",            "fruit");
        mpv_set_property_string(mpv_, "correct-downscaling", "yes");
        mpv_set_property_string(mpv_, "linear-upscaling",  "yes");
        mpv_set_property_string(mpv_, "hdr-compute-peak",  "yes");
        mpv_set_property_string(mpv_, "keepaspect",        "yes");
        mpv_set_property_string(mpv_, "keepaspect-window", "no");
        qInfo() << "[MPV] 렌더 프로파일: Quality (화질 - 고급 GPU)";
        break;

    case RenderProfile::HiEnd:
        // 최고화질 모드: 전문가용 (RTX 4080 / RX 7900 수준)
        // 최대 화질, GPU 부하 높음
        mpv_set_property_string(mpv_, "scale",             "ewa_lanczossharp4sharpest");
        mpv_set_property_string(mpv_, "dscale",            "ewa_lanczossharp");
        mpv_set_property_string(mpv_, "cscale",            "ewa_lanczossharp");
        mpv_set_property_string(mpv_, "sigmoid-upscaling", "yes");
        mpv_set_property_string(mpv_, "scale-antiring",    "0.8");
        mpv_set_property_string(mpv_, "cscale-antiring",   "0.8");
        mpv_set_property_string(mpv_, "deband",            "yes");
        mpv_set_property_string(mpv_, "deband-iterations", "4");
        mpv_set_property_string(mpv_, "deband-threshold",  "64");
        mpv_set_property_string(mpv_, "deband-range",      "20");
        mpv_set_property_string(mpv_, "deband-grain",      "32");
        mpv_set_property_string(mpv_, "dither",            "fruit");
        mpv_set_property_string(mpv_, "dither-depth",      "auto");
        mpv_set_property_string(mpv_, "correct-downscaling", "yes");
        mpv_set_property_string(mpv_, "linear-upscaling",  "yes");
        mpv_set_property_string(mpv_, "hdr-compute-peak",  "yes");
        mpv_set_property_string(mpv_, "keepaspect",        "yes");
        mpv_set_property_string(mpv_, "keepaspect-window", "no");
        qInfo() << "[MPV] 렌더 프로파일: HiEnd (최고화질 - 전문가용)";
        break;
    }

    emit renderProfileChanged(profile);
}

// ── FPS 기반 video-sync 자동 전환 ─────────────────────────────────
// 파일 로드 후 container-fps를 읽어 최적의 동기화 모드 자동 선택
// 모션 스무딩이 활성화된 경우에는 display-resample을 유지
void MpvCore::applyVideoSyncByFps(double fps) {
    if (!initialized_) return;
    if (fps <= 0) return;

    currentFps_ = fps;

    // 모션 스무딩 활성화 여부 확인
    char* interpVal = mpv_get_property_string(mpv_, "interpolation");
    bool motionSmoothingOn = interpVal && QString::fromUtf8(interpVal) == "yes";
    mpv_free(interpVal);

    if (motionSmoothingOn) {
        // 노트북에서는 display-resample이 이미 차단되어 있으므로 안전하게 return
        // 데스크톱에서는 display-resample을 유지하여 모션 스무딩 효과 보장
        qInfo() << "[MPV] video-sync: 모션 스무딩 활성화 →"
                << (isLaptop_ ? "audio 유지 (노트북)" : "display-resample 유지");
        return;
    }

    // ── 모니터 주사율 × 영상 FPS 정밀 동기화 ────────────────────
    // 단순 60fps 기준이 아닌, 배수 관계를 고려한 정밀 선택
    // RenderEnvironment::selectVideoSync()가 배수 관계 분석:
    //   - 정수 배수 (60Hz/24fps=2.5 → 비정수): audio 모드 (저더 방지)
    //   - 정수 배수 (120Hz/24fps=5배): display-resample (완벽한 3:2 풀다운)
    //   - 120Hz 이상: 비정수여도 display-resample (충분한 주사율)
    double refreshRate = renderEnv_.refreshRate > 0 ? renderEnv_.refreshRate : 60.0;
    QString optimalSync = RenderEnvironment::selectVideoSync(fps, refreshRate);
    // RenderEnvironment::selectVideoSync()가 FPSxd7주사율 배수 관계를 분석하여 자동 선택
    // 노트북/데스크탑 구분 없이 모든 환경에서 동일하게 적용
    // (v6.11.9에서 추가한 노트북 강제 로직 제거 - 프레임 끊김 원인)
    qInfo() << "[MPV] video-sync 자동 선택:" << optimalSync
             << "(fps=" << fps << "Hz=" << refreshRate << ")";

    mpv_set_property_string(mpv_, "video-sync", optimalSync.toUtf8().constData());

    if (optimalSync == "display-resample") {
        mpv_set_property_string(mpv_, "interpolation", "no");
        // display-resample 시 framedrop 비활성화
        // framedrop=vo는 display-resample과 충돌하여 노트북 통합 GPU에서 프레임 끊김 유발
        // audio 모드에서만 framedrop=vo 사용
        mpv_set_property_string(mpv_, "framedrop", "no");
        // display-resample 최대 드롭 허용: 10% 이내 (노트북 환경 안전 마진)
        mpv_set_property_string(mpv_, "video-sync-max-video-change", "10");
    } else {
        // audio 모드: framedrop=vo 복원 (GPU 연산 능력 부족 시 프레임 스킵)
        mpv_set_property_string(mpv_, "framedrop", "vo");
    }
}
// tryGpuNext: DISABLED
// vo 변경은 libmpv 렌더 컨텍스트와 충돌하여 영상이 별도 창으로 분리되는 문제가 발생함.
// vo=libmpv는 초기화 시 한 번만 설정하며 이후 절대 변경하지 않음.

// ── GPU 벤더 재감지 및 설정 재적용 ───────────────────────────────
// MpvWidget::initializeGL() 이후 호출 (OpenGL 컨텍스트 준비 완료 후)
// 초기 initialize()에서는 OpenGL 컨텍스트가 없어 GPU 벤더 감지 불가.
// 이 함수에서 GL_RENDERER/GL_VENDOR를 읽어 GPU 벤더 기반 설정 재적용.
void MpvCore::redetectGpuAndApply() {
    if (!initialized_) return;

    RenderEnvInfo env = RenderEnvironment::detect();
    renderEnv_ = env;

    // GPU 벤더 감지 결과에 따라 설정 재적용
    // (초기 initialize()에서는 Unknown이었던 GPU 벤더가 이제 확정됨)
    qInfo() << "[MPV] GPU 재감지 완료:" << env.gpuRenderer
            << "→" << RenderEnvironment::gpuTierName(env.gpuTier);

    // ── 배치 적용: 재생 중 끊김 방지 ──────────────────────────────
    // mpv_set_property_string을 연속 호웉하면 각 호웉마다 렌더러 재초기화 가능
    // 모든 설정을 로컈 변수에 준비 후 일괄 적용
    struct RenderProp { const char* key; QByteArray val; };
    const QVector<RenderProp> renderProps = {
        {"scale",               env.scaleAlgo.toUtf8()},
        {"dscale",              env.dscaleAlgo.toUtf8()},
        {"cscale",              env.cscaleAlgo.toUtf8()},
        {"sigmoid-upscaling",   QByteArray(env.sigUpscaling      ? "yes" : "no")},
        {"deband",              QByteArray(env.debandEnabled      ? "yes" : "no")},
        {"hdr-compute-peak",    QByteArray(env.hdrComputePeak     ? "yes" : "no")},
        {"linear-upscaling",    QByteArray(env.linearUpscaling    ? "yes" : "no")},
        {"correct-downscaling", QByteArray(env.correctDownscaling ? "yes" : "no")},
        // keepaspect-window=no: 영상이 FBO(위젯) 크기에 맞게 꽉 채움
        // redetectGpuAndApply 호출 시 MPV 렌더러 파라미터 재계산으로
        // 이 값이 초기화될 수 있어 명시적 재설정 필요
        {"keepaspect",          QByteArray("yes")},
        {"keepaspect-window",   QByteArray("no")},
    };
    for (const auto& p : renderProps) {
        mpv_set_property_string(mpv_, p.key, p.val.constData());
    }

    // 실시간 프레임 드롭 모니터링 시작
    if (!frameDropTimer_) {
        frameDropTimer_ = new QTimer(this);
        frameDropTimer_->setInterval(3000);  // 3초 주기
        connect(frameDropTimer_, &QTimer::timeout,
                this, &MpvCore::onFrameDropCheck);
    }
    // 재생 시작 시 타이머 활성화 (현재 재생 중이면 즉시 시작)
    char* pauseVal = mpv_get_property_string(mpv_, "pause");
    bool isPaused = pauseVal && QString::fromUtf8(pauseVal) == "yes";
    mpv_free(pauseVal);
    if (!isPaused) frameDropTimer_->start();

    // 원래 scale 알고리즘 저장 (품질 강등 복원용)
    originalScale_   = env.scaleAlgo;
    debandOriginal_  = env.debandEnabled;

    // ── VRAM 기반 셰이더 캐시 설정 ───────────────────────────────
    // VRAM 4GB 이상: 셰이더 캐시 활성화 → 파일 로드 시간 단축
    // VRAM 2GB 미만: 캐시 비활성화 → VRAM 절약
    if (env.vramMb >= 4096) {
        // 셰이더 캐시 디렉토리 설정 (MPV 기본 캐시 경로 사용)
        mpv_set_property_string(mpv_, "gpu-shader-cache-dir", "~~/.cache/mpv/shaders");
        // Direct Memory Access: VRAM 8GB 이상에서 직접 렌더링 활성화
        if (env.vramMb >= 8192) {
            mpv_set_property_string(mpv_, "vd-lavc-dr", "yes");
            qInfo() << "[MPV] VRAM 8GB+ → vd-lavc-dr=yes (직접 렌더링 활성화)";
        }
        qInfo() << "[MPV] VRAM" << env.vramMb << "MB → 셰이더 캐시 활성화";
    } else if (env.vramMb > 0 && env.vramMb < 2048) {
        // VRAM 2GB 미만: 셰이더 캐시 비활성화
        mpv_set_property_string(mpv_, "gpu-shader-cache-dir", "");
        qInfo() << "[MPV] VRAM" << env.vramMb << "MB 미만 → 셰이더 캐시 비활성화";
    }

    // ── HDR 디스플레이 감지 시 tone-mapping 자동 전환 ────────────
    // Windows HDR 활성화된 디스플레이: tone-mapping=auto (패스스루)
    // SDR 디스플레이: bt.2446a 유지 (HDR→SDR 변환)
    if (env.hdrEnabled) {
        mpv_set_property_string(mpv_, "tone-mapping",          "auto");
        mpv_set_property_string(mpv_, "target-colorspace-hint", "yes");
        mpv_set_property_string(mpv_, "target-peak",            "auto");
        qInfo() << "[MPV] HDR 디스플레이 감지 → tone-mapping=auto (패스스루 모드)";
    } else {
        // SDR 디스플레이: BT.2446a 톤매핑 유지
        mpv_set_property_string(mpv_, "tone-mapping", "bt.2446a");
        qInfo() << "[MPV] SDR 디스플레이 → tone-mapping=bt.2446a";
    }

    // ── 디코딩 스레드 수 GPU 부하 연동 ───────────────────────────
    // 4K 환경 또는 통합 GPU에서 스레드 수 제한 → CPU-GPU 대역폭 경합 감소
    if (env.lavcThreads > 0) {
        mpv_set_property_string(mpv_, "vd-lavc-threads",
            QString::number(env.lavcThreads).toUtf8().constData());
        qInfo() << "[MPV] lavc-threads=" << env.lavcThreads
                << "(GPU 부하 연동, 자동=0)";
    } else {
        mpv_set_property_string(mpv_, "vd-lavc-threads", "0");  // 자동
    }

    // NOTE: gpu-next(vo 변경)는 libmpv 렌더 컨텍스트와 충돌하여
    // 영상이 별도 창으로 분리되는 문제가 발생함.
    // vo=libmpv는 초기화 시 한 번만 설정하며 이후 절대 변경하지 않음.
}

// ── 영상 해상도 기반 업스케일 알고리즘 최적화 ────────────────────
// FILE_LOADED 이벤트에서 호출 (video-params/w, video-params/h 확인 후)
//
// 핵심 원칙:
//   - 영상 >= 화면: 다운스케일만 필요 → scale=bilinear (부하 최소화)
//   - 영상 << 화면: 업스케일 효과 큼 → 고품질 알고리즘 적용
void MpvCore::optimizeScaleForContent(int videoW, int videoH) {
    if (!initialized_ || videoW <= 0 || videoH <= 0) return;

    currentVideoW_ = videoW;
    currentVideoH_ = videoH;

    RenderEnvironment::optimizeScaleForContent(mpv_, videoW, videoH, renderEnv_);

    // 최적화된 scale 알고리즘을 원래 알고리즘으로 업데이트 (강등 복원 기준)
    char* currentScale = mpv_get_property_string(mpv_, "scale");
    if (currentScale) {
        originalScale_ = QString::fromUtf8(currentScale);
        mpv_free(currentScale);
    }
}

// ── 코덱 기반 최적 hwdec 자동 선택 및 적용 ──────────────────────
// FILE_LOADED 이벤트에서 호출 (video-codec 확인 후)
//
// 사용자가 설정창에서 hwdec를 수동 설정한 경우 덮어쓰지 않음.
// auto-safe 또는 기본값인 경우에만 코덱별 최적값 자동 적용.
void MpvCore::applyOptimalHwdec(const QString& codec) {
    if (!initialized_ || codec.isEmpty()) return;
    currentCodec_ = codec;

    // 노트북(Optimus) 환경에서는 d3d11va 강제 금지
    // 이유: Intel GPU에서 D3D11 디바이스 생성 시 NVIDIA OpenGL 컨텍스트와 GPU 불일치 발생
    //       → 하드웨어 디코딩 실패 및 재생 불가
    // 해결: auto-safe 유지 (MPV가 현재 GPU에 맞는 방식을 자동 선택)
    if (isLaptop_) {
        qInfo() << "[MPV] 노트북 Optimus 환경 → hwdec=auto-safe 유지 (d3d11va 강제 금지)";
        mpv_set_property_string(mpv_, "hwdec", "auto-safe");
        return;
    }

    // 사용자가 수동으로 hwdec를 설정한 경우 덮어쓰지 않음
    // (설정창에서 "소프트웨어" 또는 특정 방식을 선택한 경우)
    char* currentHwdec = mpv_get_property_string(mpv_, "hwdec");
    QString hwdecStr = currentHwdec ? QString::fromUtf8(currentHwdec) : "auto-safe";
    mpv_free(currentHwdec);
    // 코덱별 최적 hwdec 선택
    QString optimalHwdec = RenderEnvironment::selectHwdec(
        codec, renderEnv_.gpuVendor, renderEnv_.gpuTier);
    if (optimalHwdec != hwdecStr) {
        mpv_set_property_string(mpv_, "hwdec", optimalHwdec.toUtf8().constData());
        qInfo() << "[MPV] 코덱별 hwdec 최적화:" << codec
                << "→ hwdec=" << optimalHwdec;
    }
}

// ── 실시간 프레임 드롭 모니터링 및 자동 품질 강등/복원 ───────────
// 3초 주기로 frame-drop-count를 확인하여 끊김 감지 시 품질 자동 강등.
// 정상 복귀 후 30초(10회 정상) 유지 시 원래 품질로 복원.
void MpvCore::onFrameDropCheck() {
    if (!initialized_) return;

    // 현재 frame-drop-count 읽기
    int64_t dropCount = 0;
    mpv_get_property(mpv_, "frame-drop-count", MPV_FORMAT_INT64, &dropCount);

    int newDrops = static_cast<int>(dropCount) - prevFrameDropCount_;
    prevFrameDropCount_ = static_cast<int>(dropCount);

    // 일시 정지 중이면 카운터 리셋
    char* pauseVal = mpv_get_property_string(mpv_, "pause");
    bool isPaused = pauseVal && QString::fromUtf8(pauseVal) == "yes";
    mpv_free(pauseVal);
    if (isPaused) return;

    if (newDrops >= 3) {
        // 3초간 3프레임 이상 드롭: 끊김 감지
        dropEventCount_++;
        normalEventCount_ = 0;
        qWarning() << "[MPV] 프레임 드롭 감지:" << newDrops
                   << "프레임/3초 (연속" << dropEventCount_ << "회)";

        if (!qualityDegraded_ && dropEventCount_ >= 2) {
            // 연속 2회 (6초간) 드롭 → 품질 강등
            qualityDegraded_ = true;

            // 1단계: deband 비활성화 (가장 효과적인 부하 감소)
            mpv_set_property_string(mpv_, "deband", "no");
            qInfo() << "[MPV] 자동 품질 강등: deband 비활성화";
            emit renderQualityDegraded("프레임 드롭 감지 → deband 비활성화");

        } else if (qualityDegraded_ && dropEventCount_ >= 4) {
            // 연속 4회 (12초간) 드롭 → 추가 강등: scale 알고리즘 단순화
            QString currentScale;
            char* cs = mpv_get_property_string(mpv_, "scale");
            if (cs) { currentScale = QString::fromUtf8(cs); mpv_free(cs); }

            if (currentScale == "ewa_lanczossharp") {
                mpv_set_property_string(mpv_, "scale",  "spline36");
                mpv_set_property_string(mpv_, "cscale", "spline36");
                qInfo() << "[MPV] 자동 품질 강등: ewa_lanczossharp → spline36";
                emit renderQualityDegraded("심각한 드롭 → scale=spline36");
            } else if (currentScale == "spline36") {
                mpv_set_property_string(mpv_, "scale",  "bilinear");
                mpv_set_property_string(mpv_, "cscale", "bilinear");
                qInfo() << "[MPV] 자동 품질 강등: spline36 → bilinear";
                emit renderQualityDegraded("심각한 드롭 → scale=bilinear");
            }
        }
    } else {
        // 정상 프레임 타임
        if (qualityDegraded_) {
            normalEventCount_++;
            dropEventCount_ = 0;

            if (normalEventCount_ >= 10) {
                // 30초간 정상 → 원래 품질로 복원
                qualityDegraded_ = false;
                normalEventCount_ = 0;

                mpv_set_property_string(mpv_, "scale",
                    originalScale_.toUtf8().constData());
                mpv_set_property_string(mpv_, "cscale",
                    renderEnv_.cscaleAlgo.toUtf8().constData());
                mpv_set_property_string(mpv_, "deband",
                    debandOriginal_ ? "yes" : "no");

                qInfo() << "[MPV] 자동 품질 복원:"
                        << "scale=" << originalScale_
                        << ", deband=" << (debandOriginal_ ? "yes" : "no");
                emit renderQualityRestored();
            }
        } else {
            dropEventCount_ = 0;
        }
    }
}

// ── 실시간 스펙트럼 (의사 FFT 기반 VU 레벨 분산) ──────────────────────────
static const int SPEC_BINS = 32;

void MpvCore::setSpectrumEnabled(bool on) {
    specEnabled_ = on;
    if (on) {
        if (!spectrumTimer_) {
            spectrumTimer_ = new QTimer(this);
            spectrumTimer_->setInterval(16);
            connect(spectrumTimer_, &QTimer::timeout, this, &MpvCore::onSpectrumTick);
        }
        specBins_.resize(SPEC_BINS, 0.0f);
        specSmooth_.resize(SPEC_BINS, 0.0f);
        spectrumTimer_->start();
    } else {
        if (spectrumTimer_) spectrumTimer_->stop();
    }
}

void MpvCore::onSpectrumTick() {
    if (!mpv_ || !initialized_) return;

    double vol = 0.0;
    mpv_get_property(mpv_, "volume", MPV_FORMAT_DOUBLE, &vol);

    int paused = 0;
    mpv_get_property(mpv_, "pause", MPV_FORMAT_FLAG, &paused);

    double pos = 0.0;
    mpv_get_property(mpv_, "time-pos", MPV_FORMAT_DOUBLE, &pos);

    float targetLevel = paused ? 0.0f : qMin(1.0f, (float)(vol / 130.0));
    float alpha = (targetLevel > specLevel_) ? 0.35f : 0.08f;
    specLevel_ = specLevel_ * (1.0f - alpha) + targetLevel * alpha;

    static quint64 seed = 12345;
    seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;

    for (int i = 0; i < SPEC_BINS; ++i) {
        float freqWeight;
        if (i < 8)       freqWeight = 1.0f - (float)i / 16.0f;
        else if (i < 20) freqWeight = 0.5f + (float)(i-8) / 48.0f;
        else             freqWeight = 0.75f - (float)(i-20) / 48.0f;

        quint64 h = seed ^ ((quint64)i * 2654435761ULL);
        h ^= h >> 33; h *= 0xff51afd7ed558ccdULL;
        h ^= h >> 33; h *= 0xc4ceb9fe1a85ec53ULL;
        float noise = (float)(h & 0xFFFF) / 65535.0f;
        float variation = 0.8f + noise * 0.4f;

        float target = specLevel_ * freqWeight * variation;
        float a = (target > specSmooth_[i]) ? 0.4f : 0.12f;
        specSmooth_[i] = specSmooth_[i] * (1.0f - a) + target * a;
        specBins_[i] = qMin(specSmooth_[i], 1.0f);
    }

    emit spectrumReady(specBins_);
}
