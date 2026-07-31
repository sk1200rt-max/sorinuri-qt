#include "MpvCore.h"
#include <QDebug>
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

MpvCore::MpvCore(QObject* parent) : QObject(parent) {
    setlocale(LC_NUMERIC, "C");

    mpv_ = mpv_create();
    if (!mpv_) throw std::runtime_error("mpv_create() failed");

    // 이벤트 타이머 (Qt 이벤트 루프와 연동)
    eventTimer_ = new QTimer(this);
    eventTimer_->setInterval(16);  // ~60fps
    connect(eventTimer_, &QTimer::timeout, this, &MpvCore::onMpvEvents);
}

MpvCore::~MpvCore() {
    if (eventTimer_) eventTimer_->stop();
    if (mpv_) mpv_terminate_destroy(mpv_);
}

bool MpvCore::initialize(WId windowId) {
    if (initialized_) return true;

    // ── mpv_initialize 이전에 설정해야 하는 옵션 ──────────────────
    if (windowId != 0) {
        // 전통적 --wid 모드 (사용 안 함)
        int64_t wid = static_cast<int64_t>(windowId);
        check_error(mpv_set_option(mpv_, "wid", MPV_FORMAT_INT64, &wid));
        check_error(mpv_set_option_string(mpv_, "vo", "gpu"));
    } else {
        // render API 모드 (QOpenGLWidget + mpv_render_context)
        // vo=libmpv: MPV가 자체 윈도우 생성 안 함
        check_error(mpv_set_option_string(mpv_, "vo", "libmpv"));
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

    // ── 하드웨어 디코딩 ──────────────────────────────────────────────
    // auto-safe: NVIDIA/AMD/Intel GPU 자동 감지, 실패 시 소프트웨어로 폴백
    check_error(mpv_set_property_string(mpv_, "hwdec", "auto-safe"));

    // ── 비디오 동기화 ────────────────────────────────────────────────
    // audio: 오디오 타이밍 기준으로 동기화 → OpenGL 렌더 컨텍스트와 충돌 없음
    // display-resample은 모니터 주사율 재샘플링으로 끊김 발생 가능
    check_error(mpv_set_property_string(mpv_, "video-sync", "audio"));

    // ── 업스케일링 알고리즘 (화질 핵심) ─────────────────────────────
    // ewa_lanczossharp: 현존 최고의 업스케일 선명도 (1080p→4K 등)
    // mitchell: 다운스케일 시 링잉 없이 부드럽게
    // sinc: 크로마(색상) 업스케일링 정확도 극대화
    check_error(mpv_set_property_string(mpv_, "scale",  "ewa_lanczossharp"));
    check_error(mpv_set_property_string(mpv_, "dscale", "mitchell"));
    check_error(mpv_set_property_string(mpv_, "cscale", "sinc"));
    // 시그모이드 업스케일링: 링잉 아티팩트 억제하면서 선명도 유지
    check_error(mpv_set_property_string(mpv_, "sigmoid-upscaling", "yes"));

    // ── HDR 톤매핑 (HDR 영상을 SDR 모니터에서 정확하게 재현) ─────────
    // bt.2446a: ITU-R BT.2446 Annex A 방식. 현존 가장 자연스러운 HDR→SDR 변환
    //           밝은 영역 클리핑 없이 전체 다이나믹 레인지를 SDR에 매핑
    check_error(mpv_set_property_string(mpv_, "tone-mapping", "bt.2446a"));
    // 영상의 실제 최대 밝기를 동적으로 측정하여 톤매핑에 반영
    check_error(mpv_set_property_string(mpv_, "hdr-compute-peak", "yes"));

    // ── 색상 관리 (Color Management) ────────────────────────────────
    // target-colorspace-hint: 모니터의 ICC 프로파일/EDID를 읽어
    //   영상의 색공간(BT.709/BT.2020/DCI-P3)을 정확하게 변환
    //   → 제작사가 의도한 색감 그대로 재현
    check_error(mpv_set_property_string(mpv_, "target-colorspace-hint", "yes"));
    // 선형 광학 연산: 색상 혼합 및 스케일링을 선형 광학 공간에서 수행
    //   → 물리적으로 정확한 색상 처리
    check_error(mpv_set_property_string(mpv_, "linear-upscaling",   "yes"));
    check_error(mpv_set_property_string(mpv_, "correct-downscaling", "yes"));

    // ── 디밴딩 (Color Banding 제거) ──────────────────────────────────
    // 어두운 장면, 하늘, 그라데이션에서 발생하는 등고선 노이즈 제거
    // iterations=2: 2회 반복으로 강한 밴딩도 제거
    check_error(mpv_set_property_string(mpv_, "deband",            "yes"));
    check_error(mpv_set_property_string(mpv_, "deband-iterations", "2"));
    check_error(mpv_set_property_string(mpv_, "deband-threshold",  "48"));
    check_error(mpv_set_property_string(mpv_, "deband-range",      "16"));
    check_error(mpv_set_property_string(mpv_, "deband-grain",      "24"));

    // ── 디더링 (Dithering) ───────────────────────────────────────────
    // 8bit/10bit 패널에 맞춰 디더링 적용 → 부드러운 그라데이션
    // fruit: 오류 확산 디더링 (가장 자연스러운 결과)
    check_error(mpv_set_property_string(mpv_, "dither-depth", "auto"));
    check_error(mpv_set_property_string(mpv_, "dither",       "fruit"));

    // ── 오디오: WASAPI Exclusive (기본) ──────────────────────────────
    // Exclusive: Windows 믹서 완전 우회 → Bit-Perfect, 패스스루 가능
    check_error(mpv_set_property_string(mpv_, "ao", "wasapi"));
    check_error(mpv_set_property_string(mpv_, "audio-exclusive", "yes"));
    // 패스스루: 돌비 애트모스/DTS:X 원본 비트스트림 리시버로 전송
    check_error(mpv_set_property_string(mpv_, "audio-spdif",
        "ac3,eac3,dts,dts-hd,truehd"));
    // 오디오 초기화 실패 시 영상은 계속 재생 (null 오디오 폴백)
    // → Exclusive 실패해도 영상/타임라인/트랙 정보 모두 정상 동작
    check_error(mpv_set_property_string(mpv_, "audio-fallback-to-null", "yes"));
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
    mpv_observe_property(mpv_, 0, "audio-out-detected-device", MPV_FORMAT_STRING);
    mpv_observe_property(mpv_, 0, "video-params/w",  MPV_FORMAT_INT64);
    mpv_observe_property(mpv_, 0, "video-params/h",  MPV_FORMAT_INT64);
    mpv_observe_property(mpv_, 0, "container-fps",   MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv_, 0, "video-codec",     MPV_FORMAT_STRING);

    // wakeup callback 대신 타이머 사용 (Qt 스레드 안전)
    mpv_set_wakeup_callback(mpv_, wakeupCallback, this);
    eventTimer_->start();

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
                int ch = 0;
                if (channelStr == "mono") ch = 1;
                else if (channelStr == "stereo") ch = 2;
                else if (channelStr.contains("5.1")) ch = 6;
                else if (channelStr.contains("7.1")) ch = 8;
                else if (channelStr.contains("6.1")) ch = 7;
                else { bool ok; int n = channelStr.toInt(&ok); if (ok) ch = n; }
                char* aoFmt = mpv_get_property_string(mpv_, "audio-out-params/format");
                QString outStr = aoFmt ? QString::fromUtf8(aoFmt) : "";
                mpv_free(aoFmt);
                emit audioFormatChanged(codecStr, ch, static_cast<int>(sr), outStr);
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
            // pause 아닌데 재생이 멈춤(core-idle) + 위치 0 부근 → AO 멈춤
            if (!paused && coreIdle && pos < 0.5) {
                appLog("워치독: 재생 멈춤 감지 → spdif 해제 + ao-reload 강제 복구");
                qWarning() << "[MPV] 워치독: 재생 멈춤 → ao-reload 복구";
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
        break;

    case MPV_EVENT_END_FILE: {
        auto* ef = reinterpret_cast<mpv_event_end_file*>(event->data);
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
            appLog("AO 초기화 실패 감지 → 자동 복구 시작");
            qWarning() << "[MPV] AO 실패 감지 → spdif 해제 후 ao-reload";
            // 1) 패스스루 해제 (현재 장치가 미지원이므로 PCM 디코딩으로 전환)
            mpv_set_property_string(mpv_, "audio-spdif", "");
            // 2) Exclusive 실패 가능성 대비: 재시도는 Exclusive 유지,
            //    ao-reload로 AO 재초기화 (장치가 Exclusive PCM은 지원)
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
             name == "audio-samplerate") {
        // 오디오 포맷 정보 업데이트
        char* codec = mpv_get_property_string(mpv_, "audio-codec-name");
        char* channels = mpv_get_property_string(mpv_, "audio-channels");
        int64_t samplerate = 0;
        mpv_get_property(mpv_, "audio-samplerate", MPV_FORMAT_INT64, &samplerate);

        QString codecStr   = codec    ? QString::fromUtf8(codec)    : "";
        QString channelStr = channels ? QString::fromUtf8(channels) : "";
        mpv_free(codec);
        mpv_free(channels);

        // 채널 수 파싱 ("stereo"=2, "5.1"=6, "7.1"=8, 숫자 등)
        int channelCount = 0;
        if (channelStr == "mono")   channelCount = 1;
        else if (channelStr == "stereo") channelCount = 2;
        else if (channelStr.contains("5.1")) channelCount = 6;
        else if (channelStr.contains("7.1")) channelCount = 8;
        else if (channelStr.contains("6.1")) channelCount = 7;
        else {
            // 숫자 형태 처리
            bool ok;
            int n = channelStr.toInt(&ok);
            if (ok) channelCount = n;
        }

        // 현재 오디오 출력 드라이버에서 패스스루 여부 감지
        char* aoFormat = mpv_get_property_string(mpv_, "audio-out-params/format");
        QString outputStr = aoFormat ? QString::fromUtf8(aoFormat) : "";
        mpv_free(aoFormat);
        // spdif 출력 시 패스스루
        if (outputStr.isEmpty()) {
            char* ao = mpv_get_property_string(mpv_, "current-ao");
            outputStr = ao ? QString::fromUtf8(ao) : "";
            mpv_free(ao);
        }

        emit audioFormatChanged(codecStr, channelCount, static_cast<int>(samplerate), outputStr);
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
        if (w > 0 && h > 0)
            emit videoInfoChanged(static_cast<int>(w), static_cast<int>(h), fps, vcodecStr);
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
    QString desc;
    const QVariantList devices = audioDeviceList();
    if (selected != "auto") {
        for (const QVariant& item : devices) {
            QVariantMap m = item.toMap();
            if (m.value("name").toString() == selected) {
                desc = m.value("description").toString();
                break;
            }
        }
    }
    // auto이거나 못 찾으면 첫 번째 wasapi 장치(기본 출력)의 description 사용
    if (desc.isEmpty()) {
        for (const QVariant& item : devices) {
            QVariantMap m = item.toMap();
            if (m.value("name").toString().startsWith("wasapi/")) {
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
    if (!append) {
        if (deviceLikelySupportsPassthrough()) {
            if (passthroughEnabled_)
                mpv_set_property_string(mpv_, "audio-spdif", spdifCodecs_.toUtf8().constData());
        } else {
            mpv_set_property_string(mpv_, "audio-spdif", "");
        }
    }
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
    if (!initialized_) return;
    mpv_set_property_string(mpv_, "vo", vo.toUtf8().constData());
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
    if (value.typeId() == QMetaType::QString) {
        mpv_set_property_string(mpv_, name.toUtf8().constData(),
            value.toString().toUtf8().constData());
    } else if (value.typeId() == QMetaType::Double || value.typeId() == QMetaType::Float) {
        double v = value.toDouble();
        mpv_set_property(mpv_, name.toUtf8().constData(), MPV_FORMAT_DOUBLE, &v);
    } else if (value.typeId() == QMetaType::Int || value.typeId() == QMetaType::Bool) {
        int v = value.toInt();
        mpv_set_property(mpv_, name.toUtf8().constData(), MPV_FORMAT_FLAG, &v);
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
    // track-list에서 오디오 트랙만 추출
    QVariantList result;
    if (!initialized_) return result;
    // TODO: mpv_node 파싱
    return result;
}

QVariantList MpvCore::videoTracks() const {
    QVariantList result;
    return result;
}

QVariantList MpvCore::subtitleTracks() const {
    QVariantList result;
    return result;
}
