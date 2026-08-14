#include "PlaybackQueue.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>

namespace {
constexpr auto kPlaylistRoot = "playbackQueue/savedPlaylists";

QJsonObject toJson(const PlaybackQueue::Entry& entry) {
    return QJsonObject{
        {"url", entry.url},
        {"title", entry.title},
        {"artist", entry.artist},
        {"artworkUrl", entry.artworkUrl},
        {"source", entry.source},
    };
}

PlaybackQueue::Entry fromJson(const QJsonObject& object) {
    PlaybackQueue::Entry entry;
    entry.url = object.value("url").toString();
    entry.title = object.value("title").toString();
    entry.artist = object.value("artist").toString();
    entry.artworkUrl = object.value("artworkUrl").toString();
    entry.source = object.value("source").toString();
    return entry;
}
}

PlaybackQueue::PlaybackQueue(QObject* parent) : QObject(parent) {
    const int savedMode = settings().value("playbackQueue/repeatMode", 0).toInt();
    repeatMode_ = static_cast<RepeatMode>(qBound(0, savedMode, 2));
}

QSettings& PlaybackQueue::settings() {
    static QSettings instance;
    return instance;
}

void PlaybackQueue::replace(const QList<Entry>& entries, int startIndex) {
    entries_.clear();
    for (const Entry& entry : entries) {
        if (!entry.url.trimmed().isEmpty())
            entries_.append(entry);
    }
    currentIndex_ = entries_.isEmpty() ? -1 : qBound(0, startIndex, entries_.size() - 1);
    emit queueChanged();
    emit currentChanged(currentIndex_);
}

void PlaybackQueue::clear() {
    entries_.clear();
    currentIndex_ = -1;
    emit queueChanged();
    emit currentChanged(currentIndex_);
}

PlaybackQueue::Entry PlaybackQueue::currentEntry() const {
    return (currentIndex_ >= 0 && currentIndex_ < entries_.size())
        ? entries_.at(currentIndex_) : Entry{};
}

QStringList PlaybackQueue::urls() const {
    QStringList result;
    for (const Entry& entry : entries_)
        result.append(entry.url);
    return result;
}

void PlaybackQueue::setCurrentUrl(const QString& url) {
    const int index = std::find_if(entries_.cbegin(), entries_.cend(),
        [&url](const Entry& entry) { return entry.url == url; }) - entries_.cbegin();
    if (index < 0 || index >= entries_.size() || currentIndex_ == index)
        return;
    currentIndex_ = index;
    emit currentChanged(currentIndex_);
}

void PlaybackQueue::setRepeatMode(RepeatMode mode) {
    if (repeatMode_ == mode) return;
    repeatMode_ = mode;
    settings().setValue("playbackQueue/repeatMode", static_cast<int>(mode));
    emit repeatModeChanged(repeatMode_);
}

bool PlaybackQueue::saveNamedPlaylist(const QString& name, const QList<Entry>& entries) {
    const QString cleanName = name.trimmed();
    if (cleanName.isEmpty() || entries.isEmpty()) return false;

    QJsonArray array;
    for (const Entry& entry : entries) {
        if (!entry.url.trimmed().isEmpty())
            array.append(toJson(entry));
    }
    if (array.isEmpty()) return false;

    settings().setValue(QStringLiteral("%1/%2").arg(kPlaylistRoot, cleanName),
                        QJsonDocument(array).toJson(QJsonDocument::Compact));
    return true;
}

QStringList PlaybackQueue::savedPlaylistNames() const {
    QSettings& s = settings();
    s.beginGroup(kPlaylistRoot);
    const QStringList names = s.childKeys();
    s.endGroup();
    return names;
}

QList<PlaybackQueue::Entry> PlaybackQueue::loadNamedPlaylist(const QString& name) const {
    QList<Entry> result;
    const QByteArray json = settings().value(QStringLiteral("%1/%2").arg(kPlaylistRoot, name)).toByteArray();
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isArray()) return result;
    for (const QJsonValue& value : doc.array()) {
        const Entry entry = fromJson(value.toObject());
        if (!entry.url.trimmed().isEmpty())
            result.append(entry);
    }
    return result;
}

bool PlaybackQueue::removeNamedPlaylist(const QString& name) {
    const QString key = QStringLiteral("%1/%2").arg(kPlaylistRoot, name);
    if (!settings().contains(key)) return false;
    settings().remove(key);
    return true;
}
