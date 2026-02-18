#ifndef CUESHEETPARSER_H
#define CUESHEETPARSER_H

#include <QString>
#include <QVector>
#include <QtGlobal>

struct CueTrackSegment {
    QString sourceFilePath;
    QString cueSheetPath;
    QString title;
    QString performer;
    QString album;
    int trackNumber = 0;
    qint64 startMs = 0;
    qint64 endMs = -1;
};

class CueSheetParser
{
public:
    static bool parseFile(const QString &cuePath,
                          QVector<CueTrackSegment> *segments,
                          QString *errorMessage = nullptr);
};

#endif // CUESHEETPARSER_H
