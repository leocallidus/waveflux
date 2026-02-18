#ifndef XSPFPLAYLISTPARSER_H
#define XSPFPLAYLISTPARSER_H

#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>

struct XspfTrackEntry {
    QString source;
    QString title;
    QString creator;
    QString album;
    qint64 durationMs = -1;
};

struct XspfParseError {
    QString message;
    qint64 line = -1;
    qint64 column = -1;
};

class XspfPlaylistParser
{
public:
    static bool parseFile(const QString &xspfPath,
                          QVector<XspfTrackEntry> *entries,
                          QStringList *warnings = nullptr,
                          XspfParseError *error = nullptr);
};

#endif // XSPFPLAYLISTPARSER_H
