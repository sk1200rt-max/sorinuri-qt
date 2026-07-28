#include "Settings.h"

Settings* Settings::instance_ = nullptr;

Settings::Settings(QObject* parent)
    : QObject(parent), s_("Sorinuri", "SorinuriPlayer") {}

Settings* Settings::instance() {
    if (!instance_) instance_ = new Settings();
    return instance_;
}

int     Settings::volume()           const { return s_.value("audio/volume", 100).toInt(); }
bool    Settings::wasapiExclusive()  const { return s_.value("audio/exclusive", true).toBool(); }
bool    Settings::audioPassthrough() const { return s_.value("audio/passthrough", true).toBool(); }
QString Settings::audioDevice()      const { return s_.value("audio/device", "").toString(); }
QString Settings::hwdec()            const { return s_.value("video/hwdec", "d3d11va").toString(); }

void Settings::setVolume(int v)              { s_.setValue("audio/volume", v); }
void Settings::setWasapiExclusive(bool v)    { s_.setValue("audio/exclusive", v); }
void Settings::setAudioPassthrough(bool v)   { s_.setValue("audio/passthrough", v); }
void Settings::setAudioDevice(const QString& v) { s_.setValue("audio/device", v); }
