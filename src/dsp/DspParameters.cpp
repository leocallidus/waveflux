#include "DspParameters.h"

#include <cmath>
#include <algorithm>

namespace WaveFlux::Dsp {

QVariantMap ParameterDefinition::toVariantMap() const
{
    QVariantMap map;
    map.insert(QStringLiteral("id"), id);
    map.insert(QStringLiteral("tabId"), tabId);
    map.insert(QStringLiteral("groupId"), groupId);
    map.insert(QStringLiteral("nameKey"), nameKey);
    map.insert(QStringLiteral("descriptionKey"), descriptionKey);
    map.insert(QStringLiteral("type"), static_cast<int>(type));
    map.insert(QStringLiteral("minValue"), minValue);
    map.insert(QStringLiteral("maxValue"), maxValue);
    map.insert(QStringLiteral("step"), step);
    map.insert(QStringLiteral("neutralValue"), neutralValue);
    map.insert(QStringLiteral("defaultValue"), defaultValue);
    map.insert(QStringLiteral("unit"), unit);
    map.insert(QStringLiteral("enumValues"), enumValues);
    return map;
}

const std::vector<ParameterDefinition> &allParameterDefinitions()
{
    static const std::vector<ParameterDefinition> definitions = {
        // --- General Tab Adjustments ---
        {
            QStringLiteral("general.echoMix"),
            QStringLiteral("general"),
            QStringLiteral("adjustments"),
            QStringLiteral("dsp.general.echo"),
            QStringLiteral("dsp.general.echoDesc"),
            ParameterType::Double,
            0.0, 100.0, 1.0, 0.0, 0.0,
            QStringLiteral("%"),
            {}
        },
        {
            QStringLiteral("general.chorusMix"),
            QStringLiteral("general"),
            QStringLiteral("adjustments"),
            QStringLiteral("dsp.general.chorus"),
            QStringLiteral("dsp.general.chorusDesc"),
            ParameterType::Double,
            0.0, 100.0, 1.0, 0.0, 0.0,
            QStringLiteral("%"),
            {}
        },
        {
            QStringLiteral("general.speed"),
            QStringLiteral("general"),
            QStringLiteral("adjustments"),
            QStringLiteral("dsp.general.speed"),
            QStringLiteral("dsp.general.speedDesc"),
            ParameterType::Double,
            0.25, 3.00, 0.01, 1.00, 1.00,
            QStringLiteral("×"),
            {}
        },
        {
            QStringLiteral("general.reverbMix"),
            QStringLiteral("general"),
            QStringLiteral("adjustments"),
            QStringLiteral("dsp.general.reverb"),
            QStringLiteral("dsp.general.reverbDesc"),
            ParameterType::Double,
            0.0, 100.0, 1.0, 0.0, 0.0,
            QStringLiteral("%"),
            {}
        },
        {
            QStringLiteral("general.bass"),
            QStringLiteral("general"),
            QStringLiteral("adjustments"),
            QStringLiteral("dsp.general.bass"),
            QStringLiteral("dsp.general.bassDesc"),
            ParameterType::Double,
            0.00, 2.00, 0.01, 1.00, 1.00,
            QStringLiteral("×"),
            {}
        },
        {
            QStringLiteral("general.tempo"),
            QStringLiteral("general"),
            QStringLiteral("adjustments"),
            QStringLiteral("dsp.general.tempo"),
            QStringLiteral("dsp.general.tempoDesc"),
            ParameterType::Double,
            0.50, 3.00, 0.01, 1.00, 1.00,
            QStringLiteral("×"),
            {}
        },
        {
            QStringLiteral("general.flangerMix"),
            QStringLiteral("general"),
            QStringLiteral("adjustments"),
            QStringLiteral("dsp.general.flanger"),
            QStringLiteral("dsp.general.flangerDesc"),
            ParameterType::Double,
            0.0, 100.0, 1.0, 0.0, 0.0,
            QStringLiteral("%"),
            {}
        },
        {
            QStringLiteral("general.stereoWidth"),
            QStringLiteral("general"),
            QStringLiteral("adjustments"),
            QStringLiteral("dsp.general.stereoWidth"),
            QStringLiteral("dsp.general.stereoWidthDesc"),
            ParameterType::Double,
            1.00, 5.00, 0.01, 1.00, 1.00,
            QStringLiteral("×"),
            {}
        },
        {
            QStringLiteral("general.tonalitySemitones"),
            QStringLiteral("general"),
            QStringLiteral("adjustments"),
            QStringLiteral("dsp.general.tonality"),
            QStringLiteral("dsp.general.tonalityDesc"),
            ParameterType::Double,
            -10.00, 10.00, 0.01, 0.00, 0.00,
            QStringLiteral("st"),
            {}
        },
        // --- General Tab Playback Switches ---
        {
            QStringLiteral("general.voiceSuppression"),
            QStringLiteral("general"),
            QStringLiteral("playback"),
            QStringLiteral("dsp.general.voiceSuppression"),
            QStringLiteral("dsp.general.voiceSuppressionDesc"),
            ParameterType::Bool,
            0.0, 1.0, 1.0, 0.0, 0.0,
            QString(),
            {}
        },
        {
            QStringLiteral("general.fadePauseResume"),
            QStringLiteral("general"),
            QStringLiteral("playback"),
            QStringLiteral("dsp.general.fadePauseResume"),
            QStringLiteral("dsp.general.fadePauseResumeDesc"),
            ParameterType::Bool,
            0.0, 1.0, 1.0, 0.0, 0.0,
            QString(),
            {}
        },
        {
            QStringLiteral("general.fadeTrackNavigation"),
            QStringLiteral("general"),
            QStringLiteral("playback"),
            QStringLiteral("dsp.general.fadeTrackNavigation"),
            QStringLiteral("dsp.general.fadeTrackNavigationDesc"),
            ParameterType::Bool,
            0.0, 1.0, 1.0, 0.0, 0.0,
            QString(),
            {}
        },

        // --- Volume Tab: Volume & Balance ---
        {
            QStringLiteral("volume.smoothChanges"),
            QStringLiteral("volume"),
            QStringLiteral("volumeBalance"),
            QStringLiteral("dsp.volume.smoothChanges"),
            QStringLiteral("dsp.volume.smoothChangesDesc"),
            ParameterType::Bool,
            0.0, 1.0, 1.0, 0.0, 0.0,
            QString(),
            {}
        },
        {
            QStringLiteral("volume.logarithmicControl"),
            QStringLiteral("volume"),
            QStringLiteral("volumeBalance"),
            QStringLiteral("dsp.volume.logarithmicControl"),
            QStringLiteral("dsp.volume.logarithmicControlDesc"),
            ParameterType::Bool,
            0.0, 1.0, 1.0, 0.0, 0.0,
            QString(),
            {}
        },
        {
            QStringLiteral("volume.loudnessCompensation"),
            QStringLiteral("volume"),
            QStringLiteral("volumeBalance"),
            QStringLiteral("dsp.volume.loudnessCompensation"),
            QStringLiteral("dsp.volume.loudnessCompensationDesc"),
            ParameterType::Bool,
            0.0, 1.0, 1.0, 0.0, 0.0,
            QString(),
            {}
        },
        {
            QStringLiteral("volume.balance"),
            QStringLiteral("volume"),
            QStringLiteral("volumeBalance"),
            QStringLiteral("dsp.volume.balance"),
            QStringLiteral("dsp.volume.balanceDesc"),
            ParameterType::Double,
            -1.00, 1.00, 0.01, 0.00, 0.00,
            QString(),
            {}
        },

        // --- Volume Tab: Amplitude Normalization ---
        {
            QStringLiteral("volume.amplitudeNormalization.enabled"),
            QStringLiteral("volume"),
            QStringLiteral("amplitudeNormalization"),
            QStringLiteral("dsp.volume.amplitudeNormalization"),
            QStringLiteral("dsp.volume.amplitudeNormalizationDesc"),
            ParameterType::Bool,
            0.0, 1.0, 1.0, 0.0, 0.0,
            QString(),
            {}
        },
        {
            QStringLiteral("volume.amplitudeNormalization.targetPeakDbfs"),
            QStringLiteral("volume"),
            QStringLiteral("amplitudeNormalization"),
            QStringLiteral("dsp.volume.targetPeakLevel"),
            QStringLiteral("dsp.volume.targetPeakLevelDesc"),
            ParameterType::Double,
            -20.0, 0.0, 0.1, -1.0, -1.0,
            QStringLiteral("dBFS"),
            {}
        },
        {
            QStringLiteral("volume.amplitudeNormalization.preampDb"),
            QStringLiteral("volume"),
            QStringLiteral("amplitudeNormalization"),
            QStringLiteral("dsp.volume.preamp"),
            QStringLiteral("dsp.volume.preampDesc"),
            ParameterType::Double,
            -20.0, 20.0, 0.1, 0.0, 0.0,
            QStringLiteral("dB"),
            {}
        },
        {
            QStringLiteral("volume.amplitudeNormalization.useTagValues"),
            QStringLiteral("volume"),
            QStringLiteral("amplitudeNormalization"),
            QStringLiteral("dsp.volume.useTagValues"),
            QStringLiteral("dsp.volume.useTagValuesDesc"),
            ParameterType::Bool,
            0.0, 1.0, 1.0, 1.0, 1.0,
            QString(),
            {}
        },

        // --- Volume Tab: ReplayGain ---
        {
            QStringLiteral("volume.replayGain.enabled"),
            QStringLiteral("volume"),
            QStringLiteral("replayGain"),
            QStringLiteral("dsp.volume.replayGain"),
            QStringLiteral("dsp.volume.replayGainDesc"),
            ParameterType::Bool,
            0.0, 1.0, 1.0, 0.0, 0.0,
            QString(),
            {}
        },
        {
            QStringLiteral("volume.replayGain.mode"),
            QStringLiteral("volume"),
            QStringLiteral("replayGain"),
            QStringLiteral("dsp.volume.replayGainMode"),
            QStringLiteral("dsp.volume.replayGainModeDesc"),
            ParameterType::StringEnum,
            0.0, 2.0, 1.0, 0.0, 0.0,
            QString(),
            {QStringLiteral("auto"), QStringLiteral("track"), QStringLiteral("album")}
        },
        {
            QStringLiteral("volume.replayGain.fallbackDb"),
            QStringLiteral("volume"),
            QStringLiteral("replayGain"),
            QStringLiteral("dsp.volume.fallbackGain"),
            QStringLiteral("dsp.volume.fallbackGainDesc"),
            ParameterType::Double,
            -20.0, 20.0, 0.1, 0.0, 0.0,
            QStringLiteral("dB"),
            {}
        },
        {
            QStringLiteral("volume.replayGain.analyzeOnTheFly"),
            QStringLiteral("volume"),
            QStringLiteral("replayGain"),
            QStringLiteral("dsp.volume.analyzeOnTheFly"),
            QStringLiteral("dsp.volume.analyzeOnTheFlyDesc"),
            ParameterType::Bool,
            0.0, 1.0, 1.0, 0.0, 0.0,
            QString(),
            {}
        },
        {
            QStringLiteral("volume.replayGain.preampDb"),
            QStringLiteral("volume"),
            QStringLiteral("replayGain"),
            QStringLiteral("dsp.volume.replayGainPreamp"),
            QStringLiteral("dsp.volume.replayGainPreampDesc"),
            ParameterType::Double,
            -20.0, 20.0, 0.1, 0.0, 0.0,
            QStringLiteral("dB"),
            {}
        },
        {
            QStringLiteral("volume.replayGain.useTags"),
            QStringLiteral("volume"),
            QStringLiteral("replayGain"),
            QStringLiteral("dsp.volume.useReplayGainTags"),
            QStringLiteral("dsp.volume.useReplayGainTagsDesc"),
            ParameterType::Bool,
            0.0, 1.0, 1.0, 1.0, 1.0,
            QString(),
            {}
        },
        {
            QStringLiteral("volume.replayGain.tagSource"),
            QStringLiteral("volume"),
            QStringLiteral("replayGain"),
            QStringLiteral("dsp.volume.tagSource"),
            QStringLiteral("dsp.volume.tagSourceDesc"),
            ParameterType::StringEnum,
            0.0, 2.0, 1.0, 0.0, 0.0,
            QString(),
            {QStringLiteral("auto"), QStringLiteral("file"), QStringLiteral("cue")}
        },

        // --- Mix Tab: Playback Progression ---
        {
            QStringLiteral("mix.autoAdvance"),
            QStringLiteral("mix"),
            QStringLiteral("progression"),
            QStringLiteral("dsp.mix.autoAdvance"),
            QStringLiteral("dsp.mix.autoAdvanceDesc"),
            ParameterType::Bool,
            0.0, 1.0, 1.0, 1.0, 1.0,
            QString(),
            {}
        },
        {
            QStringLiteral("mix.enabled"),
            QStringLiteral("mix"),
            QStringLiteral("progression"),
            QStringLiteral("dsp.mix.enableMixing"),
            QStringLiteral("dsp.mix.enableMixingDesc"),
            ParameterType::Bool,
            0.0, 1.0, 1.0, 0.0, 0.0,
            QString(),
            {}
        },

        // --- Mix Tab: Manual Transition ---
        {
            QStringLiteral("mix.manual.crossfade"),
            QStringLiteral("mix"),
            QStringLiteral("manual"),
            QStringLiteral("dsp.mix.manualCrossfade"),
            QStringLiteral("dsp.mix.manualCrossfadeDesc"),
            ParameterType::Bool,
            0.0, 1.0, 1.0, 0.0, 0.0,
            QString(),
            {}
        },
        {
            QStringLiteral("mix.manual.crossfadeMs"),
            QStringLiteral("mix"),
            QStringLiteral("manual"),
            QStringLiteral("dsp.mix.manualCrossfadeMs"),
            QStringLiteral("dsp.mix.manualCrossfadeMsDesc"),
            ParameterType::Int,
            0.0, 10000.0, 50.0, 0.0, 1000.0,
            QStringLiteral("ms"),
            {}
        },
        {
            QStringLiteral("mix.manual.fadeOut"),
            QStringLiteral("mix"),
            QStringLiteral("manual"),
            QStringLiteral("dsp.mix.manualFadeOut"),
            QStringLiteral("dsp.mix.manualFadeOutDesc"),
            ParameterType::Bool,
            0.0, 1.0, 1.0, 1.0, 1.0,
            QString(),
            {}
        },
        {
            QStringLiteral("mix.manual.fadeOutMs"),
            QStringLiteral("mix"),
            QStringLiteral("manual"),
            QStringLiteral("dsp.mix.manualFadeOutMs"),
            QStringLiteral("dsp.mix.manualFadeOutMsDesc"),
            ParameterType::Int,
            0.0, 10000.0, 50.0, 0.0, 500.0,
            QStringLiteral("ms"),
            {}
        },
        {
            QStringLiteral("mix.manual.fadeIn"),
            QStringLiteral("mix"),
            QStringLiteral("manual"),
            QStringLiteral("dsp.mix.manualFadeIn"),
            QStringLiteral("dsp.mix.manualFadeInDesc"),
            ParameterType::Bool,
            0.0, 1.0, 1.0, 1.0, 1.0,
            QString(),
            {}
        },
        {
            QStringLiteral("mix.manual.fadeInMs"),
            QStringLiteral("mix"),
            QStringLiteral("manual"),
            QStringLiteral("dsp.mix.manualFadeInMs"),
            QStringLiteral("dsp.mix.manualFadeInMsDesc"),
            ParameterType::Int,
            0.0, 10000.0, 50.0, 0.0, 500.0,
            QStringLiteral("ms"),
            {}
        },

        // --- Mix Tab: Automatic Transition ---
        {
            QStringLiteral("mix.automatic.mode"),
            QStringLiteral("mix"),
            QStringLiteral("automatic"),
            QStringLiteral("dsp.mix.automaticMode"),
            QStringLiteral("dsp.mix.automaticModeDesc"),
            ParameterType::StringEnum,
            0.0, 2.0, 1.0, 0.0, 0.0,
            QString(),
            {QStringLiteral("none"), QStringLiteral("pause"), QStringLiteral("crossfade")}
        },
        {
            QStringLiteral("mix.automatic.pauseMs"),
            QStringLiteral("mix"),
            QStringLiteral("automatic"),
            QStringLiteral("dsp.mix.automaticPauseMs"),
            QStringLiteral("dsp.mix.automaticPauseMsDesc"),
            ParameterType::Int,
            0.0, 10000.0, 50.0, 0.0, 1000.0,
            QStringLiteral("ms"),
            {}
        },
        {
            QStringLiteral("mix.automatic.crossfadeMs"),
            QStringLiteral("mix"),
            QStringLiteral("automatic"),
            QStringLiteral("dsp.mix.automaticCrossfadeMs"),
            QStringLiteral("dsp.mix.automaticCrossfadeMsDesc"),
            ParameterType::Int,
            0.0, 10000.0, 50.0, 0.0, 1000.0,
            QStringLiteral("ms"),
            {}
        },
        {
            QStringLiteral("mix.automatic.fadeOutMs"),
            QStringLiteral("mix"),
            QStringLiteral("automatic"),
            QStringLiteral("dsp.mix.automaticFadeOutMs"),
            QStringLiteral("dsp.mix.automaticFadeOutMsDesc"),
            ParameterType::Int,
            0.0, 10000.0, 50.0, 0.0, 1000.0,
            QStringLiteral("ms"),
            {}
        },
        {
            QStringLiteral("mix.automatic.fadeInMs"),
            QStringLiteral("mix"),
            QStringLiteral("automatic"),
            QStringLiteral("dsp.mix.automaticFadeInMs"),
            QStringLiteral("dsp.mix.automaticFadeInMsDesc"),
            ParameterType::Int,
            0.0, 10000.0, 50.0, 0.0, 1000.0,
            QStringLiteral("ms"),
            {}
        },

        // --- Silence Removal Tab ---
        {
            QStringLiteral("silenceRemoval.enabled"),
            QStringLiteral("silenceRemoval"),
            QStringLiteral("detection"),
            QStringLiteral("dsp.silenceRemoval.enable"),
            QStringLiteral("dsp.silenceRemoval.enableDesc"),
            ParameterType::Bool,
            0.0, 1.0, 1.0, 0.0, 0.0,
            QString(),
            {}
        },
        {
            QStringLiteral("silenceRemoval.minimumDurationMs"),
            QStringLiteral("silenceRemoval"),
            QStringLiteral("detection"),
            QStringLiteral("dsp.silenceRemoval.minimumDuration"),
            QStringLiteral("dsp.silenceRemoval.minimumDurationDesc"),
            ParameterType::Int,
            50.0, 5000.0, 50.0, 500.0, 500.0,
            QStringLiteral("ms"),
            {}
        },
        {
            QStringLiteral("silenceRemoval.thresholdDbfs"),
            QStringLiteral("silenceRemoval"),
            QStringLiteral("detection"),
            QStringLiteral("dsp.silenceRemoval.threshold"),
            QStringLiteral("dsp.silenceRemoval.thresholdDesc"),
            ParameterType::Double,
            -90.0, -20.0, 1.0, -60.0, -60.0,
            QStringLiteral("dBFS"),
            {}
        },
        {
            QStringLiteral("silenceRemoval.trimEdges"),
            QStringLiteral("silenceRemoval"),
            QStringLiteral("detection"),
            QStringLiteral("dsp.silenceRemoval.trimEdges"),
            QStringLiteral("dsp.silenceRemoval.trimEdgesDesc"),
            ParameterType::Bool,
            0.0, 1.0, 1.0, 1.0, 1.0,
            QString(),
            {}
        }
    };

    return definitions;
}

const ParameterDefinition *findParameterDefinition(const QString &id)
{
    const auto &defs = allParameterDefinitions();
    QString target = id;
    if (target.startsWith(QStringLiteral("dsp."))) {
        target = target.mid(4);
    }
    for (const auto &def : defs) {
        if (def.id == id || def.id == target || def.nameKey == id) {
            return &def;
        }
        if (def.id == target + QStringLiteral("Mix") || target == def.id + QStringLiteral("Mix")) {
            return &def;
        }
    }
    return nullptr;
}

std::vector<ParameterDefinition> parametersForTab(const QString &tabId)
{
    std::vector<ParameterDefinition> result;
    const auto &defs = allParameterDefinitions();
    for (const auto &def : defs) {
        if (def.tabId == tabId) {
            result.push_back(def);
        }
    }
    return result;
}

std::vector<ParameterDefinition> parametersForGroup(const QString &groupId)
{
    std::vector<ParameterDefinition> result;
    const auto &defs = allParameterDefinitions();
    for (const auto &def : defs) {
        if (def.groupId == groupId) {
            result.push_back(def);
        }
    }
    return result;
}

double sanitizeDouble(const ParameterDefinition &def, double value)
{
    if (std::isnan(value) || std::isinf(value)) {
        return def.defaultValue;
    }
    double clamped = std::clamp(value, def.minValue, def.maxValue);
    if (def.step > 0.0) {
        clamped = std::round((clamped - def.minValue) / def.step) * def.step + def.minValue;
        clamped = std::clamp(clamped, def.minValue, def.maxValue);
    }
    return clamped;
}

int sanitizeInt(const ParameterDefinition &def, int value)
{
    const int minVal = static_cast<int>(def.minValue);
    const int maxVal = static_cast<int>(def.maxValue);
    int clamped = std::clamp(value, minVal, maxVal);
    const int stepVal = static_cast<int>(def.step);
    if (stepVal > 0) {
        clamped = ((clamped - minVal + stepVal / 2) / stepVal) * stepVal + minVal;
        clamped = std::clamp(clamped, minVal, maxVal);
    }
    return clamped;
}

QString sanitizeEnum(const ParameterDefinition &def, const QString &value)
{
    const QString trimmed = value.trimmed().toLower();
    for (const QString &validOption : def.enumValues) {
        if (validOption.toLower() == trimmed) {
            return validOption;
        }
    }
    return def.enumValues.isEmpty() ? QString() : def.enumValues.first();
}

QVariant sanitizeValue(const ParameterDefinition &def, const QVariant &value)
{
    switch (def.type) {
    case ParameterType::Double:
        return sanitizeDouble(def, value.toDouble());
    case ParameterType::Int:
        return sanitizeInt(def, value.toInt());
    case ParameterType::Bool:
        return value.toBool();
    case ParameterType::StringEnum:
        return sanitizeEnum(def, value.toString());
    }
    return value;
}

} // namespace WaveFlux::Dsp
