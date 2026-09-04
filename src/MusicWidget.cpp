#include "MusicWidget.h"
#include "UiTheme.h"
#include "LyricsWidget.h"
#include "MpvCore.h"
#include <QPainter>
#include <QApplication>
#include <QSettings>
#include <QTimer>
#include <QPainterPath>
#include <QResizeEvent>
#include <QImage>
#include <QFontMetrics>
#include <QFileInfo>
#include <QMessageBox>
#include <cmath>

static const QColor kTeal(0, 200, 180);
static const QColor kDark(14, 14, 14);

static QLabel* makeBadge(const QString& text, const QString& color, QWidget* parent) {
    auto* lbl = new QLabel(text, parent);
    lbl->setStyleSheet(QString(
        "QLabel{color:%1;border:1px solid %1;border-radius:3px;"
        "padding:1px 6px;font-size:11px;font-family:'Consolas',monospace;background:transparent;}").arg(color));
    lbl->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    return lbl;
}

static QPixmap blurPixmap(const QPixmap& src, int) {
    if (src.isNull()) return src;
    // 픽셀 단위 다중 블러는 큰 앨범아트에서 UI 이벤트 루프를 길게 점유한다.
    // 축소 뒤 부드러운 확대는 배경 질감은 유지하면서 파일 전환을 즉시 끝낸다.
    const QImage thumbnailImage = src.toImage().scaled(96, 96, Qt::IgnoreAspectRatio,
                                                        Qt::FastTransformation);
    return QPixmap::fromImage(thumbnailImage.scaled(320, 320, Qt::IgnoreAspectRatio,
                                                     Qt::SmoothTransformation));
}

static QColor extractDominant(const QPixmap& src) {
    if (src.isNull()) return QColor(0,120,100);
    QImage img = src.toImage().scaled(50,50,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
    long long r=0,g=0,b=0,cnt=0;
    for (int y=0;y<img.height();++y) for (int x=0;x<img.width();++x) {
        QRgb c=img.pixel(x,y);
        int br=(qRed(c)+qGreen(c)+qBlue(c))/3;
        if (br>30&&br<220){r+=qRed(c);g+=qGreen(c);b+=qBlue(c);++cnt;}
    }
    if (!cnt) return QColor(0,120,100);
    QColor avg(r/cnt,g/cnt,b/cnt);
    int h,s,v; avg.getHsv(&h,&s,&v);
    return QColor::fromHsv(h,qMin(255,s+80),qMax(80,v));
}

// EqPanel
const QStringList EqPanel::kBands={"31Hz","63Hz","125Hz","250Hz","500Hz","1kHz","2kHz","4kHz","8kHz","16kHz"};
const QVector<QVector<double>> EqPanel::kPresets={
    {0,0,0,0,0,0,0,0,0,0},
    {2,1,0,0,-0.5,-0.5,0,0.5,1,1.5},
    {5,4,3,1,0,0,0,0,0,0},
    {-1,0,1,2,3,3,2,1,0,-1},
    {3,2,1,0,0,0,0,1,2,3},
    {4,3,1,0,-1,-1,0,1,3,4},
    {2,1,0,1,2,2,1,0,-1,-1},
};

EqPanel::EqPanel(QWidget* parent):QWidget(parent){buildUI();}

void EqPanel::buildUI(){
    setStyleSheet("background:#101718;color:#F2F7F6;");
    auto* root=new QVBoxLayout(this);
    root->setContentsMargins(16,12,16,12);root->setSpacing(8);
    auto* hdr=new QHBoxLayout;
    auto* ttl=new QLabel("EQUALIZER",this);
    ttl->setStyleSheet("font-size:13px;font-weight:800;color:#F2F7F6;letter-spacing:1px;");
    hdr->addWidget(ttl);hdr->addStretch();
    auto* onOff=new QPushButton("ON",this);
    onOff->setCheckable(true);onOff->setChecked(true);onOff->setFocusPolicy(Qt::NoFocus);
    onOff->setStyleSheet("QPushButton{background:#0A4940;color:#F2F7F6;border:1px solid #00D4B4;border-radius:10px;padding:2px 10px;font-size:11px;font-weight:700;}"
                         "QPushButton:!checked{background:#151F20;color:#71807F;border-color:#2B3B3C;}");
    hdr->addWidget(onOff);root->addLayout(hdr);
    QStringList pnames={"Flat","Audiophile","Bass Boost","Vocal","Classical","Rock","Jazz"};
    auto* prow=new QHBoxLayout;prow->setSpacing(4);
    for(int i=0;i<pnames.size();++i){
        auto* btn=new QPushButton(pnames[i],this);
        btn->setFocusPolicy(Qt::NoFocus);
        btn->setStyleSheet("QPushButton{background:#151F20;color:#A3B1B0;border-radius:7px;padding:3px 8px;font-size:11px;border:1px solid #2B3B3C;}"
                           "QPushButton:hover{background:#1C292A;color:#F2F7F6;border-color:#00D4B4;}"
                           "QPushButton:checked{background:#0A4940;color:#F2F7F6;border-color:#00D4B4;}");
        btn->setCheckable(true);if(i==0)btn->setChecked(true);
        connect(btn,&QPushButton::clicked,this,[this,i](){applyPreset(i);});
        prow->addWidget(btn);
    }
    root->addLayout(prow);
    auto* srow=new QHBoxLayout;srow->setSpacing(6);
    for(int i=0;i<10;++i){
        auto* col=new QVBoxLayout;col->setSpacing(2);col->setAlignment(Qt::AlignHCenter);
        auto* gl=new QLabel("0",this);gl->setAlignment(Qt::AlignCenter);
        gl->setStyleSheet("font-size:10px;color:#00D4B4;font-family:'Cascadia Mono','Consolas';font-weight:700;");
        gl->setFixedWidth(36);gainLabels_.append(gl);col->addWidget(gl);
        auto* sl=new QSlider(Qt::Vertical,this);
        sl->setRange(-120,120);sl->setValue(0);sl->setFixedWidth(24);sl->setMinimumHeight(100);
        sl->setFocusPolicy(Qt::NoFocus);  // HiDPI: EQ 슬라이더 클릭 후 단축키 유지
        sl->setStyleSheet("QSlider::groove:vertical{background:#2B3B3C;width:4px;border-radius:2px;}"
                          "QSlider::handle:vertical{background:#00D4B4;width:14px;height:14px;margin:-5px -5px;border-radius:7px;}"
                          "QSlider::add-page:vertical{background:#00D4B4;border-radius:2px;}"
                          "QSlider::sub-page:vertical{background:#71807F;border-radius:2px;}");
        connect(sl,&QSlider::valueChanged,this,[this,i,gl](int v){
            double dB=v/10.0;
            gl->setText(dB>=0?QString("+%1").arg(dB,0,'f',1):QString("%1").arg(dB,0,'f',1));
            QVector<double> gains;
            for(auto* s:sliders_) gains.append(s->value()/10.0);
            emit eqChanged(gains);
        });
        sliders_.append(sl);col->addWidget(sl,1,Qt::AlignHCenter);
        auto* bl=new QLabel(kBands[i],this);bl->setAlignment(Qt::AlignCenter);
        bl->setStyleSheet("font-size:9px;color:#71807F;");bl->setFixedWidth(36);col->addWidget(bl);
        srow->addLayout(col);
    }
    root->addLayout(srow,1);
    auto* br=new QPushButton("초기화",this);
    br->setFocusPolicy(Qt::NoFocus);  // HiDPI: 버튼 클릭 후 단축키 유지
    br->setStyleSheet("QPushButton{background:#151F20;color:#A3B1B0;border:1px solid #2B3B3C;border-radius:8px;padding:4px 16px;font-size:12px;}"
                      "QPushButton:hover{background:#1C292A;color:#F2F7F6;border-color:#00D4B4;}");
    connect(br,&QPushButton::clicked,this,[this](){applyPreset(0);});
    auto* saveBtn=new QPushButton("프리셋 저장",this);
    saveBtn->setFocusPolicy(Qt::NoFocus);
    saveBtn->setStyleSheet(br->styleSheet());
    connect(saveBtn,&QPushButton::clicked,this,[this,saveBtn](){
        QSettings s("Sorinuri","SorinuriPlayer");
        QVariantList list;
        for(auto* sl:sliders_) list.append(sl->value()/10.0);
        s.setValue("eq/user_preset",list);
        saveBtn->setText("✓ 저장됨");
        QTimer::singleShot(1500,saveBtn,[saveBtn](){saveBtn->setText("프리셋 저장");});
    });
    auto* loadBtn=new QPushButton("저장된 프리셋",this);
    loadBtn->setFocusPolicy(Qt::NoFocus);
    loadBtn->setStyleSheet(br->styleSheet());
    connect(loadBtn,&QPushButton::clicked,this,[this](){
        QSettings s("Sorinuri","SorinuriPlayer");
        QVariantList list=s.value("eq/user_preset").toList();
        if(list.size()==10)
            for(int i=0;i<10;++i) sliders_[i]->setValue(qRound(list[i].toDouble()*10));
    });
    auto* brow=new QHBoxLayout;
    brow->addWidget(br);brow->addWidget(saveBtn);brow->addWidget(loadBtn);brow->addStretch();
    root->addLayout(brow);
}

void EqPanel::applyPreset(int idx){
    if(idx<0||idx>=kPresets.size()) return;
    const auto& p=kPresets[idx];
    for(int i=0;i<sliders_.size()&&i<p.size();++i) sliders_[i]->setValue(qRound(p[i]*10));
}

// PlaylistPanel
PlaylistPanel::PlaylistPanel(QWidget* parent):QWidget(parent){
    setStyleSheet("background:#101718;color:#F2F7F6;");
    auto* root=new QVBoxLayout(this);root->setContentsMargins(12,10,12,10);root->setSpacing(6);
    auto* hdr=new QHBoxLayout;
    auto* ttl=new QLabel("PLAYLIST",this);
    ttl->setStyleSheet("font-size:13px;font-weight:800;color:#F2F7F6;letter-spacing:1px;");
    hdr->addWidget(ttl);hdr->addStretch();root->addLayout(hdr);
    searchEdit_=new QLineEdit(this);searchEdit_->setPlaceholderText("곡 검색...");
    searchEdit_->setStyleSheet("QLineEdit{background:#151F20;border:1px solid #2B3B3C;border-radius:8px;color:#F2F7F6;padding:5px 9px;font-size:12px;}QLineEdit:focus{border-color:#00D4B4;}");
    root->addWidget(searchEdit_);
    listWidget_=new QListWidget(this);
    listWidget_->setStyleSheet("QListWidget{background:#141414;border:none;color:white;}"
                               "QListWidget::item{padding:6px 4px;border-bottom:1px solid #1e1e1e;}"
                               "QListWidget::item:hover{background:#1e1e1e;}"
                               "QListWidget::item:selected{background:#1a3030;border-left:3px solid #00c8b4;}");
    root->addWidget(listWidget_,1);
    connect(listWidget_,&QListWidget::itemDoubleClicked,this,[this](QListWidgetItem* item){
        emit trackSelected(listWidget_->row(item));
    });
    connect(searchEdit_,&QLineEdit::textChanged,this,[this](const QString& t){
        for(int i=0;i<listWidget_->count();++i)
            listWidget_->item(i)->setHidden(!listWidget_->item(i)->text().contains(t,Qt::CaseInsensitive));
    });
}

void PlaylistPanel::setTracks(const QStringList& paths){
    allPaths_=paths;listWidget_->clear();
    for(int i=0;i<paths.size();++i){
        QFileInfo fi(paths[i]);
        listWidget_->addItem(QString("%1. %2").arg(i+1).arg(fi.completeBaseName()));
    }
}

void PlaylistPanel::setCurrentIndex(int idx){
    if(idx>=0&&idx<listWidget_->count()){
        listWidget_->setCurrentRow(idx);
        listWidget_->scrollToItem(listWidget_->item(idx));
    }
}

// MusicWidget
MusicWidget::MusicWidget(MpvCore* core, QWidget* parent):QWidget(parent),core_(core){
    setAutoFillBackground(false);
    setMouseTracking(true);
    dominantColor_=kTeal;
    specBins_.fill(0.0f,64);specPeak_.fill(0.0f,64);
    peakTimer_=new QTimer(this);peakTimer_->setInterval(50);
    connect(peakTimer_,&QTimer::timeout,this,[this](){
        bool ch=false;
        for(int i=0;i<specPeak_.size();++i) if(specPeak_[i]>0.002f){specPeak_[i]*=0.92f;ch=true;}
        if(ch) update();
    });
    // 음악 화면이 처음 만들어져도 백그라운드 타이머를 시작하지 않는다.
    // 실제 음악 재생 화면이 활성화될 때 setVisualizationActive()가 시작한다.
    rotationTimer_=new QTimer(this);rotationTimer_->setInterval(33);
    connect(rotationTimer_,&QTimer::timeout,this,&MusicWidget::onRotationTick);
    setupUI();setupConnections();
}

void MusicWidget::mouseMoveEvent(QMouseEvent* e) {
    QWidget::mouseMoveEvent(e);
    if (parent()) {
        QMouseEvent* parentEvent = new QMouseEvent(
            e->type(), e->position(), e->globalPosition(),
            e->button(), e->buttons(), e->modifiers());
        QApplication::postEvent(parent(), parentEvent);
    }
}

void MusicWidget::contextMenuEvent(QContextMenuEvent* e) {
    QMenu menu(this);
    menu.setStyleSheet(SorinuriUi::menuStyle());

    auto* actPlay = menu.addAction(isPlaying_ ? "⏸  일시정지" : "▶  재생");
    connect(actPlay, &QAction::triggered, this, &MusicWidget::playPauseRequested);

    auto* actPrev = menu.addAction("⏮  이전 트랙");
    connect(actPrev, &QAction::triggered, this, &MusicWidget::prevRequested);
    auto* actNext = menu.addAction("⏭  다음 트랙");
    connect(actNext, &QAction::triggered, this, &MusicWidget::nextRequested);

    menu.addSeparator();

    auto* actShuffle = menu.addAction(isShuffle_ ? "✓  셔플 켜짐" : "⇌  셔플");
    connect(actShuffle, &QAction::triggered, this, [this](){
        isShuffle_ = !isShuffle_;
        btnShuffle_->setStyleSheet(isShuffle_
            ?"QPushButton{background:transparent;color:#00c8b4;border:none;font-size:16px;}"
            :"QPushButton{background:transparent;color:#aaa;border:none;font-size:16px;}QPushButton:hover{color:white;}");
        emit shuffleToggled(isShuffle_);
    });

    auto* actRepeat = menu.addAction(isRepeat_ ? "✓  반복 켜짐" : "↺  반복");
    connect(actRepeat, &QAction::triggered, this, [this](){
        isRepeat_ = !isRepeat_;
        btnRepeat_->setStyleSheet(isRepeat_
            ?"QPushButton{background:transparent;color:#00c8b4;border:none;font-size:16px;}"
            :"QPushButton{background:transparent;color:#aaa;border:none;font-size:16px;}QPushButton:hover{color:white;}");
        emit repeatToggled(isRepeat_);
    });

    menu.addSeparator();

    auto* actPlaylist = menu.addAction("☰  재생목록 보기");
    connect(actPlaylist, &QAction::triggered, this, [this](){ onRightPanelToggle(2); });
    auto* actLyrics = menu.addAction("♪  가사 보기");
    connect(actLyrics, &QAction::triggered, this, [this](){ onRightPanelToggle(0); });
    auto* actEq = menu.addAction("🎚  이퀄라이저");
    connect(actEq, &QAction::triggered, this, [this](){ onRightPanelToggle(1); });

    menu.addSeparator();

    if (!currentMeta_.filePath.isEmpty()) {
        auto* actInfo = menu.addAction("ℹ  파일 정보");
        connect(actInfo, &QAction::triggered, this, [this](){
            QFileInfo fi(currentMeta_.filePath);
            QString info = QString(
                "파일명: %1\n경로: %2\n크기: %3 MB\n코덱: %4\n샘플레이트: %5 Hz\n비트 깊이: %6 bit\n채널: %7")
                .arg(fi.fileName())
                .arg(fi.absolutePath())
                .arg(fi.size() / (1024.0*1024.0), 0, 'f', 2)
                .arg(currentMeta_.codec.isEmpty() ? "알 수 없음" : currentMeta_.codec.toUpper())
                .arg(currentMeta_.sampleRate)
                .arg(currentMeta_.bitDepth > 0 ? QString::number(currentMeta_.bitDepth) : "알 수 없음")
                .arg(currentMeta_.channels == 1 ? "모노" :
                     currentMeta_.channels == 2 ? "스테레오" :
                     QString("%1ch").arg(currentMeta_.channels));
            QMessageBox* mb = new QMessageBox(this);
            mb->setWindowTitle("파일 정보");
            mb->setText(info);
            mb->setStyleSheet("QMessageBox{background:#1e1e1e;color:white;}"
                              "QLabel{color:white;font-size:13px;}"
                              "QPushButton{background:#333;color:white;border:1px solid #555;border-radius:4px;padding:4px 16px;}"
                              "QPushButton:hover{background:#444;}");
            mb->exec();
        });
    }

    // 재생목록에 추가 (현재 파일)
    if (!currentMeta_.filePath.isEmpty()) {
        auto* actAddPlaylist = menu.addAction("➕  재생목록에 추가");
        connect(actAddPlaylist, &QAction::triggered, this, [this](){
            emit addToPlaylistRequested(currentMeta_.filePath);
        });
    }
    menu.addSeparator();
    auto* actSettings = menu.addAction("⚙  설정");
    connect(actSettings, &QAction::triggered, this, &MusicWidget::settingsRequested);
    menu.exec(e->globalPos());
}

void MusicWidget::setupUI(){
    // 음악은 영상용 전역 콘솔의 변형이 아니라, 현재 곡에 집중하는 독립 감상 화면이다.
    setStyleSheet("background:#0A0F10;");
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* stage = new QWidget(this);
    stage->setObjectName("musicStage");
    stage->setStyleSheet(
        "QWidget#musicStage{background:#0D1415;border-bottom:1px solid #1D2A2B;}"
        "QLabel{background:transparent;}");
    auto* stageLayout = new QHBoxLayout(stage);
    stageLayout->setContentsMargins(48, 32, 48, 24);
    stageLayout->setSpacing(42);

    // 앨범아트는 별도의 고정 사이드바가 아니라, 현재 곡을 대표하는 독립 스테이지다.
    auto* artStage = new QWidget(stage);
    artStage->setMinimumWidth(330);
    auto* artLayout = new QVBoxLayout(artStage);
    artLayout->setContentsMargins(0, 0, 0, 0);
    artLayout->setSpacing(12);
    artLayout->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    albumArtLabel_ = new QLabel(artStage);
    albumArtLabel_->setObjectName("musicAlbumArt");
    albumArtLabel_->setFixedSize(294, 294);
    albumArtLabel_->setAlignment(Qt::AlignCenter);
    albumArtLabel_->setText(QStringLiteral("♪"));
    albumArtLabel_->setStyleSheet(
        "QLabel#musicAlbumArt{background:#142223;color:#7EA49E;border:1px solid #29403D;"
        "border-radius:12px;font-size:54px;font-weight:300;}");
    artLayout->addWidget(albumArtLabel_, 0, Qt::AlignHCenter);
    auto* sourceLabel = new QLabel(QStringLiteral("MUSIC  ·  LOCAL / ORIGINALS"), artStage);
    sourceLabel->setAlignment(Qt::AlignCenter);
    sourceLabel->setStyleSheet("color:#6F8481;font-size:10px;font-weight:700;letter-spacing:1.4px;");
    artLayout->addWidget(sourceLabel);
    stageLayout->addWidget(artStage, 0);

    auto* detailStage = new QWidget(stage);
    auto* detailLayout = new QVBoxLayout(detailStage);
    detailLayout->setContentsMargins(0, 12, 0, 12);
    detailLayout->setSpacing(9);
    auto* eyebrow = new QLabel(QStringLiteral("현재 감상 중"), detailStage);
    eyebrow->setStyleSheet("color:#00D4B4;font-size:11px;font-weight:700;letter-spacing:1.5px;");
    detailLayout->addWidget(eyebrow);

    titleLabel_ = new QLabel(QStringLiteral("트랙 제목"), detailStage);
    titleLabel_->setWordWrap(true);
    titleLabel_->setStyleSheet("color:#F3F7F6;font-size:32px;font-weight:750;letter-spacing:-0.5px;");
    detailLayout->addWidget(titleLabel_);
    artistLabel_ = new QLabel(QStringLiteral("아티스트"), detailStage);
    artistLabel_->setStyleSheet("color:#B1C0BE;font-size:17px;font-weight:500;");
    detailLayout->addWidget(artistLabel_);
    albumLabel_ = new QLabel(detailStage);
    albumLabel_->setStyleSheet("color:#718582;font-size:12px;");
    detailLayout->addWidget(albumLabel_);
    detailLayout->addSpacing(13);

    badgeRow_ = new QWidget(detailStage);
    badgeRow_->setStyleSheet("background:transparent;");
    auto* badgeLayout = new QHBoxLayout(badgeRow_);
    badgeLayout->setContentsMargins(0, 0, 0, 0);
    badgeLayout->setSpacing(6);
    codecBadge_ = makeBadge("FLAC", "#00D4B4", badgeRow_);
    bitBadge_ = makeBadge("24-bit", "#A3B1B0", badgeRow_);
    rateBadge_ = makeBadge("192kHz", "#A3B1B0", badgeRow_);
    chBadge_ = makeBadge("Stereo", "#A3B1B0", badgeRow_);
    bpBadge_ = makeBadge("BIT-PERFECT", "#00D4B4", badgeRow_);
    for (QLabel* badge : {codecBadge_, bitBadge_, rateBadge_, chBadge_, bpBadge_}) badgeLayout->addWidget(badge);
    badgeLayout->addStretch(1);
    detailLayout->addWidget(badgeRow_);
    detailLayout->addStretch(1);

    auto* assistHint = new QLabel(QStringLiteral("가사 · EQ · 재생목록은 필요할 때만 열립니다"), detailStage);
    assistHint->setStyleSheet("color:#6E807E;font-size:11px;");
    detailLayout->addWidget(assistHint);
    stageLayout->addWidget(detailStage, 1);
    root->addWidget(stage, 1);

    // 보조 기능은 항상 화면을 나누지 않고, 사용자가 요청할 때만 열린다.
    assistantPanel_ = new QWidget(this);
    assistantPanel_->setObjectName("musicAssistantPanel");
    assistantPanel_->setStyleSheet(
        "QWidget#musicAssistantPanel{background:#10191A;border-top:1px solid #233333;border-bottom:1px solid #233333;}");
    auto* assistantLayout = new QVBoxLayout(assistantPanel_);
    assistantLayout->setContentsMargins(32, 12, 32, 14);
    assistantLayout->setSpacing(8);
    auto* tabRow = new QHBoxLayout();
    tabRow->setContentsMargins(0, 0, 0, 0);
    tabRow->setSpacing(6);
    auto makeAssistantButton = [this](const QString& text) {
        auto* button = new QPushButton(text, assistantPanel_);
        button->setCheckable(true);
        button->setFocusPolicy(Qt::NoFocus);
        button->setCursor(Qt::PointingHandCursor);
        button->setFixedHeight(30);
        button->setStyleSheet(
            "QPushButton{background:transparent;color:#91A3A0;border:1px solid transparent;border-radius:7px;padding:0 12px;font-size:11px;font-weight:700;}"
            "QPushButton:hover{background:#192727;color:#EFF8F6;border-color:#31504C;}"
            "QPushButton:checked{background:#0B3932;color:#DFFFF7;border-color:#00D4B4;}");
        return button;
    };
    btnShowLyrics_ = makeAssistantButton(QStringLiteral("가사"));
    btnShowEq_ = makeAssistantButton(QStringLiteral("이퀄라이저"));
    btnShowPlaylist_ = makeAssistantButton(QStringLiteral("재생목록"));
    tabRow->addWidget(btnShowLyrics_);
    tabRow->addWidget(btnShowEq_);
    tabRow->addWidget(btnShowPlaylist_);
    tabRow->addStretch(1);
    auto* closeAssist = new QPushButton(QStringLiteral("접기"), assistantPanel_);
    closeAssist->setFocusPolicy(Qt::NoFocus);
    closeAssist->setCursor(Qt::PointingHandCursor);
    closeAssist->setFixedHeight(30);
    closeAssist->setStyleSheet("QPushButton{background:transparent;color:#70827F;border:none;padding:0 8px;font-size:11px;}QPushButton:hover{color:#F3F7F6;}");
    tabRow->addWidget(closeAssist);
    assistantLayout->addLayout(tabRow);

    rightStack_ = new QStackedWidget(assistantPanel_);
    rightStack_->setStyleSheet("QStackedWidget{background:#0D1415;border:1px solid #21302F;border-radius:9px;}");
    lyricsWidget_ = new LyricsWidget(rightStack_);
    eqPanel_ = new EqPanel(rightStack_);
    playlistPanel_ = new PlaylistPanel(rightStack_);
    rightStack_->addWidget(lyricsWidget_);
    rightStack_->addWidget(eqPanel_);
    rightStack_->addWidget(playlistPanel_);
    rightStack_->setCurrentIndex(0);
    rightStack_->setMinimumHeight(190);
    assistantLayout->addWidget(rightStack_);
    connect(closeAssist, &QPushButton::clicked, this, [this]() {
        if (assistantPanel_) assistantPanel_->hide();
        if (btnShowLyrics_) btnShowLyrics_->setChecked(false);
        if (btnShowEq_) btnShowEq_->setChecked(false);
        if (btnShowPlaylist_) btnShowPlaylist_->setChecked(false);
    });
    assistantPanel_->hide();
    root->addWidget(assistantPanel_);

    auto* deck = new QWidget(this);
    deck->setObjectName("musicDeck");
    deck->setStyleSheet("QWidget#musicDeck{background:#0A0F10;border-top:1px solid #1A2829;}");
    auto* deckLayout = new QVBoxLayout(deck);
    deckLayout->setContentsMargins(32, 12, 32, 10);
    deckLayout->setSpacing(10);
    auto* seekRow = new QHBoxLayout();
    seekRow->setSpacing(10);
    timeCurrent_ = new QLabel("00:00", deck);
    timeCurrent_->setFixedWidth(46);
    timeCurrent_->setStyleSheet("color:#B9C8C5;font-size:11px;font-family:'Cascadia Mono','Consolas';font-weight:700;");
    seekSlider_ = new ClickSeekSlider(Qt::Horizontal, deck);
    seekSlider_->setRange(0, 1000);
    seekSlider_->setFocusPolicy(Qt::NoFocus);
    seekSlider_->setStyleSheet(
        "QSlider::groove:horizontal{background:#263536;height:4px;border-radius:2px;}"
        "QSlider::sub-page:horizontal{background:#00B89D;border-radius:2px;}"
        "QSlider::handle:horizontal{background:#00D4B4;width:13px;height:13px;margin:-5px 0;border:2px solid #0A0F10;border-radius:7px;}");
    timeDuration_ = new QLabel("00:00", deck);
    timeDuration_->setFixedWidth(46);
    timeDuration_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    timeDuration_->setStyleSheet("color:#B9C8C5;font-size:11px;font-family:'Cascadia Mono','Consolas';font-weight:700;");
    seekRow->addWidget(timeCurrent_);
    seekRow->addWidget(seekSlider_, 1);
    seekRow->addWidget(timeDuration_);
    deckLayout->addLayout(seekRow);

    auto* controlRow = new QHBoxLayout();
    controlRow->setContentsMargins(0, 0, 0, 0);
    controlRow->setSpacing(10);
    auto makeCtrlBtn = [&](const QString& text, bool primary = false) {
        auto* button = new QPushButton(text, deck);
        const int size = primary ? 50 : 34;
        button->setFixedSize(size, size);
        button->setFocusPolicy(Qt::NoFocus);
        button->setCursor(Qt::PointingHandCursor);
        button->setStyleSheet(primary
            ? "QPushButton{background:#00BFA5;color:#052521;border:1px solid #57E4CF;border-radius:25px;font-size:19px;font-weight:800;}QPushButton:hover{background:#35D9C0;}"
            : "QPushButton{background:transparent;color:#C9D5D3;border:1px solid transparent;border-radius:8px;font-size:17px;}QPushButton:hover{background:#172425;color:#FFFFFF;border-color:#2D4442;}");
        return button;
    };
    btnShuffle_ = makeCtrlBtn(QStringLiteral("⇌"));
    btnPrev_ = makeCtrlBtn(QStringLiteral("⏮"));
    btnPlay_ = makeCtrlBtn(QStringLiteral("▶"), true);
    btnNext_ = makeCtrlBtn(QStringLiteral("⏭"));
    btnRepeat_ = makeCtrlBtn(QStringLiteral("↺"));
    btnAbRepeat_ = new QPushButton(QStringLiteral("A-B"), deck);
    btnAbRepeat_->setFixedSize(42, 28);
    btnAbRepeat_->setFocusPolicy(Qt::NoFocus);
    btnAbRepeat_->setCursor(Qt::PointingHandCursor);
    btnAbRepeat_->setToolTip("A-B 구간 반복");
    btnAbRepeat_->setStyleSheet("QPushButton{background:#111C1D;color:#A7B8B5;border:1px solid #2B4240;border-radius:7px;font-size:10px;font-weight:800;}QPushButton:hover{color:#EEFFFB;border-color:#00D4B4;}");
    controlRow->addStretch(1);
    controlRow->addWidget(btnShuffle_);
    controlRow->addWidget(btnPrev_);
    controlRow->addWidget(btnPlay_);
    controlRow->addWidget(btnNext_);
    controlRow->addWidget(btnRepeat_);
    controlRow->addWidget(btnAbRepeat_);
    controlRow->addStretch(1);
    controlRow->addSpacing(16);
    btnVolume_ = new QPushButton(QStringLiteral("볼륨"), deck);
    btnVolume_->setFixedHeight(30);
    btnVolume_->setFocusPolicy(Qt::NoFocus);
    btnVolume_->setCursor(Qt::PointingHandCursor);
    btnVolume_->setToolTip("음소거 (M)");
    btnVolume_->setStyleSheet("QPushButton{background:transparent;color:#B8C7C4;border:none;padding:0 4px;font-size:11px;font-weight:700;}QPushButton:hover{color:#FFFFFF;}");
    volSlider_ = new QSlider(Qt::Horizontal, deck);
    volSlider_->setRange(0, 100);
    volSlider_->setValue(100);
    volSlider_->setFixedWidth(104);
    volSlider_->setFocusPolicy(Qt::NoFocus);
    volSlider_->setStyleSheet("QSlider::groove:horizontal{background:#334443;height:3px;border-radius:1px;}QSlider::sub-page:horizontal{background:#85A6A0;border-radius:1px;}QSlider::handle:horizontal{background:#F0F7F5;width:10px;height:10px;margin:-3.5px 0;border-radius:5px;}");
    controlRow->addWidget(btnVolume_);
    controlRow->addWidget(volSlider_);
    deckLayout->addLayout(controlRow);

    statusBar_ = new QLabel("재생 대기 중", deck);
    statusBar_->setAlignment(Qt::AlignCenter);
    statusBar_->setStyleSheet("color:#738784;font-size:10px;font-family:'Cascadia Mono','Consolas';letter-spacing:.5px;background:transparent;padding:2px 0;");
    deckLayout->addWidget(statusBar_);
    root->addWidget(deck);
}

void MusicWidget::setupConnections(){
    connect(btnPlay_,&QPushButton::clicked,this,&MusicWidget::playPauseRequested);
    connect(btnPrev_,&QPushButton::clicked,this,&MusicWidget::prevRequested);
    connect(btnNext_,&QPushButton::clicked,this,&MusicWidget::nextRequested);
    connect(btnShuffle_,&QPushButton::clicked,this,[this](){
        isShuffle_=!isShuffle_;
        btnShuffle_->setStyleSheet(isShuffle_
            ?"QPushButton{background:transparent;color:#00c8b4;border:none;font-size:16px;}"
            :"QPushButton{background:transparent;color:#aaa;border:none;font-size:16px;}QPushButton:hover{color:white;}");
        emit shuffleToggled(isShuffle_);
    });
    connect(btnRepeat_,&QPushButton::clicked,this,[this](){
        isRepeat_=!isRepeat_;
        btnRepeat_->setStyleSheet(isRepeat_
            ?"QPushButton{background:transparent;color:#00c8b4;border:none;font-size:16px;}"
            :"QPushButton{background:transparent;color:#aaa;border:none;font-size:16px;}QPushButton:hover{color:white;}");
        emit repeatToggled(isRepeat_);
    });
    connect(btnAbRepeat_,&QPushButton::clicked,this,&MusicWidget::onAbRepeatClicked);
    connect(seekSlider_,&QSlider::sliderPressed,this,[this](){
        // ClickSeekSlider: 클릭 즉시 이동 (HiDPI 250% 지원)
        if(duration_>0) emit seekRequested(duration_*seekSlider_->value()/1000.0);
    });
    connect(seekSlider_,&QSlider::sliderMoved,this,[this](int v){
        if(duration_>0) emit seekRequested(duration_*v/1000.0);
    });
    connect(seekSlider_,&QSlider::sliderReleased,this,[this](){
        if(duration_>0) emit seekRequested(duration_*seekSlider_->value()/1000.0);
    });
    connect(volSlider_,&QSlider::valueChanged,this,&MusicWidget::volumeChanged);
    // 볼륨 버튼: 음소거 토글
    connect(btnVolume_,&QPushButton::clicked,this,[this](){
        static int lastVol=100;
        if(volSlider_->value()>0){
            lastVol=volSlider_->value();
            volSlider_->setValue(0);
            btnVolume_->setText("🔇");
        } else {
            volSlider_->setValue(lastVol>0?lastVol:100);
            btnVolume_->setText("🔊");
        }
    });
    connect(btnShowLyrics_,&QPushButton::clicked,this,[this](){onRightPanelToggle(0);});
    connect(btnShowEq_,&QPushButton::clicked,this,[this](){onRightPanelToggle(1);});
    connect(btnShowPlaylist_,&QPushButton::clicked,this,[this](){onRightPanelToggle(2);});
    connect(eqPanel_,&EqPanel::eqChanged,this,[this](const QVector<double>& gains){
        if(!core_) return;
        QString eq="equalizer=";
        for(int i=0;i<gains.size();++i){
            eq+=QString("f%1=%2").arg(i).arg(gains[i],0,'f',1);
            if(i<gains.size()-1) eq+=":";
        }
        core_->setProperty("af",eq);
    });
}

void MusicWidget::onAbRepeatClicked(){
    if(!core_) return;
    // core_->position()으로 정확한 현재 재생 위치 사용 (seekSlider 동기화 지연 방지)
    double currentPos = core_ ? core_->position() : 0.0;
    if(abState_==0){
        abPointA_=currentPos;
        abState_=1;
        core_->setProperty("ab-loop-a", abPointA_);
        btnAbRepeat_->setToolTip(QString("B 지점을 클릭하여 구간 반복 시작\nA: %1").arg(formatTime(abPointA_)));
        btnAbRepeat_->setStyleSheet(
            "QPushButton{background:rgba(0,200,180,0.15);color:#00c8b4;border:1px solid #00c8b4;border-radius:4px;"
            "font-size:11px;font-weight:bold;font-family:'Consolas';}"
            "QPushButton:hover{background:rgba(0,200,180,0.25);}");
    } else if(abState_==1){
        abPointB_=currentPos;
        if(abPointB_<=abPointA_){
            double tmp=abPointA_;abPointA_=abPointB_;abPointB_=tmp;
        }
        abState_=2;
        core_->setProperty("ab-loop-a", abPointA_);
        core_->setProperty("ab-loop-b", abPointB_);
        btnAbRepeat_->setToolTip(QString("A-B 구간 반복 중\nA: %1  B: %2\n클릭하여 해제")
            .arg(formatTime(abPointA_)).arg(formatTime(abPointB_)));
        btnAbRepeat_->setStyleSheet(
            "QPushButton{background:#00c8b4;color:#0e0e0e;border:1px solid #00c8b4;border-radius:4px;"
            "font-size:11px;font-weight:bold;font-family:'Consolas';}"
            "QPushButton:hover{background:#00ddc9;}");
    } else {
        abState_=0;abPointA_=-1.0;abPointB_=-1.0;
        core_->setProperty("ab-loop-a", QString("no"));
        core_->setProperty("ab-loop-b", QString("no"));
        btnAbRepeat_->setText("A-B");
        btnAbRepeat_->setToolTip("A-B 구간 반복\n첫 클릭: A 지점 설정\n두 번째 클릭: B 지점 설정 후 반복 시작\n세 번째 클릭: 해제");
        btnAbRepeat_->setStyleSheet(
            "QPushButton{background:transparent;color:#666;border:1px solid #444;border-radius:4px;"
            "font-size:11px;font-weight:bold;font-family:'Consolas';}"
            "QPushButton:hover{color:#aaa;border-color:#666;}");
    }
}

void MusicWidget::onRightPanelToggle(int panel){
    if (!rightStack_ || !assistantPanel_) return;
    rightStack_->setCurrentIndex(panel);
    assistantPanel_->show();
    btnShowLyrics_->setChecked(panel==0);
    btnShowEq_->setChecked(panel==1);
    btnShowPlaylist_->setChecked(panel==2);
}

void MusicWidget::onRotationTick(){
    rotationAngle_+=0.3;
    if(rotationAngle_>=360.0) rotationAngle_-=360.0;
    update();
}

void MusicWidget::loadMeta(const MusicMeta& meta){
    currentMeta_=meta;
    // A-B 반복 상태 초기화 (새 트랙 로드 시)
    if(abState_!=0){
        abState_=0;abPointA_=-1.0;abPointB_=-1.0;
        if(core_){
            core_->setProperty("ab-loop-a", QString("no"));
            core_->setProperty("ab-loop-b", QString("no"));
        }
        btnAbRepeat_->setText("A-B");
        btnAbRepeat_->setStyleSheet(
            "QPushButton{background:transparent;color:#666;border:1px solid #444;border-radius:4px;"
            "font-size:11px;font-weight:bold;font-family:'Consolas';}"
            "QPushButton:hover{color:#aaa;border-color:#666;}");
    }
    titleLabel_->setText(meta.title.isEmpty()?"알 수 없는 트랙":meta.title);
    artistLabel_->setText(meta.artist.isEmpty()?"알 수 없는 아티스트":meta.artist);
    QString albumText=meta.album;
    if(!meta.year.isEmpty()) albumText+="  ·  "+meta.year;
    albumLabel_->setText(albumText);
    codecBadge_->setText(meta.codec.isEmpty()?"PCM":meta.codec.toUpper());
    bitBadge_->setText(meta.bitDepth>0?QString("%1bit").arg(meta.bitDepth):"16bit");
    if(meta.sampleRate>=1000)
        rateBadge_->setText(meta.sampleRate%1000==0
            ?QString("%1kHz").arg(meta.sampleRate/1000)
            :QString("%1kHz").arg(meta.sampleRate/1000.0,0,'f',1));
    else rateBadge_->setText(QString("%1Hz").arg(meta.sampleRate));
    chBadge_->setText(meta.channels==1?"Mono":meta.channels>2?QString("%1ch").arg(meta.channels):"Stereo");
    if (!meta.albumArt.isNull()) {
        updateAlbumArt(meta.albumArt);
    } else {
        albumArtPixmap_ = QPixmap();
        dominantColor_ = kTeal;
        if (albumArtLabel_) {
            albumArtLabel_->setPixmap(QPixmap());
            albumArtLabel_->setText(QStringLiteral("♪"));
        }
    }
    QString status=QString("%1  ·  DECODE  ·  %2  ·  %3  ·  %4")
        .arg(meta.codec.toUpper())
        .arg(meta.channels==2?"2.0":QString("%1ch").arg(meta.channels))
        .arg(rateBadge_->text()).arg(bitBadge_->text());
    if(meta.hasReplayGain) status+=QString("  ·  ReplayGain: %1dB").arg(meta.replayGain,0,'f',1);
    statusBar_->setText(status);
    if(lyricsWidget_) {
        // 파일별 싱크 오프셋 자동 불러오기
        lyricsWidget_->loadSyncOffset(meta.filePath);
        lyricsWidget_->loadForTrack(
            meta.title, meta.artist, meta.filePath,
            duration_,
            meta.album
        );
    }
    update();
}

void MusicWidget::setOutputInfo(int outSampleRate,const QString& outFormat,bool exclusive){
    if(!statusBar_) return;
    QString outRate=outSampleRate>=1000
        ?(outSampleRate%1000==0?QString("%1kHz").arg(outSampleRate/1000)
                               :QString("%1kHz").arg(outSampleRate/1000.0,0,'f',1))
        :QString("%1Hz").arg(outSampleRate);
    const bool bitPerfect = exclusive && currentMeta_.sampleRate>0
                            && currentMeta_.sampleRate==outSampleRate;
    QString status=QString("%1  ·  DECODE  ·  %2  ·  OUT: %3 %4 %5")
        .arg(currentMeta_.codec.toUpper())
        .arg(currentMeta_.channels==2?"2.0":QString("%1ch").arg(currentMeta_.channels))
        .arg(outRate)
        .arg(outFormat.toUpper())
        .arg(exclusive?"EXCLUSIVE":"SHARED");
    status += bitPerfect ? "  ·  BIT-PERFECT" : "";
    statusBar_->setText(status);
    statusBar_->setStyleSheet(bitPerfect
        ?"font-size:10px;color:#4dd0a6;font-family:'Consolas';background:#0a0a0a;padding:4px 0;"
        :"font-size:10px;color:#555;font-family:'Consolas';background:#0a0a0a;padding:4px 0;");
}

void MusicWidget::updateAlbumArt(const QPixmap& art){
    albumArtPixmap_ = art;
    dominantColor_ = extractDominant(art);
    if (albumArtLabel_) {
        albumArtLabel_->setPixmap(art.scaled(albumArtLabel_->size(), Qt::KeepAspectRatioByExpanding,
                                              Qt::SmoothTransformation));
        albumArtLabel_->setText(QString());
    }
    update();
}

void MusicWidget::updateBlurBackground(const QPixmap& art){
    if(art.isNull()){blurBg_=QPixmap();return;}
    QPixmap blurred=blurPixmap(art,8);
    QImage img=blurred.toImage().scaled(size(),Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
    QPainter p(&img);p.fillRect(img.rect(),QColor(0,0,0,160));
    blurBg_=QPixmap::fromImage(img);
}

void MusicWidget::extractDominantColor(const QPixmap& art){dominantColor_=extractDominant(art);}

void MusicWidget::updatePosition(double pos,double duration){
    duration_=duration;
    if(!seekSlider_->isSliderDown()){
        int v=(duration>0)?qRound(pos/duration*1000):0;
        seekSlider_->setValue(v);
    }
    timeCurrent_->setText(formatTime(pos));
    timeDuration_->setText(formatTime(duration));
    if(lyricsWidget_) lyricsWidget_->setPosition(pos);
}

void MusicWidget::setPlaying(bool playing){
    isPlaying_=playing;
    btnPlay_->setText(playing?"⏸":"▶");
    if(playing && visualizationActive_) rotationTimer_->start();
    else rotationTimer_->stop();
    update();
}

void MusicWidget::setVisualizationActive(bool active) {
    if (visualizationActive_ == active) return;
    visualizationActive_ = active;
    if (!peakTimer_ || !rotationTimer_) return;

    if (active) {
        peakTimer_->start();
        if (isPlaying_) rotationTimer_->start();
    } else {
        // 영상 모드·일시정지·숨겨진 음악 화면에서는 20/30fps wake-up과 repaint를
        // 멈춰 다른 응용 프로그램과 GPU·CPU 시간을 경쟁하지 않게 한다.
        peakTimer_->stop();
        rotationTimer_->stop();
    }
}

void MusicWidget::updateSpectrum(const QVector<float>& bins){
    if (!visualizationActive_) return;
    specBins_=bins;
    for(int i=0;i<qMin(bins.size(),specPeak_.size());++i)
        if(bins[i]>specPeak_[i]) specPeak_[i]=bins[i];
    update();
}

void MusicWidget::setMiniMode(bool mini){miniMode_=mini;}

void MusicWidget::paintEvent(QPaintEvent*){
    // 실제 앨범아트·재생 제어는 위젯 계층이 담당한다. 배경을 직접 다시 그리지 않아
    // 큰 앨범아트와 스펙트럼 repaint가 파일 전환이나 HiDPI 레이아웃을 방해하지 않게 한다.
}

void MusicWidget::drawAlbumArt(QPainter& p,const QRect& rect){
    p.save();
    QPainterPath circle;circle.addEllipse(rect);p.setClipPath(circle);
    if(!albumArtPixmap_.isNull()){
        p.translate(rect.center());p.rotate(rotationAngle_);p.translate(-rect.center());
        QPixmap scaled=albumArtPixmap_.scaled(rect.size(),Qt::KeepAspectRatioByExpanding,Qt::SmoothTransformation);
        QRect dr=scaled.rect();dr.moveCenter(rect.center());p.drawPixmap(dr,scaled);
    } else {
        p.fillPath(circle,QColor(30,30,30));
        p.setPen(QColor(60,60,60));p.setFont(QFont("Segoe UI",48));
        p.drawText(rect,Qt::AlignCenter,"♪");
    }
    p.restore();
    p.setPen(QPen(dominantColor_,2.5));p.drawEllipse(rect.adjusted(1,1,-1,-1));
    QRect cd(rect.center().x()-6,rect.center().y()-6,12,12);
    p.setPen(Qt::NoPen);p.setBrush(QColor(20,20,20));p.drawEllipse(cd);
    p.setBrush(dominantColor_);
    QRect id(rect.center().x()-3,rect.center().y()-3,6,6);p.drawEllipse(id);
}

void MusicWidget::drawSpectrum(QPainter& p,const QRect& rect){
    if(specBins_.isEmpty()) return;
    int n=specBins_.size();float barW=(float)rect.width()/n;float maxH=rect.height();
    for(int i=0;i<n;++i){
        float val=qMin(specBins_[i],1.0f);float barH=val*maxH;
        float t=(float)i/n;
        QColor barColor(qRound(0+t*0),qRound(120+t*80),qRound(200-t*20));barColor.setAlpha(180);
        p.fillRect(QRectF(rect.left()+i*barW+1,rect.bottom()-barH,barW-2,barH),barColor);
        if(specPeak_[i]>0.01f){
            float peakY=rect.bottom()-specPeak_[i]*maxH;
            QColor pc=dominantColor_;pc.setAlpha(200);
            p.fillRect(QRectF(rect.left()+i*barW+1,peakY-2,barW-2,2),pc);
        }
    }
}

void MusicWidget::resizeEvent(QResizeEvent* e){
    QWidget::resizeEvent(e);
    if (albumArtLabel_ && !albumArtPixmap_.isNull()) {
        albumArtLabel_->setPixmap(albumArtPixmap_.scaled(albumArtLabel_->size(), Qt::KeepAspectRatioByExpanding,
                                                          Qt::SmoothTransformation));
    }
}

QString MusicWidget::formatTime(double secs) const{
    int s=qRound(secs);
    return QString("%1:%2").arg(s/60,2,10,QChar('0')).arg(s%60,2,10,QChar('0'));
}
