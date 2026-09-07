#include "LocalLyricsProvider.h"
#include "LrcParser.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace WaveFlux::Lyrics {

LocalLyricsProvider::LocalLyricsProvider(QObject *parent)
    : ILyricsProvider(parent)
{
}

QString LocalLyricsProvider::findSidecarFile(const TrackSnapshot &track)
{
    if (track.filePath.isEmpty() || track.isStream) {
        return QString();
    }

    const QFileInfo fi(track.filePath);
    if (!fi.exists()) {
        return QString();
    }

    const QString dir = fi.absolutePath();
    const QString base = fi.completeBaseName();

    QStringList candidates;
    if (track.isCueTrack) {
        // Formats: base.trackNN.lrc, base.trackN.lrc, etc.
        const int tNum1 = track.trackIndex + 1;
        candidates << QStringLiteral("%1/%2.track%3.lrc").arg(dir, base, QString::number(tNum1).rightJustified(2, u'0'));
        candidates << QStringLiteral("%1/%2.track%3.lrc").arg(dir, base, QString::number(tNum1));
        candidates << QStringLiteral("%1/%2.track%3.lrc").arg(dir, base, QString::number(track.trackIndex).rightJustified(2, u'0'));
        candidates << QStringLiteral("%1/%2.track%3.lrc").arg(dir, base, QString::number(track.trackIndex));
        // Fallback to plain base.lrc
        candidates << QStringLiteral("%1/%2.lrc").arg(dir, base);
    } else {
        candidates << QStringLiteral("%1/%2.lrc").arg(dir, base);
        candidates << QStringLiteral("%1/lyrics/%2.lrc").arg(dir, base);
        candidates << QStringLiteral("%1/%2.txt").arg(dir, base);
    }

    for (const QString &path : candidates) {
        if (QFile::exists(path)) {
            return path;
        }
    }
    return QString();
}

void LocalLyricsProvider::search(const TrackSnapshot &track, const QuerySnapshot &query, quint64 requestId)
{
    Q_UNUSED(query)
    const QString sidecarPath = findSidecarFile(track);
    if (sidecarPath.isEmpty()) {
        emit resultsReady(requestId, {});
        return;
    }

    QFile file(sidecarPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit resultsReady(requestId, {});
        return;
    }

    const QByteArray data = file.read(kMaxLrcFileSize + 1);
    file.close();

    if (data.size() > kMaxLrcFileSize) {
        emit resultsReady(requestId, {});
        return;
    }

    const ParsedLyrics parsed = LrcParser::parse(data);
    if (!parsed.valid) {
        emit resultsReady(requestId, {});
        return;
    }

    LyricsCandidate cand;
    cand.id = parsed.contentHash;
    cand.providerId = providerId();
    cand.kind = parsed.kind;
    cand.hasSynced = (parsed.kind == LyricsKind::Synced);
    cand.isInstrumental = (parsed.kind == LyricsKind::Instrumental);
    cand.lrcText = QString::fromUtf8(data);
    cand.plainText = parsed.plainText;
    cand.title = parsed.title.isEmpty() ? track.title : parsed.title;
    cand.artist = parsed.artist.isEmpty() ? track.artist : parsed.artist;
    cand.album = parsed.album.isEmpty() ? track.album : parsed.album;
    cand.durationMs = track.durationMs;

    emit resultsReady(requestId, {cand});
}

void LocalLyricsProvider::cancel(quint64 requestId)
{
    Q_UNUSED(requestId)
}

} // namespace WaveFlux::Lyrics
