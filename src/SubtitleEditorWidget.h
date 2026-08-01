#pragma once
#include <QWidget>
#include <QVector>

class QPlainTextEdit;
class QLabel;
class QSlider;
class QPushButton;
class QSpinBox;
class MpvCore;

// 자막 편집기 위젯
// - 현재 로드된 자막 파일 내용 표시 및 직접 편집
// - 싱크 오프셋 조절 (ms 단위)
// - 편집 내용 임시 파일로 저장 후 MPV에 재로드
class SubtitleEditorWidget : public QWidget {
    Q_OBJECT
public:
    explicit SubtitleEditorWidget(MpvCore* core, QWidget* parent = nullptr);

    // 현재 자막 파일 경로 설정 (MainWindow에서 파일 로드 시 호출)
    void setSubtitleFile(const QString& path);

public slots:
    void onSyncOffsetChanged(int ms);
    void onApplyEdit();
    void onReloadSubtitle();

signals:
    void subtitleReloaded(const QString& path);

private:
    void buildUI();
    void loadFile(const QString& path);

    MpvCore*        core_       = nullptr;
    QString         filePath_;
    int             syncOffset_ = 0;  // ms

    QPlainTextEdit* editor_     = nullptr;
    QLabel*         lblFile_    = nullptr;
    QLabel*         lblSync_    = nullptr;
    QSpinBox*       spinSync_   = nullptr;
    QPushButton*    btnApply_   = nullptr;
    QPushButton*    btnReload_  = nullptr;
    QPushButton*    btnSave_    = nullptr;
};
