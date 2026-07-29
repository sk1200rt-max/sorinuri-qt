#include "MusicWidget.h"
#include "LyricsWidget.h"
#include "MpvCore.h"
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QImage>
#include <QFontMetrics>
#include <QFileInfo>
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

static QPixmap blurPixmap(const QPixmap& src, int radius) {
    if (src.isNull()) return src;
    QImage img = src.toImage().scaled(300, 300, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    for (int pass = 0; pass < radius; ++pass)
        for (int y = 1; y < img.height()-1; ++y)
            for (int x = 1; x < img.width()-1; ++x) {
                int r=0,g=0,b=0;
                for (int dy=-1;dy<=1;++dy) for (int dx=-1;dx<=1;++dx) {
                    QRgb c=img.pixel(x+dx,y+dy); r+=qRed(c); g+=qGreen(c); b+=qBlue(c);
                }
                img.setPixel(x,y,qRgb(r/9,g/9,b/9));
            }
    return QPixmap::fromImage(img);
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
    setStyleSheet("background:#141414;color:white;");
    auto* root=new QVBoxLayout(this);
    root->setContentsMargins(16,12,16,12);root->setSpacing(8);
    auto* hdr=new QHBoxLayout;
    auto* ttl=new QLabel("EQUALIZER",this);
    ttl->setStyleSheet("font-size:13px;font-weight:bold;color:white;letter-spacing:2px;");
    hdr->addWidget(ttl);hdr->addStretch();
    auto* onOff=new QPushButton("ON",this);
    onOff->setCheckable(true);onOff->setChecked(true);
    onOff->setStyleSheet("QPushButton{background:#00c8b4;color:#0e0e0e;border-radius:10px;padding:2px 10px;font-size:11px;font-weight:bold;}"
                         "QPushButton:!checked{background:#333;color:#888;}");
    hdr->addWidget(onOff);root->addLayout(hdr);
    QStringList pnames={"Flat","Audiophile","Bass Boost","Vocal","Classical","Rock","Jazz"};
    auto* prow=new QHBoxLayout;prow->setSpacing(4);
    for(int i=0;i<pnames.size();++i){
        auto* btn=new QPushButton(pnames[i],this);
        btn->setStyleSheet("QPushButton{background:#222;color:#aaa;border-radius:3px;padding:3px 8px;font-size:11px;border:1px solid #333;}"
                           "QPushButton:hover{background:#2a2a2a;color:white;}"
                           "QPushButton:checked{background:#00c8b4;color:#0e0e0e;border-color:#00c8b4;}");
        btn->setCheckable(true);if(i==0)btn->setChecked(true);
        connect(btn,&QPushButton::clicked,this,[this,i](){applyPreset(i);});
        prow->addWidget(btn);
    }
    root->addLayout(prow);
    auto* srow=new QHBoxLayout;srow->setSpacing(6);
    for(int i=0;i<10;++i){
        auto* col=new QVBoxLayout;col->setSpacing(2);col->setAlignment(Qt::AlignHCenter);
        auto* gl=new QLabel("0",this);gl->setAlignment(Qt::AlignCenter);
        gl->setStyleSheet("font-size:10px;color:#00c8b4;font-family:'Consolas';");
        gl->setFixedWidth(36);gainLabels_.append(gl);col->addWidget(gl);
        auto* sl=new QSlider(Qt::Vertical,this);
        sl->setRange(-120,120);sl->setValue(0);sl->setFixedWidth(24);sl->setMinimumHeight(100);
        sl->setStyleSheet("QSlider::groove:vertical{background:#333;width:4px;border-radius:2px;}"
                          "QSlider::handle:vertical{background:#00c8b4;width:14px;height:14px;margin:-5px -5px;border-radius:7px;}"
                          "QSlider::add-page:vertical{background:#00c8b4;border-radius:2px;}"
                          "QSlider::sub-page:vertical{background:#555;border-radius:2px;}");
        connect(sl,&QSlider::valueChanged,this,[this,i,gl](int v){
            double dB=v/10.0;
            gl->setText(dB>=0?QString("+%1").arg(dB,0,'f',1):QString("%1").arg(dB,0,'f',1));
            QVector<double> gains;
            for(auto* s:sliders_) gains.append(s->value()/10.0);
            emit eqChanged(gains);
        });
        sliders_.append(sl);col->addWidget(sl,1,Qt::AlignHCenter);
        auto* bl=new QLabel(kBands[i],this);bl->setAlignment(Qt::AlignCenter);
        bl->setStyleSheet("font-size:9px;color:#666;");bl->setFixedWidth(36);col->addWidget(bl);
        srow->addLayout(col);
    }
    root->addLayout(srow,1);
    auto* br=new QPushButton("Reset",this);
    br->setStyleSheet("QPushButton{background:#222;color:#aaa;border:1px solid #444;border-radius:4px;padding:4px 16px;font-size:12px;}"
                      "QPushButton:hover{background:#2a2a2a;color:white;}");
    connect(br,&QPushButton::clicked,this,[this](){applyPreset(0);});
    auto* brow=new QHBoxLayout;brow->addWidget(br);brow->addStretch();root->addLayout(brow);
}

void EqPanel::applyPreset(int idx){
    if(idx<0||idx>=kPresets.size()) return;
    const auto& p=kPresets[idx];
    for(int i=0;i<sliders_.size()&&i<p.size();++i) sliders_[i]->setValue(qRound(p[i]*10));
}

// PlaylistPanel
PlaylistPanel::PlaylistPanel(QWidget* parent):QWidget(parent){
    setStyleSheet("background:#141414;color:white;");
    auto* root=new QVBoxLayout(this);root->setContentsMargins(12,10,12,10);root->setSpacing(6);
    auto* hdr=new QHBoxLayout;
    auto* ttl=new QLabel("PLAYLIST",this);
    ttl->setStyleSheet("font-size:13px;font-weight:bold;color:white;letter-spacing:2px;");
    hdr->addWidget(ttl);hdr->addStretch();root->addLayout(hdr);
    searchEdit_=new QLineEdit(this);searchEdit_->setPlaceholderText("곡 검색...");
    searchEdit_->setStyleSheet("QLineEdit{background:#1e1e1e;border:1px solid #333;border-radius:4px;color:white;padding:4px 8px;font-size:12px;}");
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
    dominantColor_=kTeal;
    specBins_.fill(0.0f,64);specPeak_.fill(0.0f,64);
    peakTimer_=new QTimer(this);peakTimer_->setInterval(50);
    connect(peakTimer_,&QTimer::timeout,this,[this](){
        bool ch=false;
        for(int i=0;i<specPeak_.size();++i) if(specPeak_[i]>0.002f){specPeak_[i]*=0.92f;ch=true;}
        if(ch) update();
    });
    peakTimer_->start();
    rotationTimer_=new QTimer(this);rotationTimer_->setInterval(33);
    connect(rotationTimer_,&QTimer::timeout,this,&MusicWidget::onRotationTick);
    setupUI();setupConnections();
}

void MusicWidget::setupUI(){
    setStyleSheet("background:transparent;");
    auto* root=new QVBoxLayout(this);root->setContentsMargins(0,0,0,0);root->setSpacing(0);
    auto* content=new QHBoxLayout;content->setContentsMargins(20,16,20,8);content->setSpacing(20);
    auto* leftPanel=new QVBoxLayout;leftPanel->setSpacing(8);leftPanel->setAlignment(Qt::AlignTop|Qt::AlignHCenter);
    auto* artSpacer=new QWidget(this);artSpacer->setFixedSize(280,280);artSpacer->setAttribute(Qt::WA_TransparentForMouseEvents);
    leftPanel->addWidget(artSpacer,0,Qt::AlignHCenter);leftPanel->addSpacing(12);
    titleLabel_=new QLabel("트랙 제목",this);titleLabel_->setAlignment(Qt::AlignCenter);
    titleLabel_->setStyleSheet("font-size:20px;font-weight:bold;color:white;");titleLabel_->setWordWrap(true);
    leftPanel->addWidget(titleLabel_);
    artistLabel_=new QLabel("아티스트",this);artistLabel_->setAlignment(Qt::AlignCenter);
    artistLabel_->setStyleSheet("font-size:14px;color:#aaa;");leftPanel->addWidget(artistLabel_);
    albumLabel_=new QLabel("",this);albumLabel_->setAlignment(Qt::AlignCenter);
    albumLabel_->setStyleSheet("font-size:12px;color:#666;");leftPanel->addWidget(albumLabel_);
    badgeRow_=new QWidget(this);badgeRow_->setStyleSheet("background:transparent;");
    auto* bl=new QHBoxLayout(badgeRow_);bl->setContentsMargins(0,4,0,4);bl->setSpacing(4);bl->setAlignment(Qt::AlignCenter);
    codecBadge_=makeBadge("FLAC","#00c8b4",badgeRow_);bitBadge_=makeBadge("24bit","#4488ff",badgeRow_);
    rateBadge_=makeBadge("192kHz","#aa44ff",badgeRow_);chBadge_=makeBadge("Stereo","#888",badgeRow_);
    bpBadge_=makeBadge("BIT-PERFECT","#00cc44",badgeRow_);
    bl->addWidget(codecBadge_);bl->addWidget(bitBadge_);bl->addWidget(rateBadge_);bl->addWidget(chBadge_);bl->addWidget(bpBadge_);
    leftPanel->addWidget(badgeRow_);
    auto* specSpacer=new QWidget(this);specSpacer->setFixedHeight(60);specSpacer->setAttribute(Qt::WA_TransparentForMouseEvents);
    leftPanel->addWidget(specSpacer);
    auto* ctrlRow=new QHBoxLayout;ctrlRow->setSpacing(16);ctrlRow->setAlignment(Qt::AlignHCenter);
    auto makeCtrlBtn=[&](const QString& text,bool big=false){
        auto* btn=new QPushButton(text,this);int sz=big?48:32;btn->setFixedSize(sz,sz);
        if(big) btn->setStyleSheet(QString("QPushButton{background:#00c8b4;color:#0e0e0e;border-radius:%1px;font-size:18px;font-weight:bold;border:none;}"
                                           "QPushButton:hover{background:#00e0cc;}").arg(sz/2));
        else btn->setStyleSheet("QPushButton{background:transparent;color:#aaa;border:none;font-size:16px;}"
                                "QPushButton:hover{color:white;}");
        return btn;
    };
    btnShuffle_=makeCtrlBtn("⇌");btnPrev_=makeCtrlBtn("⏮");btnPlay_=makeCtrlBtn("▶",true);
    btnNext_=makeCtrlBtn("⏭");btnRepeat_=makeCtrlBtn("↺");
    ctrlRow->addWidget(btnShuffle_);ctrlRow->addWidget(btnPrev_);ctrlRow->addWidget(btnPlay_);
    ctrlRow->addWidget(btnNext_);ctrlRow->addWidget(btnRepeat_);
    leftPanel->addLayout(ctrlRow);leftPanel->addStretch();
    auto* rightPanel=new QVBoxLayout;rightPanel->setSpacing(0);
    auto* tabRow=new QHBoxLayout;tabRow->setSpacing(0);tabRow->setContentsMargins(0,0,0,0);
    auto makeTabBtn=[&](const QString& text){
        auto* btn=new QPushButton(text,this);btn->setCheckable(true);
        btn->setStyleSheet("QPushButton{background:#1a1a1a;color:#666;border:none;padding:6px 16px;font-size:12px;border-bottom:2px solid transparent;}"
                           "QPushButton:checked{color:#00c8b4;border-bottom:2px solid #00c8b4;}"
                           "QPushButton:hover{color:#aaa;}");
        return btn;
    };
    btnShowLyrics_=makeTabBtn("가사");btnShowEq_=makeTabBtn("EQ");btnShowPlaylist_=makeTabBtn("재생목록");
    btnShowLyrics_->setChecked(true);
    tabRow->addWidget(btnShowLyrics_);tabRow->addWidget(btnShowEq_);tabRow->addWidget(btnShowPlaylist_);tabRow->addStretch();
    rightPanel->addLayout(tabRow);
    rightStack_=new QStackedWidget(this);rightStack_->setStyleSheet("background:#141414;border-radius:8px;");
    lyricsWidget_=new LyricsWidget(this);eqPanel_=new EqPanel(this);playlistPanel_=new PlaylistPanel(this);
    rightStack_->addWidget(lyricsWidget_);rightStack_->addWidget(eqPanel_);rightStack_->addWidget(playlistPanel_);
    rightStack_->setCurrentIndex(0);rightPanel->addWidget(rightStack_,1);
    auto* leftWidget=new QWidget(this);leftWidget->setLayout(leftPanel);leftWidget->setFixedWidth(320);
    content->addWidget(leftWidget);content->addLayout(rightPanel,1);root->addLayout(content,1);
    auto* seekRow=new QHBoxLayout;seekRow->setContentsMargins(20,0,20,4);seekRow->setSpacing(8);
    timeCurrent_=new QLabel("00:00",this);timeCurrent_->setStyleSheet("font-size:12px;color:#888;font-family:'Consolas';");timeCurrent_->setFixedWidth(42);
    seekSlider_=new QSlider(Qt::Horizontal,this);seekSlider_->setRange(0,1000);
    seekSlider_->setStyleSheet("QSlider::groove:horizontal{background:#2a2a2a;height:4px;border-radius:2px;}"
                               "QSlider::handle:horizontal{background:#00c8b4;width:12px;height:12px;margin:-4px 0;border-radius:6px;}"
                               "QSlider::sub-page:horizontal{background:#00c8b4;border-radius:2px;}");
    timeDuration_=new QLabel("00:00",this);timeDuration_->setStyleSheet("font-size:12px;color:#888;font-family:'Consolas';");
    timeDuration_->setFixedWidth(42);timeDuration_->setAlignment(Qt::AlignRight);
    seekRow->addWidget(timeCurrent_);seekRow->addWidget(seekSlider_,1);seekRow->addWidget(timeDuration_);root->addLayout(seekRow);
    auto* bottomRow=new QHBoxLayout;bottomRow->setContentsMargins(20,4,20,8);bottomRow->setSpacing(8);
    speedLabel_=new QLabel("1.0x",this);speedLabel_->setStyleSheet("font-size:12px;color:#666;font-family:'Consolas';");
    btnVolume_=new QPushButton("🔊",this);btnVolume_->setFixedSize(28,28);
    btnVolume_->setStyleSheet("QPushButton{background:transparent;border:none;font-size:14px;color:#888;}QPushButton:hover{color:white;}");
    volSlider_=new QSlider(Qt::Horizontal,this);volSlider_->setRange(0,100);volSlider_->setValue(100);volSlider_->setFixedWidth(100);
    volSlider_->setStyleSheet("QSlider::groove:horizontal{background:#2a2a2a;height:3px;border-radius:2px;}"
                              "QSlider::handle:horizontal{background:#888;width:10px;height:10px;margin:-3.5px 0;border-radius:5px;}"
                              "QSlider::sub-page:horizontal{background:#666;border-radius:2px;}");
    btnMini_=new QPushButton("⊟",this);btnMini_->setFixedSize(28,28);btnMini_->setToolTip("미니 플레이어 (M)");
    btnMini_->setStyleSheet("QPushButton{background:transparent;border:none;font-size:14px;color:#888;}QPushButton:hover{color:white;}");
    btnSettings_=new QPushButton("⚙",this);btnSettings_->setFixedSize(28,28);
    btnSettings_->setStyleSheet("QPushButton{background:transparent;border:none;font-size:14px;color:#888;}QPushButton:hover{color:white;}");
    bottomRow->addWidget(speedLabel_);bottomRow->addStretch();
    bottomRow->addWidget(btnVolume_);bottomRow->addWidget(volSlider_);bottomRow->addSpacing(8);
    bottomRow->addWidget(btnMini_);bottomRow->addWidget(btnSettings_);root->addLayout(bottomRow);
    statusBar_=new QLabel("FLAC  ·  DECODE  ·  2.0  ·  192kHz  ·  24bit  ·  BIT-PERFECT",this);
    statusBar_->setAlignment(Qt::AlignCenter);
    statusBar_->setStyleSheet("font-size:10px;color:#555;font-family:'Consolas';background:#0a0a0a;padding:4px 0;");
    root->addWidget(statusBar_);
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
    connect(seekSlider_,&QSlider::sliderMoved,this,[this](int v){
        if(duration_>0) emit seekRequested(duration_*v/1000.0);
    });
    connect(volSlider_,&QSlider::valueChanged,this,&MusicWidget::volumeChanged);
    connect(btnMini_,&QPushButton::clicked,this,&MusicWidget::miniModeRequested);
    connect(btnSettings_,&QPushButton::clicked,this,&MusicWidget::settingsRequested);
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

void MusicWidget::onRightPanelToggle(int panel){
    rightStack_->setCurrentIndex(panel);
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
    if(!meta.albumArt.isNull()) updateAlbumArt(meta.albumArt);
    else { albumArtPixmap_=QPixmap(); updateBlurBackground(QPixmap()); dominantColor_=kTeal; }
    QString status=QString("%1  ·  DECODE  ·  %2  ·  %3  ·  %4  ·  BIT-PERFECT")
        .arg(meta.codec.toUpper())
        .arg(meta.channels==2?"2.0":QString("%1ch").arg(meta.channels))
        .arg(rateBadge_->text()).arg(bitBadge_->text());
    if(meta.hasReplayGain) status+=QString("  ·  ReplayGain: %1dB").arg(meta.replayGain,0,'f',1);
    statusBar_->setText(status);
    if(lyricsWidget_) lyricsWidget_->loadForTrack(meta.title,meta.artist,meta.album,"");
    update();
}

void MusicWidget::updateAlbumArt(const QPixmap& art){
    albumArtPixmap_=art;
    updateBlurBackground(art);
    dominantColor_=extractDominant(art);
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
    if(playing) rotationTimer_->start();
    else rotationTimer_->stop();
    update();
}

void MusicWidget::updateSpectrum(const QVector<float>& bins){
    specBins_=bins;
    for(int i=0;i<qMin(bins.size(),specPeak_.size());++i)
        if(bins[i]>specPeak_[i]) specPeak_[i]=bins[i];
    update();
}

void MusicWidget::setMiniMode(bool mini){miniMode_=mini;}

void MusicWidget::paintEvent(QPaintEvent*){
    QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.setRenderHint(QPainter::SmoothPixmapTransform);
    if(!blurBg_.isNull()){
        p.drawPixmap(rect(),blurBg_.scaled(size(),Qt::IgnoreAspectRatio,Qt::SmoothTransformation));
        p.fillRect(rect(),QColor(0,0,0,60));
    } else p.fillRect(rect(),kDark);
    QRect artRect(20+(320-280)/2,16,280,280);
    drawAlbumArt(p,artRect);
    QRect specRect(20,artRect.bottom()+12+80+30+26,320,60);
    drawSpectrum(p,specRect);
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
    if(!currentMeta_.albumArt.isNull()) updateBlurBackground(currentMeta_.albumArt);
}

QString MusicWidget::formatTime(double secs) const{
    int s=qRound(secs);
    return QString("%1:%2").arg(s/60,2,10,QChar('0')).arg(s%60,2,10,QChar('0'));
}
