#include "UrlDialog.h"
#include <QClipboard>
#include <QApplication>

UrlDialog::UrlDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("URL 열기");
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setFixedSize(520, 160);

    // 다크 테마
    setStyleSheet(
        "QDialog { background: #1a1a1a; border: 1px solid #333; border-radius: 6px; }"
        "QLabel  { color: #ccc; font-size: 12px; }"
        "QLineEdit { background: #111; color: #eee; border: 1px solid #333; "
        "            border-radius: 4px; padding: 6px 10px; font-size: 13px; }"
        "QLineEdit:focus { border-color: #4fc3f7; }"
        "QPushButton { background: #2a2a2a; color: #ddd; border: 1px solid #333; "
        "              border-radius: 4px; padding: 6px 18px; font-size: 12px; }"
        "QPushButton:hover { background: #333; border-color: #4fc3f7; color: #fff; }"
        "QPushButton#okBtn { background: #1565c0; border-color: #1976d2; color: #fff; }"
        "QPushButton#okBtn:hover { background: #1976d2; }"
    );

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(12);

    // 타이틀
    auto* titleLabel = new QLabel("URL 열기", this);
    titleLabel->setStyleSheet("color: #fff; font-size: 14px; font-weight: bold;");
    layout->addWidget(titleLabel);

    // 안내 텍스트
    auto* hintLabel = new QLabel(
        "유튜브, 트위치, 직접 스트림 URL을 입력하세요.\n"
        "유튜브 5.1 서라운드는 yt-dlp가 자동으로 처리합니다.", this);
    hintLabel->setStyleSheet("color: #888; font-size: 11px;");
    layout->addWidget(hintLabel);

    // URL 입력창
    urlEdit_ = new QLineEdit(this);
    urlEdit_->setPlaceholderText("https://www.youtube.com/watch?v=...");
    // 클립보드에 URL이 있으면 자동 붙여넣기
    QString clip = QApplication::clipboard()->text().trimmed();
    if (clip.startsWith("http://") || clip.startsWith("https://") ||
        clip.startsWith("rtmp://") || clip.startsWith("rtsp://")) {
        urlEdit_->setText(clip);
        urlEdit_->selectAll();
    }
    layout->addWidget(urlEdit_);

    // 버튼
    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    cancelBtn_ = new QPushButton("취소", this);
    okBtn_     = new QPushButton("재생", this);
    okBtn_->setObjectName("okBtn");
    okBtn_->setDefault(true);
    btnLayout->addWidget(cancelBtn_);
    btnLayout->addWidget(okBtn_);
    layout->addLayout(btnLayout);

    connect(okBtn_,     &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelBtn_, &QPushButton::clicked, this, &QDialog::reject);
    connect(urlEdit_, &QLineEdit::returnPressed, this, &QDialog::accept);

    urlEdit_->setFocus();
}
