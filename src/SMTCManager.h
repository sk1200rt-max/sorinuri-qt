#pragma once
#include <QObject>
#include <QString>
#include <QPixmap>
#include <QTimer>

/**
 * SMTCManager - Windows System Media Transport Controls 연동
 *
 * Windows 잠금 화면 / 알림 센터에 재생 중인 미디어 정보를 표시합니다.
 * - 제목, 아티스트, 앨범명 표시
 * - 앨범아트 썸네일 표시
 * - 재생/일시정지/이전/다음 버튼 제어
 * - 타임라인(재생 위치/총 길이) 표시
 *
 * 구현 방식: ISystemMediaTransportControlsInterop::GetForWindow()
 * Win32 데스크탑 앱에서 WinRT SMTC를 사용하기 위한 COM 인터페이스
 * 참고: https://learn.microsoft.com/en-us/windows/win32/api/systemmediatransportcontrolsinterop/
 */
class SMTCManager : public QObject {
    Q_OBJECT
public:
    explicit SMTCManager(QObject* parent = nullptr);
    ~SMTCManager() override;

    // HWND로 초기화 (MainWindow::winId() 전달)
    bool initialize(void* hwnd);

    // 미디어 정보 업데이트
    void updateMetadata(const QString& title,
                        const QString& artist,
                        const QString& album,
                        const QPixmap& albumArt = QPixmap());

    // 재생 상태 업데이트
    void setPlaying(bool playing);
    void setStopped();

    // 타임라인 업데이트 (초 단위)
    void updateTimeline(double position, double duration);

    // SMTC 활성화/비활성화
    void setEnabled(bool enabled);
    bool isEnabled() const { return enabled_; }

signals:
    void playRequested();
    void pauseRequested();
    void stopRequested();
    void nextRequested();
    void previousRequested();
    void seekRequested(double positionSeconds);

private:
    bool    enabled_     = false;
    bool    initialized_ = false;
    void*   smtcPtr_     = nullptr;  // ISystemMediaTransportControls* (void*로 헤더 의존성 제거)
    QTimer* timelineTimer_ = nullptr;
    double  currentPos_       = 0.0;
    double  currentDur_       = 0.0;
    QString albumArtTempPath_;  // 임시 앉범아트 파일 경로

    void cleanup();
};
