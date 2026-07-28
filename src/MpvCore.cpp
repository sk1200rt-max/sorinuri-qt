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
    int64_t wid = static_cast<int64_t>(windowId);
    check_error(mpv_set_option(mpv_, "wid", MPV_FORMAT_INT64, &wid));

    check_error(mpv_set_option_string(mpv_, "vo",        "gpu"));
    check_error(mpv_set_option_string(mpv_, "osc",       "no"));
    check_error(mpv_set_option_string(mpv_, "idle",      "yes"));
    check_error(mpv_set_option_string(mpv_, "keep-open", "yes"));

    // ── mpv_initialize ────────────────────────────────────────────
    int ret = mpv_initialize(mpv_);
    if (ret < 0) {
        qCritical() << "[MPV] mpv_initialize 실패:" << mpv_error_string(ret);
        return false;
    }

    // ── 초기화 후 설정 ────────────────────────────────────────────
    // 비디오
    check_error(mpv_set_property_string(mpv_, "gpu-api",      "d3d11"));
    check_error(mpv_set_property_string(mpv_, "hwdec",        "d3d11va"));
    check_error(mpv_set_property_string(mpv_, "video-sync",   "display-resample"));

    // 오디오 (WASAPI + 패스스루)
    check_error(mpv_set_property_string(mpv_, "ao",           "wasapi"));
    check_error(mpv_set_property_string(mpv_, "audio-spdif",  "ac3,eac3,dts,dts-hd,truehd"));
    check_error(mpv_set_property_string(mpv_, "af",           ""));

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

    // wakeup callback 대신 타이머 사용 (Qt 스레드 안전)
    mpv_set_wakeup_callback(mpv_, wakeupCallback, this);
    eventTimer_->start();

    initialized_ = true;
    qInfo() << "[MPV] 초기화 완료 WID:" << windowId;
    return true;
}

void MpvCore::wakeupCallback(void* ctx) {
    MpvCore* self = static_cast<MpvCore*>(ctx);
    // Qt 메인 스레드에서 이벤트 처리 요청
    QMetaObject::invokeMethod(self, "onMpvEvents", Qt::QueuedConnection);
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
        emit fileLoaded(path);
        emit playbackStarted();
        break;
    }

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

        emit audioFormatChanged(codecStr, 0, static_cast<int>(samplerate), channelStr);
    }
}

// ─── 재생 제어 ────────────────────────────────────────────────────
void MpvCore::loadFile(const QString& path, bool append) {
    if (!initialized_) return;
    const char* mode = append ? "append-play" : "replace";
    const char* args[] = { "loadfile", path.toUtf8().constData(), mode, nullptr };
    mpv_command_async(mpv_, 0, args);
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
