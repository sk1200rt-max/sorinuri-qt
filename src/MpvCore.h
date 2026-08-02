#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QTimer>
#include <QWindow>
#include <mpv/client.h>
#include "RenderEnvironment.h"

/**
 * MpvCore - libmpv 래퍼
 *
 * libmpv를 Qt 시그널/슬롯 시스템과 연동합니다.
 * 비디오 렌더링은 MpvWidget이 담당합니다.
 */

// ── GPU 렌더링 성능 프로파일 ──────────────────────────────────────
enum class RenderProfile {
    Eco,        // 절전 - 통합 GPU (Intel Iris/UHD, AMD APU)
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
    void setVideoTrack(int id) {
        if (initialized_) {
            int64_t v = id;
            mpv_set_property_async(mpv_, 0, "vid", MPV_FORMAT_INT64, &v);
        }
    }

    // 오디오 설정
    void setAudioDevice(const QString& device);
    void setAudioExclusive(bool exclusive);
    void setAudioPassthrough(bool passthrough);
    void setSpdifCodecs(const QStringList& codecs);
    QVariantList audioDeviceList() const;
    bool deviceLikelySupportsPassthrough() const;

    // 비디오 설정
    void setHwdec(const QString& method);
    // setVideoOutput/tryGpuNext 사용 금지: vo 변경 시 영상이 별도 창으로 분리됨
    // vo=libmpv는 초기화 시 한 번만 설정하며 이후 절대 변경하지 않음
    void setVideoOutput(const QString& vo);  // 내부 호환성 유지용 (외부 호출 금지)
    void setMotionSmoothing(bool enabled);

    // ── GPU 렌더링 최적화 ────────────────────────────────────────
    void applyRenderProfile(RenderProfile profile);
    RenderProfile currentRenderProfile() const { return renderProfile_; }

    // FPS + 모니터 주사율 기반 정밀 video-sync 자동 전환
    void applyVideoSyncByFps(double fps);

    // OpenGL 컨텍스트 준비 후 GPU 벤더 감지 재실행 및 설정 재적용
    // MpvWidget::initializeGL()에서 호출
    void redetectGpuAndApply();

    // tryGpuNext: 사용 금지 (vo 변경으로 인한 창 분리 문제)
    // void tryGpuNext();  // DISABLED

    // 영상 해상도 기반 업스케일 알고리즘 최적화
    // FILE_LOADED 이벤트에서 video-params/w, video-params/h 확인 후 호출
    void optimizeScaleForContent(int videoW, int videoH);

    // 코덱 기반 최적 hwdec 자동 선택 및 적용
    // FILE_LOADED 이벤트에서 video-codec 확인 후 호출
    void applyOptimalHwdec(const QString& codec);

    // 상태 조회
    double  duration() const;
    double  position() const;
    int     volume() const;
    bool    isPaused() const;
    bool    isMuted() const;
    QString currentFile() const;

    // 트랙 정보
    QVariantList audioTracks() const;
    QVariantList videoTracks() const;
    QVariantList subtitleTracks() const;

    // 저수준 MPV 접근
    mpv_handle* handle() const { return mpv_; }

    // 속성 직접 설정
    void     setProperty(const QString& name, const QVariant& value);
    QVariant getProperty(const QString& name) const;
    void     command(const QStringList& args);

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
    void audioFormatChanged(const QString& codec, int channels, int sampleRate,
                            const QString& output);
    void videoInfoChanged(int width, int height, double fps, const QString& codec);
    void errorOccurred(const QString& message);
    void renderProfileChanged(RenderProfile profile);

    // 실시간 품질 자동 강등/복원 알림 (UI 표시용)
    void renderQualityDegraded(const QString& reason);
    void renderQualityRestored();

private slots:
    void onMpvEvents();
    void onFrameDropCheck();  // 실시간 프레임 드롭 모니터링 타이머

private:
    void handleEvent(mpv_event* event);
    void handlePropertyChange(mpv_event_property* prop);
    static void wakeupCallback(void* ctx);

    mpv_handle* mpv_       = nullptr;
    QTimer*     eventTimer_ = nullptr;
    bool        initialized_ = false;

    // 오디오 설정 상태
    bool    passthroughEnabled_ = true;
    QString spdifCodecs_ = QStringLiteral("ac3,eac3,dts,dts-hd,truehd");

    // GPU 렌더링 상태
    RenderProfile renderProfile_ = RenderProfile::Quality;
    bool          gpuNextActive_ = false;
    RenderEnvInfo renderEnv_;              // 감지된 환경 정보

    // 실시간 프레임 드롭 모니터링
    QTimer*  frameDropTimer_    = nullptr; // 3초 주기 체크
    int      frameDropCount_    = 0;       // 누적 드롭 카운트
    int      prevFrameDropCount_= 0;       // 이전 체크 시점 드롭 카운트
    int      dropEventCount_    = 0;       // 연속 드롭 이벤트 횟수
    int      normalEventCount_  = 0;       // 연속 정상 이벤트 횟수
    bool     qualityDegraded_   = false;   // 품질 강등 상태
    QString  originalScale_;               // 강등 전 원래 scale 알고리즘
    bool     debandOriginal_    = true;    // 강등 전 deband 상태

    // 현재 재생 중인 영상 정보
    int     currentVideoW_ = 0;
    int     currentVideoH_ = 0;
    double  currentFps_    = 0.0;
    QString currentCodec_;
};
