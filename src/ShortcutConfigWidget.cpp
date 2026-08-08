#include "ShortcutConfigWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QPushButton>
#include <QLabel>
#include <QKeyEvent>
#include <QSettings>
#include <QMessageBox>
#include <QApplication>
#include <QFocusEvent>

static const char* STYLE = R"(
QWidget {
    background: #141414;
    color: #e0e0e0;
    font-size: 12px;
}
QTableWidget {
    background: #1a1a1a;
    gridline-color: #2a2a2a;
    border: 1px solid #2a2a2a;
    border-radius: 4px;
    selection-background-color: #00c8b4;
    selection-color: #000;
}
QTableWidget::item {
    padding: 6px 10px;
    border-bottom: 1px solid #222;
}
QTableWidget::item:selected {
    background: #00c8b4;
    color: #000;
}
QHeaderView::section {
    background: #1e1e1e;
    color: #888;
    padding: 6px 10px;
    border: none;
    border-bottom: 1px solid #333;
    font-size: 11px;
}
QPushButton#btnSave {
    background: #00c8b4;
    color: #000;
    border: none;
    border-radius: 4px;
    padding: 7px 20px;
    font-weight: 700;
    font-size: 12px;
}
QPushButton#btnSave:hover { background: #00e5cc; }
QPushButton#btnReset {
    background: #2a2a2a;
    color: #aaa;
    border: 1px solid #444;
    border-radius: 4px;
    padding: 7px 16px;
    font-size: 12px;
}
QPushButton#btnReset:hover { background: #333; color: #fff; }
QLabel#lblCapture {
    color: #00c8b4;
    font-size: 11px;
}
QLabel#lblStatus {
    color: #666;
    font-size: 11px;
}
)";

ShortcutConfigWidget::ShortcutConfigWidget(QWidget* parent)
    : QWidget(parent)
{
    // 기본 단축키 목록 초기화
    entries_ = {
        {"play_pause",   "재생 / 일시정지",       QKeySequence(Qt::Key_Space),         {}},
        {"seek_back5",   "5초 뒤로",              QKeySequence(Qt::Key_Left),          {}},
        {"seek_fwd5",    "5초 앞으로",            QKeySequence(Qt::Key_Right),         {}},
        {"seek_back60",  "60초 뒤로",             QKeySequence(Qt::SHIFT | Qt::Key_Left), {}},
        {"seek_fwd60",   "60초 앞으로",           QKeySequence(Qt::SHIFT | Qt::Key_Right), {}},
        {"vol_up",       "볼륨 +5",               QKeySequence(Qt::Key_Up),            {}},
        {"vol_down",     "볼륨 -5",               QKeySequence(Qt::Key_Down),          {}},
        {"fullscreen",   "전체화면 토글",          QKeySequence(Qt::Key_F),             {}},
        {"mute",         "음소거",                QKeySequence(Qt::Key_M),             {}},
        {"media_info",   "미디어 정보 오버레이",   QKeySequence(Qt::Key_Tab),           {}},
        {"pro_panel",    "전문 기능 패널",         QKeySequence(Qt::Key_P),             {}},
        {"screenshot",   "스크린샷",              QKeySequence(Qt::Key_S),             {}},
        {"open_file",    "파일 열기",             QKeySequence(Qt::CTRL | Qt::Key_O),  {}},
        {"open_url",     "URL 열기",              QKeySequence(Qt::CTRL | Qt::Key_U),  {}},
        {"settings",     "설정",                  QKeySequence(Qt::Key_F5),            {}},
        {"shortcut_help","단축키 도움말",          QKeySequence(Qt::Key_Question),      {}},
        {"playlist_prev","이전 곡",               QKeySequence(Qt::Key_PageUp),        {}},
        {"playlist_next","다음 곡",               QKeySequence(Qt::Key_PageDown),      {}},
        {"speed_up",     "재생 속도 +0.1x",       QKeySequence(Qt::Key_BracketRight),  {}},
        {"speed_down",   "재생 속도 -0.1x",       QKeySequence(Qt::Key_BracketLeft),   {}},
        {"screen_record","화면 녹화 시작/중지",    QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_R), {}},
    };

    buildUI();
    loadSettings();
}

void ShortcutConfigWidget::buildUI() {
    setStyleSheet(STYLE);
    setFocusPolicy(Qt::StrongFocus);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 12, 16, 12);
    root->setSpacing(10);

    // 헤더
    auto* header = new QLabel("단축키 커스터마이징");
    header->setStyleSheet("color:#e0e0e0; font-size:14px; font-weight:700;");
    root->addWidget(header);

    auto* desc = new QLabel("항목을 더블클릭하면 새 단축키를 입력할 수 있습니다. 기본값으로 되돌리려면 Delete 키를 누르세요.");
    desc->setStyleSheet("color:#666; font-size:11px;");
    desc->setWordWrap(true);
    root->addWidget(desc);

    // 캡처 상태 레이블
    auto* lblCapture = new QLabel("");
    lblCapture->setObjectName("lblCapture");
    lblCapture->setAlignment(Qt::AlignCenter);
    root->addWidget(lblCapture);

    // 테이블
    table_ = new QTableWidget(this);
    table_->setColumnCount(3);
    table_->setHorizontalHeaderLabels({"기능", "현재 단축키", "기본값"});
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table_->verticalHeader()->setVisible(false);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAlternatingRowColors(false);
    root->addWidget(table_, 1);

    connect(table_, &QTableWidget::cellDoubleClicked,
            this, &ShortcutConfigWidget::onCellDoubleClicked);

    // 버튼 행
    auto* btnRow = new QHBoxLayout;
    btnRow->setSpacing(8);

    lblStatus_ = new QLabel("변경 사항이 있으면 저장 버튼을 누르세요.");
    lblStatus_->setObjectName("lblStatus");
    btnRow->addWidget(lblStatus_, 1);

    btnReset_ = new QPushButton("전체 초기화");
    btnReset_->setObjectName("btnReset");
    btnReset_->setFocusPolicy(Qt::NoFocus);
    connect(btnReset_, &QPushButton::clicked, this, &ShortcutConfigWidget::resetAll);
    btnRow->addWidget(btnReset_);

    btnSave_ = new QPushButton("저장");
    btnSave_->setObjectName("btnSave");
    btnSave_->setFocusPolicy(Qt::NoFocus);
    connect(btnSave_, &QPushButton::clicked, this, &ShortcutConfigWidget::saveSettings);
    btnRow->addWidget(btnSave_);

    root->addLayout(btnRow);

    // 캡처 레이블 참조 저장
    // (동적으로 찾아서 업데이트)
}

void ShortcutConfigWidget::populateTable() {
    table_->setRowCount(entries_.size());
    for (int i = 0; i < entries_.size(); ++i) {
        const auto& e = entries_[i];
        // 기능 이름
        auto* nameItem = new QTableWidgetItem(e.description);
        nameItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        table_->setItem(i, 0, nameItem);

        // 현재 단축키
        bool isCustom = !e.customKey.isEmpty();
        QKeySequence displayKey = isCustom ? e.customKey : e.defaultKey;
        auto* keyItem = new QTableWidgetItem(displayKey.toString(QKeySequence::NativeText));
        keyItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        if (isCustom) {
            keyItem->setForeground(QColor(0x00, 0xc8, 0xb4));  // 민트: 커스텀 단축키
        } else {
            keyItem->setForeground(QColor(0x88, 0x88, 0x88));  // 회색: 기본값
        }
        table_->setItem(i, 1, keyItem);

        // 기본값
        auto* defItem = new QTableWidgetItem(e.defaultKey.toString(QKeySequence::NativeText));
        defItem->setFlags(Qt::ItemIsEnabled);
        defItem->setForeground(QColor(0x44, 0x44, 0x44));
        table_->setItem(i, 2, defItem);
    }
}

void ShortcutConfigWidget::loadSettings() {
    QSettings s("GaonCommunication", "Sorinuri");
    s.beginGroup("shortcuts");
    for (auto& e : entries_) {
        QString saved = s.value(e.id, "").toString();
        if (!saved.isEmpty()) {
            e.customKey = QKeySequence(saved);
        } else {
            e.customKey = QKeySequence();
        }
    }
    s.endGroup();
    populateTable();
}

void ShortcutConfigWidget::saveSettings() {
    QSettings s("GaonCommunication", "Sorinuri");
    s.beginGroup("shortcuts");
    for (const auto& e : entries_) {
        if (!e.customKey.isEmpty()) {
            s.setValue(e.id, e.customKey.toString());
        } else {
            s.remove(e.id);
        }
    }
    s.endGroup();
    s.sync();

    if (lblStatus_) lblStatus_->setText("✅ 단축키 저장 완료. 다음 실행부터 적용됩니다.");
    emit shortcutsChanged();
}

void ShortcutConfigWidget::resetAll() {
    for (auto& e : entries_) {
        e.customKey = QKeySequence();
    }
    populateTable();
    if (lblStatus_) lblStatus_->setText("전체 단축키가 기본값으로 초기화되었습니다. 저장 버튼을 눌러 적용하세요.");
}

void ShortcutConfigWidget::onCellDoubleClicked(int row, int /*col*/) {
    startCapture(row);
}

void ShortcutConfigWidget::startCapture(int row) {
    if (row < 0 || row >= entries_.size()) return;
    captureRow_ = row;
    capturing_ = true;

    // 캡처 상태 표시
    auto* lbl = findChild<QLabel*>("lblCapture");
    if (lbl) {
        lbl->setText(QString("⌨  '%1' 단축키 입력 중... (ESC: 취소, Delete: 기본값으로 초기화)")
                     .arg(entries_[row].description));
    }

    // 테이블 해당 행 강조
    auto* keyItem = table_->item(row, 1);
    if (keyItem) {
        keyItem->setText("키를 누르세요...");
        keyItem->setForeground(QColor(0xff, 0xd0, 0x00));
    }

    setFocus();
}

void ShortcutConfigWidget::keyPressEvent(QKeyEvent* e) {
    if (!capturing_ || captureRow_ < 0) {
        QWidget::keyPressEvent(e);
        return;
    }

    auto* lbl = findChild<QLabel*>("lblCapture");

    if (e->key() == Qt::Key_Escape) {
        // 취소: 원래 값 복원
        capturing_ = false;
        populateTable();
        if (lbl) lbl->setText("");
        captureRow_ = -1;
        return;
    }

    if (e->key() == Qt::Key_Delete) {
        // 기본값으로 초기화
        entries_[captureRow_].customKey = QKeySequence();
        capturing_ = false;
        populateTable();
        if (lbl) lbl->setText("");
        if (lblStatus_) lblStatus_->setText("기본값으로 초기화했습니다. 저장 버튼을 눌러 적용하세요.");
        captureRow_ = -1;
        return;
    }

    // 수정자 키만 눌린 경우 무시
    if (e->key() == Qt::Key_Control || e->key() == Qt::Key_Shift ||
        e->key() == Qt::Key_Alt || e->key() == Qt::Key_Meta) {
        return;
    }

    // 새 단축키 생성
    Qt::KeyboardModifiers mods = e->modifiers();
    int key = e->key();
    QKeySequence newKey(static_cast<int>(mods) | key);

    // 중복 검사
    for (int i = 0; i < entries_.size(); ++i) {
        if (i == captureRow_) continue;
        QKeySequence existing = entries_[i].customKey.isEmpty()
                                ? entries_[i].defaultKey
                                : entries_[i].customKey;
        if (existing == newKey) {
            if (lblStatus_) {
                lblStatus_->setText(
                    QString("⚠ '%1' 키는 이미 '%2'에 사용 중입니다.")
                    .arg(newKey.toString(QKeySequence::NativeText))
                    .arg(entries_[i].description));
            }
            // 중복이지만 그냥 허용 (사용자 선택)
        }
    }

    entries_[captureRow_].customKey = newKey;
    capturing_ = false;
    populateTable();
    if (lbl) lbl->setText("");
    if (lblStatus_) lblStatus_->setText(
        QString("'%1' 단축키가 '%2'로 변경되었습니다. 저장 버튼을 눌러 적용하세요.")
        .arg(entries_[captureRow_].description)
        .arg(newKey.toString(QKeySequence::NativeText)));
    captureRow_ = -1;
    e->accept();
}

QKeySequence ShortcutConfigWidget::keyFor(const QString& id) const {
    for (const auto& e : entries_) {
        if (e.id == id) {
            return e.customKey.isEmpty() ? e.defaultKey : e.customKey;
        }
    }
    return QKeySequence();
}
