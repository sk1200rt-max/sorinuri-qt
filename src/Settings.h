#pragma once
#include <QObject>
#include <QSettings>

class Settings : public QObject {
    Q_OBJECT
public:
    explicit Settings(QObject* parent = nullptr);
    static Settings* instance();

    int     volume() const;
    bool    wasapiExclusive() const;
    bool    audioPassthrough() const;
    QString audioDevice() const;
    QString hwdec() const;

    void setVolume(int v);
    void setWasapiExclusive(bool v);
    void setAudioPassthrough(bool v);
    void setAudioDevice(const QString& v);

private:
    QSettings s_;
    static Settings* instance_;
};
