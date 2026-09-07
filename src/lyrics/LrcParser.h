#pragma once

#include "LyricsTypes.h"
#include <QByteArray>
#include <QString>
#include <vector>

namespace WaveFlux::Lyrics {

struct ParsedLyrics {
    LyricsKind kind = LyricsKind::None;
    QString title;
    QString artist;
    QString album;
    qint64 offsetMs = 0;
    std::vector<CueGroup> cues;
    QString plainText;
    QString contentHash;
    bool valid = false;
    QString error;
};

class LrcParser {
public:
    static ParsedLyrics parse(const QByteArray &data);
    static ParsedLyrics parsePlainText(const QString &text);
    static QString stripEnhancedTags(const QString &line);
    static bool parseTimestamp(const QString &tag, qint64 &outMs);
};

} // namespace WaveFlux::Lyrics
