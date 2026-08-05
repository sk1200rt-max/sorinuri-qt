#pragma once
#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QPropertyAnimation>
#include <QScrollArea>
#include <QRect>

class MpvCore;

// Tab키로 토글되는 미디어 정보 오버레이 패널
// 랜딩페이지 프리뷰 사이드바와 동일한 디자인:
//   - 탭 목록 (재생목록/오디오/화질/자막/전문 기능)
//   - 현재 오디오 카드 (원본 코덱, 출력, WASAPI, 비트퍼펙트)
//   - 현재 영상 카드 (코덱, 해상도, 색공간, HDR)
class MediaInfoOverlay : public QWidget {
    Q_OBJECT
public:
    explicit MediaInfoOverlay(QWidget* parent = nullptr);

    void connectMpv(MpvCore* core);
    void toggle();
    bool isOpen() const { return isVisible(); }

public slots:
    void updateAudioInfo(const QString& codec, int channels, int sampleRate,
                         const QString& output);
    void updateVideoInfo(int width, int height, double fps, const QString& codec);
    void onFileLoaded(const QString& path);

private:
    void buildUi();
    void applyStyle();
    QString formatChannels(int ch) const;
    QString formatCodecBadge(const QString& codec) const;

    MpvCore* core_ = nullptr;

    // 탭 목록 라벨들
    QLabel* tabPlaylist_   = nullptr;
    QLabel* tabAudio_      = nullptr;
    QLabel* tabVideo_      = nullptr;
    QLabel* tabSubtitle_   = nullptr;
    QLabel* tabPro_        = nullptr;

    // 현재 오디오 카드
    QLabel* audioBadge_    = nullptr;
    QLabel* audioOriginal_ = nullptr;
    QLabel* audioOutput_   = nullptr;
    QLabel* audioWasapi_   = nullptr;
    QLabel* audioBitperf_  = nullptr;

    // 현재 영상 카드
    QLabel* videoBadge_    = nullptr;
    QLabel* videoOriginal_ = nullptr;
    QLabel* videoOutput_   = nullptr;
    QLabel* videoColor_    = nullptr;
    QLabel* videoHdr_      = nullptr;

    QPropertyAnimation* anim_ = nullptr;
};
