#ifndef TRACKINFOFORMATTER_H
#define TRACKINFOFORMATTER_H

#include <QString>

namespace WaveFlux {

class TrackInfoFormatter
{
public:
    enum class RenderContext {
        WindowTitle,
        Playlist,
        WaveformOverlay,
        WaveformTooltip
    };

    struct TrackInfoContext {
        QString artist;
        QString title;
        QString album;
        QString comment;
        QString genre;
        QString year;
        QString trackNumber;
        int playlistIndex = -1;
        int playlistCount = 0;
        qint64 positionMs = -1;
        qint64 hoverPositionMs = -1;
        qint64 durationMs = -1;
        qint64 playlistDurationMs = -1;
        int bitDepth = 0;
        int bitrateKbps = 0;
        int sampleRateHz = 0;
        int channelCount = 0;
        int bpm = 0;
        QString filePath;
        QString appVersion;
    };

    static QString render(const QString &format,
                          const TrackInfoContext &track,
                          RenderContext renderContext);

private:
    TrackInfoFormatter() = delete;
};

} // namespace WaveFlux

#endif // TRACKINFOFORMATTER_H
