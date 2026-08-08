#pragma once
#include <QWidget>
#include <QMap>
#include <QKeySequence>

class QTableWidget;
class QPushButton;
class QLabel;
class QLineEdit;
class QSettings;

/**
 * ShortcutConfigWidget - 단축키 커스터마이징 UI
 *
 * ProFeaturesWidget 탭에 추가되어 사용자가 단축키를 변경하고
 * QSettings에 JSON 형태로 저장할 수 있게 합니다.
 *
 * 기본 단축키 목록:
 *   Space  → 재생/일시정지
 *   Left   → 5초 뒤로
 *   Right  → 5초 앞으로
 *   Up     → 볼륨 +5
 *   Down   → 볼륨 -5
 *   F      → 전체화면 토글
 *   M      → 음소거
 *   Tab    → 미디어 정보 오버레이
 *   P      → 전문 기능 패널
 *   S      → 스크린샷
 *   Ctrl+O → 파일 열기
 *   Ctrl+U → URL 열기
 *   F5     → 설정
 *   ?      → 단축키 도움말
 */
struct ShortcutEntry {
    QString id;           // 고유 식별자 (저장 키)
    QString description;  // 사용자 표시 이름
    QKeySequence defaultKey;
    QKeySequence customKey;  // 비어 있으면 기본값 사용
};

class ShortcutConfigWidget : public QWidget {
    Q_OBJECT
public:
    explicit ShortcutConfigWidget(QWidget* parent = nullptr);

    // 현재 유효한 단축키 반환 (customKey 우선, 없으면 defaultKey)
    QKeySequence keyFor(const QString& id) const;

    // 외부에서 단축키 목록 조회 (MainWindow에서 eventFilter 적용용)
    const QList<ShortcutEntry>& entries() const { return entries_; }

signals:
    void shortcutsChanged();  // 단축키 변경 시 MainWindow에 알림

public slots:
    void loadSettings();
    void saveSettings();
    void resetAll();

private slots:
    void onCellDoubleClicked(int row, int col);
    void onResetRow();

private:
    void buildUI();
    void populateTable();
    void startCapture(int row);

    QTableWidget*         table_       = nullptr;
    QPushButton*          btnReset_    = nullptr;
    QPushButton*          btnSave_     = nullptr;
    QLabel*               lblStatus_   = nullptr;
    QList<ShortcutEntry>  entries_;
    int                   captureRow_  = -1;  // 현재 캡처 중인 행
    bool                  capturing_   = false;

protected:
    void keyPressEvent(QKeyEvent* e) override;
};
