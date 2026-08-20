#ifndef DSPCAPABILITIES_H
#define DSPCAPABILITIES_H

#include <QString>
#include <QStringList>
#include <QVariantMap>
#include "playback/PlaybackBackendRouting.h"

namespace WaveFlux::Dsp {

struct SourceAudioContext {
    PlaybackBackendKind backendKind = PlaybackBackendKind::GStreamer;
    int channelCount = 2;
    int sampleRate = 44100;
    bool isLiveStream = false;
    bool isRemoteTracker = false;
    bool isReverse = false;
    bool isCueSubtrack = false;
};

class DspCapabilities
{
public:
    static QStringList allCapabilityKeys();

    static bool isCapabilitySupported(const QString &capabilityKey,
                                      const SourceAudioContext &context);

    static QString getCapabilityReason(const QString &capabilityKey,
                                       const SourceAudioContext &context);

    static QVariantMap getCapabilitiesMap(const SourceAudioContext &context);
    static QVariantMap getCapabilityReasonsMap(const SourceAudioContext &context);
};

} // namespace WaveFlux::Dsp

#endif // DSPCAPABILITIES_H
