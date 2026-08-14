#pragma once

#include <QObject>
#include <QList>
#include <QSettings>
#include <QStringList>

// 모든 재생 원본(로컬 파일, 소리누리 오리지널, YouTube URL)을 같은 대기열로 관리한다.
class PlaybackQueue : public QObject {
    Q_OBJECT
public:
    struct Entry {
        QString url;
        QString title;
        QString artist;
        QString artworkUrl;
        QString source; // local, originals, youtube
    };

    enum class RepeatMode : int {
        Off = 0,
        All = 1,
        One = 2,
    };

    explicit PlaybackQueue(QObject* parent = nullptr);

    void replace(const QList<Entry>& entries, int startIndex = 0);
    void clear();
    bool isEmpty() const { return entries_.isEmpty(); }
    int count() const { return entries_.size(); }
    int currentIndex() const { return currentIndex_; }
    Entry currentEntry() const;
    QList<Entry> entries() const { return entries_; }
    QStringList urls() const;

    void setCurrentUrl(const QString& url);
    void setRepeatMode(RepeatMode mode);
    RepeatMode repeatMode() const { return repeatMode_; }

    // QSettings에 사용자 재생목록을 JSON으로 저장한다.
    bool saveNamedPlaylist(const QString& name, const QList<Entry>& entries);
    QStringList savedPlaylistNames() const;
    QList<Entry> loadNamedPlaylist(const QString& name) const;
    bool removeNamedPlaylist(const QString& name);

signals:
    void queueChanged();
    void currentChanged(int index);
    void repeatModeChanged(PlaybackQueue::RepeatMode mode);

private:
    static QSettings& settings();
    QList<Entry> entries_;
    int currentIndex_ = -1;
    RepeatMode repeatMode_ = RepeatMode::Off;
};

Q_DECLARE_METATYPE(PlaybackQueue::Entry)
Q_DECLARE_METATYPE(QList<PlaybackQueue::Entry>)
Q_DECLARE_METATYPE(PlaybackQueue::RepeatMode)
