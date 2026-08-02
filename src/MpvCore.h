#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QTimer>
#include <QWindow>
#include <mpv/client.h>

/**
 * MpvCore - libmpv 래퍼
 *
 * libmpv를 Qt 시그널/슬롯 시스템과 연동합니다.
 * 비디오 렌더링은 MpvWidget이 담당합니다.
 */

// ── GPU 렌더링 성능 프로파일 ──────────────────────────────────────
// GPU 성능에 따라 자동 또는 수동으로 선택
// Eco: 노트북 통합 GPU / Balanced: 중급 GPU / Quality: 고급 GPU / HiEnd: 전문가
enum class RenderProfile {
    Eco,        // 절전 - 노트북 통합 GPU (Intel Iris/UHD)
    Balanced,   // 균형 - 중급 GPU (GTX 1060 / RX 580 수준)
    Quality,    // 화질 - 고급 GPU (RTX 3060 / RX 6700 수준) ← 기본값
    HiEnd       // 최고화질 - 전문가용 (RTX 4080 / RX 7900 수준)
};

class MpvCore : public QObject {
    Q_OBJECT

public:
    explicit MpvCore(QObject* parent = nullptr);
    ~MpvCore() override;

    // 초기화 (WID 설정 후 호출)
    bool initialize(WId windowId);

    // 재생 제어
    void loadFile(const QString& path, bool append = false);
    void play();
    void pause();
    void togglePause();
    void stop();
    void seek(double seconds, bool relative = false);
    void seekPercent(double percent);

    // 속성
    void setVolume(int vol);
    void setMuted(bool muted);
    void setSpeed(double speed);
    void setSubtitleTrack(int id);
    void setAudioTrack(int id);
    void setVideoTrack(int id) { if (initialized_) { int64_t v=id; mpv_set_property_async(mpv_,0,"vid",MPV_FORMAT_INT64,&v); } }

    // 오디오 설정
    void setAudioDevice(const QString& device);
    void setAudioExclusive(bool exclusive);
    void setAudioPassthrough(bool passthrough);
    void setSpdifCodecs(const QStringList& codecs);
    QVariantList audioDeviceList() const;             // {name, description} 목록
    bool deviceLikelySupportsPassthrough() const;      // 패스스루 지원 사전 감지

    // 비디오 설정
    void setHwdec(const QString& method);
    void setVideoOutput(const QString& vo);
    void setMotionSmoothing(bool enabled);  // 모션 스무딩 (프레임 보간)

    // ── GPU 렌더링 최적화 ────────────────────────────────────────
    void applyRenderProfile(RenderProfile profile);  // 성능 프로파일 적용
    RenderProfile currentRenderProfile() const { return renderProfile_; }
    void applyVideoSyncByFps(double fps);  // FPS 기반 video-sync 자동 전환
    void tryGpuNext();                     // gpu-next 안전 전환 (실패 시 gpu 폴백)

    // 상태 조회
    double duration() const;
    double position() const;
    int    volume() const;
    bool   isPaused() const;
    bool   isMuted() const;
    QString currentFile() const;

    // 트랙 정보
    QVariantList audioTracks() const;
    QVariantList videoTracks() const;
    QVariantList subtitleTracks() const;

    // 저수준 MPV 접근
    mpv_handle* handle() const { return mpv_; }

    // 속성 직접 설정
    void setProperty(const QString& name, const QVariant& value);
    QVariant getProperty(const QString& name) const;
    void command(const QStringList& args);

signals:
    void fileLoaded(const QString& path);
    void playbackStarted();
    void playbackPaused();
    void playbackStopped();
    void playbackEnded();
    void positionChanged(double seconds);
    void durationChanged(double seconds);
    void volumeChanged(int vol);
    void tracksChanged();
    void audioFormatChanged(const QString& codec, int channels, int sampleRate, const QString& output);
    void videoInfoChanged(int width, int height, double fps, const QString& codec);
    void errorOccurred(const QString& message);
    void renderProfileChanged(RenderProfile profile);  // 프로파일 변경 알림

private slots:
    void onMpvEvents();

private:
    void handleEvent(mpv_event* event);
    void handlePropertyChange(mpv_event_property* prop);
    static void wakeupCallback(void* ctx);

    mpv_handle* mpv_ = nullptr;
    QTimer*     eventTimer_ = nullptr;
    bool        initialized_ = false;
    // 사용자 패스스루 설정 상태 (장치 사전 감지 시 복원용)
    bool    passthroughEnabled_ = true;
    QString spdifCodecs_ = QStringLiteral("ac3,eac3,dts,dts-hd,truehd");

    // GPU 렌더링 상태
    RenderProfile renderProfile_ = RenderProfile::Quality;  // 기본값: Quality
    bool          gpuNextActive_ = false;  // gpu-next 활성화 여부
};
