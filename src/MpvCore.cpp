#include "MpvCore.h"
#include <QDebug>
#include <QMetaObject>
#include <locale.h>
#include <stdexcept>

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

    check_error(mpv_set_option_string(mpv_, "osc",       "no"));
    check_error(mpv_set_option_string(mpv_, "idle",      "yes"));
    check_error(mpv_set_option_string(mpv_, "keep-open", "yes"));

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
    // display-resample: 모니터 주사율에 맞춰 프레임 타이밍 최적화 (티어링 방지)
    check_error(mpv_set_property_string(mpv_, "video-sync", "display-resample"));

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

    // ── 오디오: WASAPI Exclusive + Bit-Perfect + 패스스루 ─────────────
    check_error(mpv_set_property_string(mpv_, "ao", "wasapi"));
    // audio-exclusive=yes: Windows 믹서 완전 우회 (핵심 설정)
    //   → 볼륨 조절, 리샘플링, 믹싱 없이 원음 그대로 DAC 전달
    //   → Bit-Perfect 재생 실현
    check_error(mpv_set_property_string(mpv_, "audio-exclusive", "yes"));
    // 패스스루: 돌비 애트모스/DTS:X 원본 비트스트림을 리시버로 전송
    check_error(mpv_set_property_string(mpv_, "audio-spdif",
        "ac3,eac3,dts,dts-hd,truehd"));
    // 오디오 필터 비움 (패스스루 시 필터 적용 불가)
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
        break;

    case MPV_EVENT_FILE_LOADED: {
        char* rawPath = mpv_get_property_string(mpv_, "path");
        QString path = rawPath ? QString::fromUtf8(rawPath) : QString();
        mpv_free(rawPath);
        qInfo() << "[MPV] FILE_LOADED:" << path;
        // pause 해제 - command 방식으로
        const char* pauseArgs[] = { "set", "pause", "no", nullptr };
        mpv_command_async(mpv_, 0, pauseArgs);
        emit fileLoaded(path);
        break;
    }

    case MPV_EVENT_PLAYBACK_RESTART:
        qInfo() << "[MPV] PLAYBACK_RESTART - 재생 시작";
        emit playbackStarted();
        break;

    case MPV_EVENT_END_FILE: {
        auto* ef = reinterpret_cast<mpv_event_end_file*>(event->data);
        if (ef->reason == MPV_END_FILE_REASON_EOF)
            emit playbackEnded();
        else if (ef->reason == MPV_END_FILE_REASON_ERROR)
            emit errorOccurred(QString::fromUtf8(mpv_error_string(ef->error)));
        break;
    }

    case MPV_EVENT_PROPERTY_CHANGE:
        handlePropertyChange(reinterpret_cast<mpv_event_property*>(event->data));
        break;

    case MPV_EVENT_LOG_MESSAGE: {
        auto* msg = reinterpret_cast<mpv_event_log_message*>(event->data);
        if (msg->log_level <= MPV_LOG_LEVEL_WARN)
            qWarning() << "[MPV]" << msg->prefix << msg->text;
        break;
    }

    default:
        break;
    }
}

void MpvCore::handlePropertyChange(mpv_event_property* prop) {
    QString name = QString::fromUtf8(prop->name);

    if (name == "time-pos" && prop->format == MPV_FORMAT_DOUBLE) {
        emit positionChanged(*reinterpret_cast<double*>(prop->data));
    }
    else if (name == "duration" && prop->format == MPV_FORMAT_DOUBLE) {
        emit durationChanged(*reinterpret_cast<double*>(prop->data));
    }
    else if (name == "pause" && prop->format == MPV_FORMAT_FLAG) {
        bool paused = *reinterpret_cast<int*>(prop->data);
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
void MpvCore::loadFile(const QString& path, bool append) {
    if (!initialized_) {
        qWarning() << "[MPV] loadFile 호출되었지만 초기화 안됨!";
        return;
    }
    qInfo() << "[MPV] loadFile:" << path << "| mode:" << (append ? "append" : "replace");
    const char* mode = append ? "append-play" : "replace";
    QByteArray pathBytes = path.toUtf8();
    const char* args[] = { "loadfile", pathBytes.constData(), mode, nullptr };
    int ret = mpv_command_async(mpv_, 0, args);
    qInfo() << "[MPV] mpv_command_async result:" << ret;
    // pause 해제는 MPV_EVENT_FILE_LOADED 이벤트에서 처리
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
    if (!initialized_) return;
    if (passthrough) {
        mpv_set_property_string(mpv_, "audio-spdif",
            "ac3,eac3,dts,dts-hd,truehd");
    } else {
        mpv_set_property_string(mpv_, "audio-spdif", "");
    }
}

void MpvCore::setSpdifCodecs(const QStringList& codecs) {
    if (!initialized_) return;
    mpv_set_property_string(mpv_, "audio-spdif",
        codecs.join(',').toUtf8().constData());
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

QVariant MpvCore::getProperty(const QString& name) const {
    if (!initialized_) return {};
    char* val = mpv_get_property_string(mpv_, name.toUtf8().constData());
    if (!val) return {};
    QVariant result = QString::fromUtf8(val);
    mpv_free(val);
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
