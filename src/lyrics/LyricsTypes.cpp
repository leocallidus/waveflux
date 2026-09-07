#include "LyricsTypes.h"
#include <QCryptographicHash>

namespace WaveFlux::Lyrics {

QString TrackSnapshot::cacheKey() const
{
    if (!filePath.isEmpty()) {
        if (isCueTrack) {
            return QStringLiteral("%1::cue_%2").arg(filePath).arg(cueStartMs);
        }
        return filePath;
    }
    return QStringLiteral("track::%1::%2::%3").arg(artist).arg(title).arg(durationMs);
}

QString TrackSnapshot::normalizedLookupKey() const
{
    return QStringLiteral("%1::%2::%3::%4")
        .arg(title.trimmed().toLower())
        .arg(artist.trimmed().toLower())
        .arg(album.trimmed().toLower())
        .arg(durationMs / 1000);
}

bool TrackSnapshot::hasMinimalMetadata() const
{
    return !title.trimmed().isEmpty() && !artist.trimmed().isEmpty();
}

} // namespace WaveFlux::Lyrics
