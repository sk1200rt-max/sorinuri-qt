#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>

/**
 * UrlDialog - URL 입력 다이얼로그
 *
 * 유튜브, 트위치, 직접 스트림 URL 등을 입력받아 재생합니다.
 * yt-dlp를 통해 유튜브 5.1 서라운드 오디오를 지원합니다.
 */
class UrlDialog : public QDialog {
    Q_OBJECT
public:
    explicit UrlDialog(QWidget* parent = nullptr);

    QString url() const { return urlEdit_->text().trimmed(); }

private:
    QLineEdit*   urlEdit_   = nullptr;
    QPushButton* okBtn_     = nullptr;
    QPushButton* cancelBtn_ = nullptr;
};
