#include "ShortcutOverlay.h"
#include <QPainter>
#include <QMouseEvent>

static const QList<QPair<QString, QString>> SHORTCUTS = {
    {"Space",       "재생 / 일시정지"},
    {"←  /  →",    "5초 뒤로 / 앞으로"},
    {"↑  /  ↓",    "볼륨 증가 / 감소"},
    {"F",           "전체화면 전환"},
    {"M",           "음소거"},
    {"Ctrl+O",      "파일 열기"},
    {"",            ""},
    {"A",           "A-B 반복: A 지점 설정"},
    {"B",           "A-B 반복: B 지점 설정"},
    {"Ctrl+A",      "A-B 반복 해제"},
    {"",            ""},
    {"S",           "스크린샷 저장"},
    {">  /  <",     "재생속도 증가 / 감소"},
    {"D",           "오디오 딜레이 +100ms"},
    {"Shift+D",     "오디오 딜레이 -100ms"},
    {"Z",           "자막 딜레이 +100ms"},
    {"Shift+Z",     "자막 딜레이 -100ms"},
    {"",            ""},
    {"P",           "전문 기능 패널 토글"},
    {"?",           "이 도움말 표시 / 닫기"},
};

ShortcutOverlay::ShortcutOverlay(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setAttribute(Qt::WA_NoSystemBackground);
    setFocusPolicy(Qt::StrongFocus);
    hide();
}

void ShortcutOverlay::toggle() {
    if (isVisible()) {
        hide();
    } else {
        if (parentWidget()) {
            setGeometry(parentWidget()->rect());
        }
        show();
        raise();
        setFocus();
    }
}

void ShortcutOverlay::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // 반투명 배경
    p.fillRect(rect(), QColor(0, 0, 0, 210));

    // 패널 영역
    int panelW = 480;
    int panelH = qMin(height() - 60, 520);
    int panelX = (width() - panelW) / 2;
    int panelY = (height() - panelH) / 2;

    // 패널 배경
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(18, 18, 18, 240));
    p.drawRoundedRect(panelX, panelY, panelW, panelH, 10, 10);

    // 테두리
    p.setPen(QPen(QColor(40, 40, 40), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(panelX, panelY, panelW, panelH, 10, 10);

    // 제목
    p.setPen(QColor(255, 255, 255));
    QFont titleFont("Malgun Gothic", 14, QFont::Bold);
    p.setFont(titleFont);
    p.drawText(QRect(panelX, panelY + 16, panelW, 28), Qt::AlignCenter, "단축키 목록");

    // 구분선
    p.setPen(QColor(35, 35, 35));
    p.drawLine(panelX + 20, panelY + 48, panelX + panelW - 20, panelY + 48);

    // 단축키 목록
    QFont keyFont("Consolas", 10, QFont::Bold);
    QFont descFont("Malgun Gothic", 10);
    int y = panelY + 60;
    int rowH = 22;

    for (const auto& pair : SHORTCUTS) {
        if (pair.first.isEmpty()) {
            // 구분선
            p.setPen(QColor(28, 28, 28));
            p.drawLine(panelX + 20, y + 4, panelX + panelW - 20, y + 4);
            y += 12;
            continue;
        }

        // 키 배경 박스
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(30, 30, 30));
        p.drawRoundedRect(panelX + 20, y + 2, 130, rowH - 4, 3, 3);

        // 키 텍스트
        p.setFont(keyFont);
        p.setPen(QColor(79, 195, 247));
        p.drawText(QRect(panelX + 20, y, 130, rowH), Qt::AlignCenter, pair.first);

        // 설명 텍스트
        p.setFont(descFont);
        p.setPen(QColor(180, 180, 180));
        p.drawText(QRect(panelX + 160, y, panelW - 180, rowH), Qt::AlignVCenter | Qt::AlignLeft, pair.second);

        y += rowH;
    }

    // 닫기 안내
    p.setFont(QFont("Malgun Gothic", 9));
    p.setPen(QColor(80, 80, 80));
    p.drawText(QRect(panelX, panelY + panelH - 28, panelW, 20),
               Qt::AlignCenter, "아무 키나 누르거나 클릭하면 닫힙니다");
}

void ShortcutOverlay::keyPressEvent(QKeyEvent*) {
    hide();
}

void ShortcutOverlay::mousePressEvent(QMouseEvent*) {
    hide();
}
