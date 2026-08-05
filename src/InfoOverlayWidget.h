#pragma once
#include <QWidget>
#include <QLabel>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include "MpvCore.h"

/**
 * InfoOverlayWidget - 비최대화 모드에서 영상 영역에 표시되는 재생 정보 대시보드
 *
 * 스크린샷의 우상단 녹색 통계 패널과 달리, 전체 영상 영역을 활용하여
 * 영상/오디오/시스템 상태를 카드 레이아웃으로 표시합니다.
 *
 * 표시 조건:
 *   - 창이 최대화/전체화면이 아닐 때
 *   - 파일이 재생 중일 때 (재생 없으면 로고 + 안내 표시)
 *
 * 구현 방식:
 *   - MpvWidget의 자식 위젯으로 생성 (오버레이)
 *   - WA_TransparentForMouseEvents: 마우스 이벤트 통과 (영상 클릭 방해 안 함)
 *   - 1초 타이머로 실시간 통계 업데이트
 */
class InfoOverlayWidget : public QWidget {
    Q_OBJECT
public:
    explicit InfoOverlayWidget(MpvCore* core, QWidget* parent = nullptr);

    // 표시/숨김 제어 (MainWindow에서 창 상태 변경 시 호출)
    void setVisible(bool visible) override;

    // 재생 상태 업데이트 (파일 로드/정지 시 호출)
    void onFileLoaded(const QString& path);
    void onPlaybackStopped();

    // 오디오/영상 정보 업데이트
    void onAudioFormatChanged(const QString& codec, int channels, int sampleRate,
                              const QString& output);
    void onVideoInfoChanged(int width, int height, double fps, const QString& codec);

public slots:
    void refresh();  // 1초 타이머 슬롯 - 실시간 통계 갱신

protected:
    void resizeEvent(QResizeEvent* e) override;
    void paintEvent(QPaintEvent* e) override;

private:
    void buildLayout();
    void buildIdleLayout();
    void buildPlayingLayout();
    void clearLayout();

    // 카드 위젯 생성 헬퍼
    QFrame* makeCard(const QString& title, QWidget* content, const QString& accentColor = "#00c8b4");
    QLabel* makeValueLabel(const QString& text, int fontSize = 18, bool bold = true);
    QLabel* makeLabelSmall(const QString& text);

    // 상태 업데이트 헬퍼
    void updateVideoCard();
    void updateAudioCard();
    void updateStatsCard();
    void updateProgressCard();

    MpvCore*  core_       = nullptr;
    QTimer*   refreshTimer_ = nullptr;
    bool      hasFile_    = false;

    // 현재 파일 정보 (캐시)
    QString   currentFile_;
    QString   videoCodec_;
    int       videoW_ = 0, videoH_ = 0;
    double    videoFps_ = 0.0;
    QString   audioCodec_;
    int       audioChannels_ = 0;
    int       audioSampleRate_ = 0;
    QString   audioOutput_;

    // 실시간 통계 레이블 (refresh()에서 업데이트)
    QLabel*   lblDropped_  = nullptr;
    QLabel*   lblAvSync_   = nullptr;
    QLabel*   lblBitrate_  = nullptr;
    QLabel*   lblPosition_ = nullptr;
    QLabel*   lblDuration_ = nullptr;
    QLabel*   lblHwdec_    = nullptr;
    QLabel*   lblResolution_ = nullptr;
    QLabel*   lblFps_      = nullptr;
    QLabel*   lblAudioCodec_ = nullptr;
    QLabel*   lblChannels_ = nullptr;
    QLabel*   lblSampleRate_ = nullptr;
    QLabel*   lblAudioOut_ = nullptr;
    QLabel*   lblFileName_ = nullptr;
    QLabel*   lblVolume_   = nullptr;

    // 메인 레이아웃 컨테이너
    QWidget*  container_   = nullptr;
};
