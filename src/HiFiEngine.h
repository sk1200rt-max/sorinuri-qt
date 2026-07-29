#pragma once
#include <QObject>
#include <QString>
#include <QVector>
#include <QPair>

class MpvCore;

// EQ 밴드 (10밴드 파라메트릭)
struct EqBand {
    double freqHz;   // 중심 주파수
    double gainDb;   // 게인 (dB)
    double q;        // Q 팩터
};

// EQ 프리셋
struct EqPreset {
    QString         name;
    QVector<EqBand> bands;
};

// 업샘플링 품질
enum class ResampleQuality {
    None,       // 비트 퍼펙트 (기본)
    Linear,     // 선형 보간
    SoxHigh,    // SoX High Quality
    SoxVeryHigh // SoX Very High Quality
};

class HiFiEngine : public QObject {
    Q_OBJECT
public:
    explicit HiFiEngine(MpvCore* core, QObject* parent = nullptr);

    // ── 비트 퍼펙트 모드 ───────────────────────────────────────────
    void setBitPerfect(bool enable);
    bool isBitPerfect() const { return bitPerfect_; }

    // ── ReplayGain ─────────────────────────────────────────────────
    enum class ReplayGainMode { Off, Track, Album };
    void setReplayGain(ReplayGainMode mode);
    ReplayGainMode replayGainMode() const { return rgMode_; }

    // ── 업샘플링 ───────────────────────────────────────────────────
    void setResampleQuality(ResampleQuality q);
    ResampleQuality resampleQuality() const { return resampleQ_; }

    // ── EQ ─────────────────────────────────────────────────────────
    void setEqEnabled(bool enable);
    bool isEqEnabled() const { return eqEnabled_; }
    void applyEqPreset(const EqPreset& preset);
    void setEqBand(int bandIdx, double gainDb);
    void resetEq();

    // 내장 프리셋 목록
    static QVector<EqPreset> builtinPresets();

    // ── 현재 설정을 MPV에 적용 ─────────────────────────────────────
    void applyAll();

    // ── 음악 파일 감지 ─────────────────────────────────────────────
    static bool isMusicFile(const QString& path);
    static QStringList musicExtensions();

signals:
    void bitPerfectChanged(bool enabled);
    void eqChanged();

private:
    void applyAudioFilters();
    QString buildAfString() const;

    MpvCore*        core_       = nullptr;
    bool            bitPerfect_ = true;
    ReplayGainMode  rgMode_     = ReplayGainMode::Track;
    ResampleQuality resampleQ_  = ResampleQuality::None;
    bool            eqEnabled_  = false;
    QVector<EqBand> eqBands_;
};
