#pragma once
#include <QObject>

class SubtitleManager : public QObject {
    Q_OBJECT
public:
    explicit SubtitleManager(QObject* parent = nullptr);
};
