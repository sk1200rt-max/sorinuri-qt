#pragma once
#include <QWidget>
#include <QComboBox>
#include <QLabel>
#include <QHBoxLayout>
#include "MpvCore.h"

/**
 * TrackSelector - 오디오/자막/비디오 트랙 선택 위젯
 * 컨트롤바에 통합되어 현재 트랙 정보를 표시하고 변경 가능
 */
class TrackSelector : public QWidget {
    Q_OBJECT
public:
    explicit TrackSelector(QWidget* parent = nullptr);
    void connectMpv(MpvCore* core);
    void refresh();

private slots:
    void onAudioTrackChanged(int index);
    void onSubTrackChanged(int index);
    void onTracksChanged();

private:
    QComboBox* audioCombo_ = nullptr;
    QComboBox* subCombo_   = nullptr;
    MpvCore*   mpv_        = nullptr;
    bool       updating_   = false;
};
