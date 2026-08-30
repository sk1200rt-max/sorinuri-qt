#include "TrackSelector.h"
#include "UiTheme.h"
#include <QDebug>
#include <QAbstractItemView>
#include <QFileInfo>
#include <QRegularExpression>

namespace {

QString displayLanguageName(const QString& language, const QString& evidence) {
    const QString code = language.trimmed().toLower();
    if (code == "ko" || code == "kor" || code == "korean" || code == "ko-kr")
        return QStringLiteral("한국어");
    if (code == "en" || code == "eng" || code == "english")
        return QStringLiteral("English");
    if (code == "ja" || code == "jpn" || code == "japanese")
        return QStringLiteral("日本語");
    if (code == "zh" || code == "zho" || code == "chi" || code == "chs" || code == "cht")
        return QStringLiteral("中文");

    // SMI/SRT 같은 외부 자막은 언어 메타데이터가 'und'인 경우가 흔하다.
    // 이때 파일명·트랙 제목의 한글 또는 한국어 언어 태그를 근거로만 한국어로 표시한다.
    if (code.isEmpty() || code == "und") {
        const QString lower = evidence.toLower();
        static const QRegularExpression hangul(QStringLiteral("[\\x{AC00}-\\x{D7A3}]"));
        const bool koreanTag = lower.contains(".ko.") || lower.contains("_ko_") ||
                               lower.contains("-ko-") || lower.contains("[ko]") ||
                               lower.contains(".kor.") || lower.contains("_kor_") ||
                               lower.contains("-kor-") || lower.contains("korean") ||
                               lower.contains(QStringLiteral("한국"));
        if (koreanTag || hangul.match(evidence).hasMatch())
            return QStringLiteral("한국어(추정)");
        return QStringLiteral("언어 미지정");
    }
    return language;
}

QString subtitleFormatName(const QString& codec) {
    const QString normalized = codec.trimmed().toLower();
    if (normalized == "subrip" || normalized == "srt") return QStringLiteral("SRT");
    if (normalized == "ass" || normalized == "ssa") return QStringLiteral("ASS");
    if (normalized == "sami" || normalized == "smi") return QStringLiteral("SMI");
    if (normalized == "webvtt" || normalized == "vtt") return QStringLiteral("VTT");
    if (normalized == "hdmv_pgs_subtitle" || normalized == "pgs") return QStringLiteral("PGS");
    return codec.isEmpty() ? QStringLiteral("자막") : codec.toUpper();
}

} // namespace

TrackSelector::TrackSelector(QWidget* parent) : QWidget(parent) {
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    QLabel* audioLabel = new QLabel("오디오", this);
    audioLabel->setStyleSheet("color: #A3B1B0; font-size: 11px; font-weight: 700; padding-left: 2px;");
    layout->addWidget(audioLabel);

    audioCombo_ = new QComboBox(this);
    audioCombo_->setMinimumWidth(190);
    audioCombo_->setMaximumWidth(300);
    audioCombo_->setStyleSheet(SorinuriUi::comboBoxStyle());
    audioCombo_->setFocusPolicy(Qt::NoFocus);
    layout->addWidget(audioCombo_);

    QLabel* subLabel = new QLabel("자막", this);
    subLabel->setStyleSheet("color: #A3B1B0; font-size: 11px; font-weight: 700; padding-left: 4px;");
    layout->addWidget(subLabel);

    subCombo_ = new QComboBox(this);
    // 하단 컨트롤바는 좁은 창에서도 유지하고, 펼친 목록에서 전체 정보를 읽을 수 있게 한다.
    subCombo_->setMinimumWidth(200);
    subCombo_->setMaximumWidth(320);
    subCombo_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    subCombo_->setMinimumContentsLength(16);
    subCombo_->view()->setMinimumWidth(380);
    subCombo_->setToolTip("선택된 자막의 언어, 파일명 및 형식을 표시합니다.");
    subCombo_->setStyleSheet(SorinuriUi::comboBoxStyle());
    subCombo_->setFocusPolicy(Qt::NoFocus);
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
    subCombo_->setItemData(0, "자막을 표시하지 않습니다.", Qt::ToolTipRole);

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
            const QString externalFile = track.value("external-filename").toString();
            const QString sourceName = externalFile.isEmpty()
                ? title
                : QFileInfo(externalFile).fileName();
            const QString evidence = title + " " + externalFile;
            const QString languageName = displayLanguageName(lang, evidence);
            const QString formatName = subtitleFormatName(codec);

            // 예: "#31  한국어(추정) · movie.ko.smi · SMI"
            QString label = QString("#%1  %2").arg(id).arg(languageName);
            if (!sourceName.isEmpty()) label += " · " + sourceName;
            label += " · " + formatName;

            const int comboIndex = subCombo_->count();
            subCombo_->addItem(label, id);
            QString detail = QString("트랙 #%1\n언어 메타데이터: %2\n형식: %3")
                .arg(id)
                .arg(lang.isEmpty() ? "und" : lang)
                .arg(formatName);
            if (!externalFile.isEmpty()) detail += "\n파일: " + externalFile;
            else if (!title.isEmpty()) detail += "\n제목: " + title;
            subCombo_->setItemData(comboIndex, detail, Qt::ToolTipRole);
            if (id == currentSub) subCombo_->setCurrentIndex(comboIndex);
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
