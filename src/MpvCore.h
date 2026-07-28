#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QTimer>
#include <mpv/client.h>

/**
 * MpvCore - libmpv 래퍼
 *
 * libmpv를 Qt 시그널/슬롯 시스템과 연동합니다.
 * 비디오 렌더링은 MpvWidget이 담당합니다.
 */
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
    void setVideoTrack(int id);

    // 오디오 설정
    void setAudioDevice(const QString& device);
    void setAudioExclusive(bool exclusive);
    void setAudioPassthrough(bool passthrough);
    void setSpdifCodecs(const QStringList& codecs);

    // 비디오 설정
    void setHwdec(const QString& method);
    void setVideoOutput(const QString& vo);

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

private slots:
    void onMpvEvents();

private:
    void handleEvent(mpv_event* event);
    void handlePropertyChange(mpv_event_property* prop);
    static void wakeupCallback(void* ctx);

    mpv_handle* mpv_ = nullptr;
    QTimer*     eventTimer_ = nullptr;
    bool        initialized_ = false;
};
