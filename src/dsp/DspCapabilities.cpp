#include "DspCapabilities.h"

namespace WaveFlux::Dsp {

QStringList DspCapabilities::allCapabilityKeys()
{
    static const QStringList keys = {
        QStringLiteral("dsp.echo"),
        QStringLiteral("dsp.chorus"),
        QStringLiteral("dsp.speedVarispeed"),
        QStringLiteral("dsp.reverb"),
        QStringLiteral("dsp.bass"),
        QStringLiteral("dsp.tempo"),
        QStringLiteral("dsp.flanger"),
        QStringLiteral("dsp.stereoWidth"),
        QStringLiteral("dsp.tonality"),
        QStringLiteral("dsp.voiceSuppression"),
        QStringLiteral("dsp.equalizer"),
        QStringLiteral("dsp.amplitudeNormalization"),
        QStringLiteral("dsp.replayGain"),
        QStringLiteral("dsp.crossfade"),
        QStringLiteral("dsp.silenceRemoval")
    };
    return keys;
}

bool DspCapabilities::isCapabilitySupported(const QString &capabilityKey,
                                           const SourceAudioContext &context)
{
    if (capabilityKey == QStringLiteral("dsp.voiceSuppression") ||
        capabilityKey == QStringLiteral("dsp.stereoWidth")) {
        return context.channelCount >= 2;
    }
    if (capabilityKey == QStringLiteral("dsp.crossfade") ||
        capabilityKey == QStringLiteral("dsp.silenceRemoval")) {
        if (context.isLiveStream) {
            return false;
        }
        return true;
    }
    if (capabilityKey == QStringLiteral("dsp.speedVarispeed") ||
        capabilityKey == QStringLiteral("dsp.tempo") ||
        capabilityKey == QStringLiteral("dsp.tonality") ||
        capabilityKey == QStringLiteral("dsp.bass") ||
        capabilityKey == QStringLiteral("dsp.equalizer") ||
        capabilityKey == QStringLiteral("dsp.echo") ||
        capabilityKey == QStringLiteral("dsp.chorus") ||
        capabilityKey == QStringLiteral("dsp.flanger") ||
        capabilityKey == QStringLiteral("dsp.reverb") ||
        capabilityKey == QStringLiteral("dsp.amplitudeNormalization") ||
        capabilityKey == QStringLiteral("dsp.replayGain")) {
        return true;
    }
    return true;
}

QString DspCapabilities::getCapabilityReason(const QString &capabilityKey,
                                            const SourceAudioContext &context)
{
    if ((capabilityKey == QStringLiteral("dsp.voiceSuppression") ||
         capabilityKey == QStringLiteral("dsp.stereoWidth")) && context.channelCount < 2) {
        return QStringLiteral("dsp.reason.monoSource");
    }
    if ((capabilityKey == QStringLiteral("dsp.crossfade") ||
         capabilityKey == QStringLiteral("dsp.silenceRemoval")) && context.isLiveStream) {
        return QStringLiteral("dsp.reason.liveStream");
    }
    if (!isCapabilitySupported(capabilityKey, context)) {
        return QStringLiteral("dsp.reason.notSupported");
    }
    return QString();
}

QVariantMap DspCapabilities::getCapabilitiesMap(const SourceAudioContext &context)
{
    QVariantMap map;
    const auto keys = allCapabilityKeys();
    for (const auto &key : keys) {
        map.insert(key, isCapabilitySupported(key, context));
    }
    return map;
}

QVariantMap DspCapabilities::getCapabilityReasonsMap(const SourceAudioContext &context)
{
    QVariantMap map;
    const auto keys = allCapabilityKeys();
    for (const auto &key : keys) {
        map.insert(key, getCapabilityReason(key, context));
    }
    return map;
}

} // namespace WaveFlux::Dsp
