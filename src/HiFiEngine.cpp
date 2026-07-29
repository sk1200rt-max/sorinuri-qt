#include "HiFiEngine.h"
#include "MpvCore.h"
#include <QStringList>
#include <cmath>

HiFiEngine::HiFiEngine(MpvCore* core, QObject* parent)
    : QObject(parent), core_(core)
{
    // 기본 EQ 밴드 초기화 (10밴드: 31, 63, 125, 250, 500, 1k, 2k, 4k, 8k, 16kHz)
    static const double freqs[] = {31, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};
    for (double f : freqs)
        eqBands_.append({f, 0.0, 1.41});
}

// ── 음악 파일 감지 ───────────────────────────────────────────────────
QStringList HiFiEngine::musicExtensions() {
    return {
        "flac", "wav", "aiff", "aif", "alac", "m4a",
        "mp3", "ogg", "opus", "wma", "ape", "wv",
        "dsf", "dff",   // DSD
        "mka"           // Matroska Audio
    };
}

bool HiFiEngine::isMusicFile(const QString& path) {
    if (path.startsWith("http://") || path.startsWith("https://") ||
        path.startsWith("rtmp://") || path.startsWith("rtsp://"))
        return false;
    QString ext = path.section('.', -1).toLower();
    return musicExtensions().contains(ext);
}

// ── 비트 퍼펙트 ─────────────────────────────────────────────────────
void HiFiEngine::setBitPerfect(bool enable) {
    bitPerfect_ = enable;
    if (enable) {
        // 업샘플링 비활성화 (원본 샘플레이트 유지)
        resampleQ_ = ResampleQuality::None;
    }
    applyAll();
    emit bitPerfectChanged(enable);
}

// ── ReplayGain ───────────────────────────────────────────────────────
void HiFiEngine::setReplayGain(ReplayGainMode mode) {
    rgMode_ = mode;
    if (!core_) return;

    switch (mode) {
    case ReplayGainMode::Off:
        core_->setProperty("replaygain", QString("no"));
        break;
    case ReplayGainMode::Track:
        core_->setProperty("replaygain", QString("track"));
        core_->setProperty("replaygain-clip", QString("yes"));
        break;
    case ReplayGainMode::Album:
        core_->setProperty("replaygain", QString("album"));
        core_->setProperty("replaygain-clip", QString("yes"));
        break;
    }
}

// ── 업샘플링 ─────────────────────────────────────────────────────────
void HiFiEngine::setResampleQuality(ResampleQuality q) {
    resampleQ_ = q;
    if (q != ResampleQuality::None) bitPerfect_ = false;
    applyAudioFilters();
}

// ── EQ ───────────────────────────────────────────────────────────────
void HiFiEngine::setEqEnabled(bool enable) {
    eqEnabled_ = enable;
    applyAudioFilters();
    emit eqChanged();
}

void HiFiEngine::applyEqPreset(const EqPreset& preset) {
    if (preset.bands.size() == eqBands_.size()) {
        eqBands_ = preset.bands;
    }
    eqEnabled_ = true;
    applyAudioFilters();
    emit eqChanged();
}

void HiFiEngine::setEqBand(int bandIdx, double gainDb) {
    if (bandIdx < 0 || bandIdx >= eqBands_.size()) return;
    eqBands_[bandIdx].gainDb = gainDb;
    if (eqEnabled_) applyAudioFilters();
    emit eqChanged();
}

void HiFiEngine::resetEq() {
    for (auto& b : eqBands_) b.gainDb = 0.0;
    applyAudioFilters();
    emit eqChanged();
}

// ── 전체 설정 적용 ───────────────────────────────────────────────────
void HiFiEngine::applyAll() {
    if (!core_) return;

    if (bitPerfect_) {
        // 비트 퍼펙트: 샘플레이트 변환 없음, 원본 포맷 유지
        core_->setProperty("audio-samplerate", 0);
        core_->setProperty("audio-format", QString("float"));
        core_->setProperty("audio-channels", QString("auto"));
        core_->setProperty("gapless-audio", QString("yes"));
    }

    setReplayGain(rgMode_);
    applyAudioFilters();
}

void HiFiEngine::applyAudioFilters() {
    if (!core_) return;

    QString af = buildAfString();
    if (af.isEmpty())
        core_->setProperty("af", QString(""));
    else
        core_->setProperty("af", af);
}

QString HiFiEngine::buildAfString() const {
    QStringList filters;

    // 업샘플링 필터
    switch (resampleQ_) {
    case ResampleQuality::Linear:
        filters << "lavrresample:filter_size=16:phase_shift=8";
        break;
    case ResampleQuality::SoxHigh:
        filters << "lavrresample:filter_size=64:phase_shift=10";
        break;
    case ResampleQuality::SoxVeryHigh:
        filters << "lavrresample:filter_size=256:phase_shift=12";
        break;
    default: break;
    }

    // EQ 필터 (equalizer=f1/w1/g1:f2/w2/g2:...)
    if (eqEnabled_) {
        bool hasGain = false;
        for (const auto& b : eqBands_)
            if (std::abs(b.gainDb) > 0.01) { hasGain = true; break; }

        if (hasGain) {
            QStringList eqParts;
            for (const auto& b : eqBands_) {
                eqParts << QString("%1/%2/%3")
                    .arg(b.freqHz, 0, 'f', 0)
                    .arg(b.q, 0, 'f', 2)
                    .arg(b.gainDb, 0, 'f', 1);
            }
            filters << "equalizer=" + eqParts.join(":");
        }
    }

    return filters.join(",");
}

// ── 내장 EQ 프리셋 ───────────────────────────────────────────────────
QVector<EqPreset> HiFiEngine::builtinPresets() {
    QVector<EqPreset> presets;

    // Flat (기본)
    EqPreset flat;
    flat.name = "Flat";
    for (double f : {31.0, 63.0, 125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0, 16000.0})
        flat.bands.append({f, 0.0, 1.41});
    presets << flat;

    // Audiophile
    EqPreset audiophile;
    audiophile.name = "Audiophile";
    QVector<double> agains = {0, 0, 0, 0, -0.5, -0.5, 0, 0.5, 1.0, 1.5};
    for (int i = 0; i < 10; ++i)
        audiophile.bands.append({flat.bands[i].freqHz, agains[i], 1.41});
    presets << audiophile;

    // Bass Boost
    EqPreset bass;
    bass.name = "Bass Boost";
    QVector<double> bgains = {4, 3.5, 3, 2, 1, 0, 0, 0, 0, 0};
    for (int i = 0; i < 10; ++i)
        bass.bands.append({flat.bands[i].freqHz, bgains[i], 1.41});
    presets << bass;

    // Vocal
    EqPreset vocal;
    vocal.name = "Vocal";
    QVector<double> vgains = {-2, -1, 0, 1, 2, 3, 2, 1, 0, -1};
    for (int i = 0; i < 10; ++i)
        vocal.bands.append({flat.bands[i].freqHz, vgains[i], 1.41});
    presets << vocal;

    // Classical
    EqPreset classical;
    classical.name = "Classical";
    QVector<double> cgains = {0, 0, 0, 0, 0, 0, -1, -1, -1, -2};
    for (int i = 0; i < 10; ++i)
        classical.bands.append({flat.bands[i].freqHz, cgains[i], 1.41});
    presets << classical;

    // Rock
    EqPreset rock;
    rock.name = "Rock";
    QVector<double> rgains = {3, 2, 1, 0, -1, -1, 0, 1, 2, 3};
    for (int i = 0; i < 10; ++i)
        rock.bands.append({flat.bands[i].freqHz, rgains[i], 1.41});
    presets << rock;

    // Jazz
    EqPreset jazz;
    jazz.name = "Jazz";
    QVector<double> jgains = {2, 1, 0, 0, -1, -1, 0, 1, 2, 2};
    for (int i = 0; i < 10; ++i)
        jazz.bands.append({flat.bands[i].freqHz, jgains[i], 1.41});
    presets << jazz;

    return presets;
}
