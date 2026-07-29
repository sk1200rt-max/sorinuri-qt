#include "ShortcutOverlay.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QKeyEvent>

ShortcutOverlay::ShortcutOverlay(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setAttribute(Qt::WA_NoSystemBackground);
    setFocusPolicy(Qt::StrongFocus);
    hide();
}

void ShortcutOverlay::toggle() {
    if (isVisible()) hide();
    else { if (parentWidget()) setGeometry(parentWidget()->rect()); raise(); show(); setFocus(); }
}

void ShortcutOverlay::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor(0,0,0,200));

    int panelW = 720, panelH = 540;
    int panelX = (width()-panelW)/2, panelY = (height()-panelH)/2;

    QPainterPath bg; bg.addRoundedRect(panelX,panelY,panelW,panelH,12,12);
    p.fillPath(bg, QColor(16,16,16,245));
    p.setPen(QPen(QColor(45,45,45),1)); p.drawPath(bg);

    // 제목
    p.setFont(QFont("Malgun Gothic",14,QFont::Bold));
    p.setPen(Qt::white);
    p.drawText(QRect(panelX,panelY+14,panelW,28),Qt::AlignCenter,"소리누리 단축키 가이드");
    p.setPen(QPen(QColor(35,35,35),1));
    p.drawLine(panelX+20,panelY+46,panelX+panelW-20,panelY+46);

    struct Item { QString key; QString desc; bool section=false; };
    QVector<Item> left = {
        {""  ,"재생 제어",true},
        {"Space","재생 / 일시정지"},
        {"← / →","10초 뒤로 / 앞으로"},
        {"Shift+← / →","1분 뒤로 / 앞으로"},
        {"Ctrl+← / →","이전 / 다음 파일"},
        {"S","정지"},
        {""  ,"볼륨 / 오디오",true},
        {"↑ / ↓","볼륨 +5% / -5%"},
        {"M","음소거 토글"},
        {"A","오디오 트랙 전환"},
        {"D / Shift+D","오디오 딜레이 ±100ms"},
        {""  ,"자막",true},
        {"V","자막 트랙 전환"},
        {"Z / Shift+Z","자막 딜레이 ±100ms"},
    };
    QVector<Item> right = {
        {""  ,"화면",true},
        {"F / F11","전체화면 토글"},
        {"Esc","전체화면 해제"},
        {""  ,"전문 기능",true},
        {"P","전문 기능 패널 토글"},
        {"A / B","A-B 반복 시작 / 끝 설정"},
        {"Ctrl+A","A-B 반복 해제"},
        {"> / <","재생속도 ±0.25x"},
        {"Ctrl+S","스크린샷 저장"},
        {"?","단축키 도움말"},
        {""  ,"음악 모드 전용",true},
        {"M","미니 플레이어 토글"},
        {"E","EQ 패널"},
        {"L","재생목록 패널"},
        {"G","가사 패널"},
        {"R / X","반복 / 셔플 토글"},
    };

    QFont kf("Consolas",10,QFont::Bold);
    QFont df("Malgun Gothic",10);
    QFont sf("Malgun Gothic",10,QFont::Bold);
    int rowH=21, startY=panelY+54;
    int lx=panelX+14, rx=panelX+panelW/2+8;

    auto drawCol=[&](int ox, const QVector<Item>& items){
        int y=startY;
        for(const auto& it:items){
            if(it.section){
                p.setFont(sf); p.setPen(QColor(0,200,180));
                p.drawText(QRect(ox,y,panelW/2-20,rowH),Qt::AlignVCenter|Qt::AlignLeft,it.desc);
                y+=rowH; continue;
            }
            p.setFont(kf);
            QRect kr(ox,y+3,120,rowH-6);
            p.fillRect(kr,QColor(30,30,30));
            p.setPen(QPen(QColor(55,55,55),1)); p.drawRect(kr);
            p.setPen(QColor(0,200,180));
            p.drawText(kr,Qt::AlignCenter,it.key);
            p.setFont(df); p.setPen(QColor(175,175,175));
            p.drawText(QRect(ox+126,y,panelW/2-140,rowH),Qt::AlignVCenter|Qt::AlignLeft,it.desc);
            y+=rowH;
        }
    };
    drawCol(lx,left);
    drawCol(rx,right);

    p.setPen(QPen(QColor(35,35,35),1));
    p.drawLine(panelX+panelW/2,panelY+54,panelX+panelW/2,panelY+panelH-30);

    p.setFont(QFont("Malgun Gothic",9)); p.setPen(QColor(65,65,65));
    p.drawText(QRect(panelX,panelY+panelH-22,panelW,18),Qt::AlignCenter,"아무 키나 누르거나 클릭하면 닫힙니다");
}

void ShortcutOverlay::keyPressEvent(QKeyEvent*){hide();}
void ShortcutOverlay::mousePressEvent(QMouseEvent*){hide();}
