#include "TrackSelector.h"
#include <QDebug>

TrackSelector::TrackSelector(QWidget* parent) : QWidget(parent) {
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    QLabel* audioLabel = new QLabel("오디오:", this);
    audioLabel->setStyleSheet("color: #888; font-size: 11px;");
    layout->addWidget(audioLabel);

    audioCombo_ = new QComboBox(this);
    audioCombo_->setMinimumWidth(180);
    audioCombo_->setMaximumWidth(280);
    audioCombo_->setStyleSheet(R"(
        QComboBox {
            background: #1e1e1e;
            border: 1px solid #333;
            border-radius: 3px;
            padding: 3px 6px;
            color: #e0e0e0;
            font-size: 11px;
        }
        QComboBox::drop-down { border: none; width: 16px; }
        QComboBox QAbstractItemView {
            background: #1e1e1e;
            color: #e0e0e0;
            selection-background-color: #1a3a5c;
            font-size: 11px;
        }
    )");
    layout->addWidget(audioCombo_);

    QLabel* subLabel = new QLabel("자막:", this);
    subLabel->setStyleSheet("color: #888; font-size: 11px;");
    layout->addWidget(subLabel);

    subCombo_ = new QComboBox(this);
    subCombo_->setMinimumWidth(140);
    subCombo_->setMaximumWidth(220);
    subCombo_->setStyleSheet(audioCombo_->styleSheet());
    layout->addWidget(subCombo_);

    connect(audioCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TrackSelector::onAudioTrackChanged);
    connect(subCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TrackSelector::onSubTrackChanged);
}

void TrackSelector::connectMpv(MpvCore* core) {
    mpv_ = core;
    connect(core, &MpvCore::fileLoaded, this, &TrackSelector::onTracksChanged);
    connect(core, &MpvCore::tracksChanged, this, &TrackSelector::onTracksChanged);
}

void TrackSelector::onTracksChanged() {
    if (!mpv_) return;
    updating_ = true;

    audioCombo_->clear();
    subCombo_->clear();
    subCombo_->addItem("자막 없음", 0);

    // MPV track-list 속성 파싱
    QVariant trackList = mpv_->getProperty("track-list");
    // track-list는 mpv_node 배열이므로 실제로는 QVariantList로 반환됨
    // 각 트랙은 QVariantMap으로 id, type, lang, codec, title 등을 포함

    QVariantList tracks = trackList.toList();
    int currentAudio = mpv_->getProperty("aid").toInt();
    int currentSub   = mpv_->getProperty("sid").toInt();

    int audioIdx = 0, subIdx = 0;

    for (const QVariant& t : tracks) {
        QVariantMap track = t.toMap();
        QString type  = track["type"].toString();
        int     id    = track["id"].toInt();
        QString lang  = track["lang"].toString();
        QString codec = track["codec"].toString();
        QString title = track["title"].toString();

        if (type == "audio") {
            // 오디오 트랙 표시: "1: DTS-HD MA 7.1 [kor]"
            QString channels = track["demux-channel-count"].toString();
            QString sampleRate = track["demux-samplerate"].toString();
            QString label = QString("%1: %2 %3ch [%4]")
                .arg(id)
                .arg(codec.toUpper())
                .arg(channels)
                .arg(lang.isEmpty() ? "und" : lang);
            if (!title.isEmpty()) label += " - " + title;
            audioCombo_->addItem(label, id);
            if (id == currentAudio) audioCombo_->setCurrentIndex(audioIdx);
            audioIdx++;
        } else if (type == "sub") {
            QString label = QString("%1: [%2]").arg(id).arg(lang.isEmpty() ? "und" : lang);
            if (!title.isEmpty()) label += " " + title;
            subCombo_->addItem(label, id);
            if (id == currentSub) subCombo_->setCurrentIndex(subIdx + 1);
            subIdx++;
        }
    }

    if (audioCombo_->count() == 0) {
        audioCombo_->addItem("오디오 없음", 0);
    }

    updating_ = false;
}

void TrackSelector::onAudioTrackChanged(int index) {
    if (updating_ || !mpv_) return;
    int id = audioCombo_->itemData(index).toInt();
    if (id > 0) mpv_->setProperty("aid", id);
}

void TrackSelector::onSubTrackChanged(int index) {
    if (updating_ || !mpv_) return;
    int id = subCombo_->itemData(index).toInt();
    if (id == 0) {
        mpv_->setProperty("sid", "no");
    } else {
        mpv_->setProperty("sid", id);
    }
}

void TrackSelector::refresh() {
    onTracksChanged();
}
