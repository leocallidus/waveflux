#include "DspSettingsManager.h"
#include <cmath>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>

namespace {
constexpr const char *kSettingsGroup = "dsp_settings_v1";
constexpr const char *kSchemaVersionKey = "schemaVersion";
constexpr const char *kCurrentSchemaVersion = "waveflux.dsp.settings.v1";
}

DspSettingsManager::DspSettingsManager(QObject *parent)
    : QObject(parent)
{
    s_instance = this;
    setDefaults();
    loadSettings();
}

DspSettingsManager::~DspSettingsManager()
{
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

DspSettingsManager *DspSettingsManager::instance()
{
    return s_instance;
}

void DspSettingsManager::setDefaults()
{
    m_echoMix = 0.0;
    m_chorusMix = 0.0;
    m_speed = 1.00;
    m_reverbMix = 0.0;
    m_bass = 1.00;
    m_tempo = 1.00;
    m_flangerMix = 0.0;
    m_stereoWidth = 1.00;
    m_tonalitySemitones = 0.00;

    m_voiceSuppression = false;
    m_fadePauseResume = false;
    m_fadeTrackNavigation = false;

    m_smoothChanges = false;
    m_logarithmicControl = false;
    m_loudnessCompensation = false;
    m_balance = 0.00;

    m_amplitudeNormalizationEnabled = false;
    m_amplitudeTargetPeakDbfs = -1.0;
    m_amplitudePreampDb = 0.0;
    m_amplitudeUseTagValues = true;

    m_replayGainEnabled = false;
    m_replayGainMode = QStringLiteral("auto");
    m_replayGainFallbackDb = 0.0;
    m_replayGainAnalyzeOnTheFly = false;
    m_replayGainPreampDb = 0.0;
    m_replayGainUseTags = true;
    m_replayGainTagSource = QStringLiteral("auto");

    m_mixAutoAdvance = true;
    m_mixEnabled = false;
    m_mixManualCrossfade = false;
    m_mixManualCrossfadeMs = 1000;
    m_mixManualFadeOut = true;
    m_mixManualFadeOutMs = 500;
    m_mixManualFadeIn = true;
    m_mixManualFadeInMs = 500;
    m_mixAutomaticMode = QStringLiteral("none");
    m_mixAutomaticPauseMs = 1000;
    m_mixAutomaticCrossfadeMs = 1000;
    m_mixAutomaticFadeOutMs = 1000;
    m_mixAutomaticFadeInMs = 1000;

    m_silenceRemovalEnabled = false;
    m_silenceRemovalMinimumDurationMs = 500;
    m_silenceRemovalThresholdDbfs = -60.0;
    m_silenceRemovalTrimEdges = true;

    m_lastSelectedTab = QStringLiteral("general");
}

void DspSettingsManager::loadSettings()
{
    QSettings settings(QStringLiteral("WaveFlux"), QStringLiteral("WaveFlux"));
    settings.beginGroup(QString::fromLatin1(kSettingsGroup));

    const QString version = settings.value(QString::fromLatin1(kSchemaVersionKey)).toString();
    if (version != QString::fromLatin1(kCurrentSchemaVersion)) {
        settings.endGroup();
        checkAndPerformMigration();
        return;
    }

    const auto &defs = WaveFlux::Dsp::allParameterDefinitions();
    for (const auto &def : defs) {
        if (!settings.contains(def.id)) {
            continue;
        }
        const QVariant rawVal = settings.value(def.id);
        const QVariant val = WaveFlux::Dsp::sanitizeValue(def, rawVal);
        setParameterValue(def.id, val);
    }

    m_lastSelectedTab = settings.value(QStringLiteral("ui.lastSelectedTab"), QStringLiteral("general")).toString();
    settings.endGroup();
}

void DspSettingsManager::saveSettings()
{
    QSettings settings(QStringLiteral("WaveFlux"), QStringLiteral("WaveFlux"));
    settings.beginGroup(QString::fromLatin1(kSettingsGroup));

    settings.setValue(QString::fromLatin1(kSchemaVersionKey), QString::fromLatin1(kCurrentSchemaVersion));

    const auto &defs = WaveFlux::Dsp::allParameterDefinitions();
    for (const auto &def : defs) {
        settings.setValue(def.id, getParameterValue(def.id));
    }

    settings.setValue(QStringLiteral("ui.lastSelectedTab"), m_lastSelectedTab);
    settings.endGroup();
    settings.sync();
}

void DspSettingsManager::checkAndPerformMigration()
{
    // On upgrade / first run of DSP manager:
    // Read any legacy pitch / rate settings and initialize neutral defaults
    setDefaults();
    saveSettings();
}

void DspSettingsManager::setEchoMix(double value)
{
    const auto *def = WaveFlux::Dsp::findParameterDefinition(QStringLiteral("general.echoMix"));
    const double sanitized = def ? WaveFlux::Dsp::sanitizeDouble(*def, value) : std::clamp(value, 0.0, 100.0);
    if (std::abs(m_echoMix - sanitized) > 1e-4) {
        m_echoMix = sanitized;
        emit echoMixChanged();
        emit dspSettingsChanged();
        saveSettings();
    }
}

void DspSettingsManager::setChorusMix(double value)
{
    const auto *def = WaveFlux::Dsp::findParameterDefinition(QStringLiteral("general.chorusMix"));
    const double sanitized = def ? WaveFlux::Dsp::sanitizeDouble(*def, value) : std::clamp(value, 0.0, 100.0);
    if (std::abs(m_chorusMix - sanitized) > 1e-4) {
        m_chorusMix = sanitized;
        emit chorusMixChanged();
        emit dspSettingsChanged();
        saveSettings();
    }
}

void DspSettingsManager::setSpeed(double value)
{
    const auto *def = WaveFlux::Dsp::findParameterDefinition(QStringLiteral("general.speed"));
    const double sanitized = def ? WaveFlux::Dsp::sanitizeDouble(*def, value) : std::clamp(value, 0.25, 3.00);
    if (std::abs(m_speed - sanitized) > 1e-4) {
        m_speed = sanitized;
        emit speedChanged();
        emit dspSettingsChanged();
        saveSettings();
    }
}

void DspSettingsManager::setReverbMix(double value)
{
    const auto *def = WaveFlux::Dsp::findParameterDefinition(QStringLiteral("general.reverbMix"));
    const double sanitized = def ? WaveFlux::Dsp::sanitizeDouble(*def, value) : std::clamp(value, 0.0, 100.0);
    if (std::abs(m_reverbMix - sanitized) > 1e-4) {
        m_reverbMix = sanitized;
        emit reverbMixChanged();
        emit dspSettingsChanged();
        saveSettings();
    }
}

void DspSettingsManager::setBass(double value)
{
    const auto *def = WaveFlux::Dsp::findParameterDefinition(QStringLiteral("general.bass"));
    const double sanitized = def ? WaveFlux::Dsp::sanitizeDouble(*def, value) : std::clamp(value, 0.0, 2.0);
    if (std::abs(m_bass - sanitized) > 1e-4) {
        m_bass = sanitized;
        emit bassChanged();
        emit dspSettingsChanged();
        saveSettings();
    }
}

void DspSettingsManager::setTempo(double value)
{
    const auto *def = WaveFlux::Dsp::findParameterDefinition(QStringLiteral("general.tempo"));
    const double sanitized = def ? WaveFlux::Dsp::sanitizeDouble(*def, value) : std::clamp(value, 0.50, 3.00);
    if (std::abs(m_tempo - sanitized) > 1e-4) {
        m_tempo = sanitized;
        emit tempoChanged();
        emit dspSettingsChanged();
        saveSettings();
    }
}

void DspSettingsManager::setFlangerMix(double value)
{
    const auto *def = WaveFlux::Dsp::findParameterDefinition(QStringLiteral("general.flangerMix"));
    const double sanitized = def ? WaveFlux::Dsp::sanitizeDouble(*def, value) : std::clamp(value, 0.0, 100.0);
    if (std::abs(m_flangerMix - sanitized) > 1e-4) {
        m_flangerMix = sanitized;
        emit flangerMixChanged();
        emit dspSettingsChanged();
        saveSettings();
    }
}

void DspSettingsManager::setStereoWidth(double value)
{
    const auto *def = WaveFlux::Dsp::findParameterDefinition(QStringLiteral("general.stereoWidth"));
    const double sanitized = def ? WaveFlux::Dsp::sanitizeDouble(*def, value) : std::clamp(value, 1.00, 5.00);
    if (std::abs(m_stereoWidth - sanitized) > 1e-4) {
        m_stereoWidth = sanitized;
        emit stereoWidthChanged();
        emit dspSettingsChanged();
        saveSettings();
    }
}

void DspSettingsManager::setTonalitySemitones(double value)
{
    const auto *def = WaveFlux::Dsp::findParameterDefinition(QStringLiteral("general.tonalitySemitones"));
    const double sanitized = def ? WaveFlux::Dsp::sanitizeDouble(*def, value) : std::clamp(value, -10.00, 10.00);
    if (std::abs(m_tonalitySemitones - sanitized) > 1e-4) {
        m_tonalitySemitones = sanitized;
        emit tonalitySemitonesChanged();
        emit dspSettingsChanged();
        saveSettings();
    }
}

void DspSettingsManager::setVoiceSuppression(bool value)
{
    if (m_voiceSuppression != value) {
        m_voiceSuppression = value;
        emit voiceSuppressionChanged();
        emit dspSettingsChanged();
        saveSettings();
    }
}

void DspSettingsManager::setFadePauseResume(bool value)
{
    if (m_fadePauseResume != value) {
        m_fadePauseResume = value;
        emit fadePauseResumeChanged();
        emit dspSettingsChanged();
        saveSettings();
    }
}

void DspSettingsManager::setFadeTrackNavigation(bool value)
{
    if (m_fadeTrackNavigation != value) {
        m_fadeTrackNavigation = value;
        emit fadeTrackNavigationChanged();
        emit dspSettingsChanged();
        saveSettings();
    }
}

void DspSettingsManager::setSmoothChanges(bool value)
{
    if (m_smoothChanges != value) {
        m_smoothChanges = value;
        emit smoothChangesChanged();
        emit dspSettingsChanged();
        saveSettings();
    }
}

void DspSettingsManager::setLogarithmicControl(bool value)
{
    if (m_logarithmicControl != value) {
        m_logarithmicControl = value;
        emit logarithmicControlChanged();
        emit dspSettingsChanged();
        saveSettings();
    }
}

void DspSettingsManager::setLoudnessCompensation(bool value)
{
    if (m_loudnessCompensation != value) {
        m_loudnessCompensation = value;
        emit loudnessCompensationChanged();
        emit dspSettingsChanged();
        saveSettings();
    }
}

void DspSettingsManager::setBalance(double value)
{
    const auto *def = WaveFlux::Dsp::findParameterDefinition(QStringLiteral("volume.balance"));
    const double sanitized = def ? WaveFlux::Dsp::sanitizeDouble(*def, value) : std::clamp(value, -1.0, 1.0);
    if (std::abs(m_balance - sanitized) > 1e-4) {
        m_balance = sanitized;
        emit balanceChanged();
        emit dspSettingsChanged();
        saveSettings();
    }
}

void DspSettingsManager::setAmplitudeNormalizationEnabled(bool value)
{
    if (m_amplitudeNormalizationEnabled != value) {
        m_amplitudeNormalizationEnabled = value;
        if (value && m_replayGainEnabled) {
            // Mutually exclusive in v1
            m_replayGainEnabled = false;
            emit replayGainEnabledChanged();
        }
        emit amplitudeNormalizationEnabledChanged();
        emit dspSettingsChanged();
        saveSettings();
    }
}

void DspSettingsManager::setAmplitudeTargetPeakDbfs(double value)
{
    const auto *def = WaveFlux::Dsp::findParameterDefinition(QStringLiteral("volume.amplitudeNormalization.targetPeakDbfs"));
    const double sanitized = def ? WaveFlux::Dsp::sanitizeDouble(*def, value) : std::clamp(value, -20.0, 0.0);
    if (std::abs(m_amplitudeTargetPeakDbfs - sanitized) > 1e-4) {
        m_amplitudeTargetPeakDbfs = sanitized;
        emit amplitudeTargetPeakDbfsChanged();
        emit dspSettingsChanged();
        saveSettings();
    }
}

void DspSettingsManager::setAmplitudePreampDb(double value)
{
    const auto *def = WaveFlux::Dsp::findParameterDefinition(QStringLiteral("volume.amplitudeNormalization.preampDb"));
    const double sanitized = def ? WaveFlux::Dsp::sanitizeDouble(*def, value) : std::clamp(value, -20.0, 20.0);
    if (std::abs(m_amplitudePreampDb - sanitized) > 1e-4) {
        m_amplitudePreampDb = sanitized;
        emit amplitudePreampDbChanged();
        emit dspSettingsChanged();
        saveSettings();
    }
}

void DspSettingsManager::setAmplitudeUseTagValues(bool value)
{
    if (m_amplitudeUseTagValues != value) {
        m_amplitudeUseTagValues = value;
        emit amplitudeUseTagValuesChanged();
        emit dspSettingsChanged();
        saveSettings();
    }
}

void DspSettingsManager::setReplayGainEnabled(bool value)
{
    if (m_replayGainEnabled != value) {
        m_replayGainEnabled = value;
        if (value && m_amplitudeNormalizationEnabled) {
            // Mutually exclusive in v1
            m_amplitudeNormalizationEnabled = false;
            emit amplitudeNormalizationEnabledChanged();
        }
        emit replayGainEnabledChanged();
        emit dspSettingsChanged();
        saveSettings();
    }
}

void DspSettingsManager::setReplayGainMode(const QString &value)
{
    const auto *def = WaveFlux::Dsp::findParameterDefinition(QStringLiteral("volume.replayGain.mode"));
    const QString sanitized = def ? WaveFlux::Dsp::sanitizeEnum(*def, value) : value;
    if (m_replayGainMode != sanitized) {
        m_replayGainMode = sanitized;
        emit replayGainModeChanged();
        emit dspSettingsChanged();
        saveSettings();
    }
}

void DspSettingsManager::setReplayGainFallbackDb(double value)
{
    const auto *def = WaveFlux::Dsp::findParameterDefinition(QStringLiteral("volume.replayGain.fallbackDb"));
    const double sanitized = def ? WaveFlux::Dsp::sanitizeDouble(*def, value) : std::clamp(value, -20.0, 20.0);
    if (std::abs(m_replayGainFallbackDb - sanitized) > 1e-4) {
        m_replayGainFallbackDb = sanitized;
        emit replayGainFallbackDbChanged();
        emit dspSettingsChanged();
        saveSettings();
    }
}

void DspSettingsManager::setReplayGainAnalyzeOnTheFly(bool value)
{
    if (m_replayGainAnalyzeOnTheFly != value) {
        m_replayGainAnalyzeOnTheFly = value;
        emit replayGainAnalyzeOnTheFlyChanged();
        emit dspSettingsChanged();
        saveSettings();
    }
}

void DspSettingsManager::setReplayGainPreampDb(double value)
{
    const auto *def = WaveFlux::Dsp::findParameterDefinition(QStringLiteral("volume.replayGain.preampDb"));
    const double sanitized = def ? WaveFlux::Dsp::sanitizeDouble(*def, value) : std::clamp(value, -20.0, 20.0);
    if (std::abs(m_replayGainPreampDb - sanitized) > 1e-4) {
        m_replayGainPreampDb = sanitized;
        emit replayGainPreampDbChanged();
        emit dspSettingsChanged();
        saveSettings();
    }
}

void DspSettingsManager::setReplayGainUseTags(bool value)
{
    if (m_replayGainUseTags != value) {
        m_replayGainUseTags = value;
        emit replayGainUseTagsChanged();
        emit dspSettingsChanged();
        saveSettings();
    }
}

void DspSettingsManager::setReplayGainTagSource(const QString &value)
{
    const auto *def = WaveFlux::Dsp::findParameterDefinition(QStringLiteral("volume.replayGain.tagSource"));
    const QString sanitized = def ? WaveFlux::Dsp::sanitizeEnum(*def, value) : value;
    if (m_replayGainTagSource != sanitized) {
        m_replayGainTagSource = sanitized;
        emit replayGainTagSourceChanged();
        emit dspSettingsChanged();
        saveSettings();
    }
}

void DspSettingsManager::setEffectiveReplayGainDiagnostic(const QString &diag)
{
    if (m_effectiveReplayGainDiagnostic != diag) {
        m_effectiveReplayGainDiagnostic = diag;
        emit effectiveReplayGainDiagnosticChanged();
    }
}

void DspSettingsManager::setMixAutoAdvance(bool value)
{
    if (m_mixAutoAdvance != value) {
        m_mixAutoAdvance = value;
        emit mixAutoAdvanceChanged();
        emit dspSettingsChanged();
        saveSettings();
    }
}

void DspSettingsManager::setMixEnabled(bool value)
{
    if (m_mixEnabled != value) {
        m_mixEnabled = value;
        emit mixEnabledChanged();
        emit dspSettingsChanged();
        saveSettings();
    }
}

void DspSettingsManager::setMixManualCrossfade(bool value)
{
    if (m_mixManualCrossfade != value) {
        m_mixManualCrossfade = value;
        emit mixManualCrossfadeChanged();
        emit dspSettingsChanged();
        saveSettings();
    }
}

void DspSettingsManager::setMixManualCrossfadeMs(int value)
{
    const auto *def = WaveFlux::Dsp::findParameterDefinition(QStringLiteral("mix.manual.crossfadeMs"));
    const int sanitized = def ? WaveFlux::Dsp::sanitizeInt(*def, value) : std::clamp(value, 0, 10000);
    if (m_mixManualCrossfadeMs != sanitized) {
        m_mixManualCrossfadeMs = sanitized;
        emit mixManualCrossfadeMsChanged();
        emit dspSettingsChanged();
        saveSettings();
    }
}

void DspSettingsManager::setMixManualFadeOut(bool value)
{
    if (m_mixManualFadeOut != value) {
        m_mixManualFadeOut = value;
        emit mixManualFadeOutChanged();
        emit dspSettingsChanged();
        saveSettings();
    }
}

void DspSettingsManager::setMixManualFadeOutMs(int value)
{
    const auto *def = WaveFlux::Dsp::findParameterDefinition(QStringLiteral("mix.manual.fadeOutMs"));
    const int sanitized = def ? WaveFlux::Dsp::sanitizeInt(*def, value) : std::clamp(value, 0, 10000);
    if (m_mixManualFadeOutMs != sanitized) {
        m_mixManualFadeOutMs = sanitized;
        emit mixManualFadeOutMsChanged();
        emit dspSettingsChanged();
        saveSettings();
    }
}

void DspSettingsManager::setMixManualFadeIn(bool value)
{
    if (m_mixManualFadeIn != value) {
        m_mixManualFadeIn = value;
        emit mixManualFadeInChanged();
        emit dspSettingsChanged();
        saveSettings();
    }
}

void DspSettingsManager::setMixManualFadeInMs(int value)
{
    const auto *def = WaveFlux::Dsp::findParameterDefinition(QStringLiteral("mix.manual.fadeInMs"));
    const int sanitized = def ? WaveFlux::Dsp::sanitizeInt(*def, value) : std::clamp(value, 0, 10000);
    if (m_mixManualFadeInMs != sanitized) {
        m_mixManualFadeInMs = sanitized;
        emit mixManualFadeInMsChanged();
        emit dspSettingsChanged();
        saveSettings();
    }
}

void DspSettingsManager::setMixAutomaticMode(const QString &value)
{
    const auto *def = WaveFlux::Dsp::findParameterDefinition(QStringLiteral("mix.automatic.mode"));
    const QString sanitized = def ? WaveFlux::Dsp::sanitizeEnum(*def, value) : value;
    if (m_mixAutomaticMode != sanitized) {
        m_mixAutomaticMode = sanitized;
        emit mixAutomaticModeChanged();
        emit dspSettingsChanged();
        saveSettings();
    }
}

void DspSettingsManager::setMixAutomaticPauseMs(int value)
{
    const auto *def = WaveFlux::Dsp::findParameterDefinition(QStringLiteral("mix.automatic.pauseMs"));
    const int sanitized = def ? WaveFlux::Dsp::sanitizeInt(*def, value) : std::clamp(value, 0, 10000);
    if (m_mixAutomaticPauseMs != sanitized) {
        m_mixAutomaticPauseMs = sanitized;
        emit mixAutomaticPauseMsChanged();
        emit dspSettingsChanged();
        saveSettings();
    }
}

void DspSettingsManager::setMixAutomaticCrossfadeMs(int value)
{
    const auto *def = WaveFlux::Dsp::findParameterDefinition(QStringLiteral("mix.automatic.crossfadeMs"));
    const int sanitized = def ? WaveFlux::Dsp::sanitizeInt(*def, value) : std::clamp(value, 0, 10000);
    if (m_mixAutomaticCrossfadeMs != sanitized) {
        m_mixAutomaticCrossfadeMs = sanitized;
        emit mixAutomaticCrossfadeMsChanged();
        emit dspSettingsChanged();
        saveSettings();
    }
}

void DspSettingsManager::setMixAutomaticFadeOutMs(int value)
{
    const auto *def = WaveFlux::Dsp::findParameterDefinition(QStringLiteral("mix.automatic.fadeOutMs"));
    const int sanitized = def ? WaveFlux::Dsp::sanitizeInt(*def, value) : std::clamp(value, 0, 10000);
    if (m_mixAutomaticFadeOutMs != sanitized) {
        m_mixAutomaticFadeOutMs = sanitized;
        emit mixAutomaticFadeOutMsChanged();
        emit dspSettingsChanged();
        saveSettings();
    }
}

void DspSettingsManager::setMixAutomaticFadeInMs(int value)
{
    const auto *def = WaveFlux::Dsp::findParameterDefinition(QStringLiteral("mix.automatic.fadeInMs"));
    const int sanitized = def ? WaveFlux::Dsp::sanitizeInt(*def, value) : std::clamp(value, 0, 10000);
    if (m_mixAutomaticFadeInMs != sanitized) {
        m_mixAutomaticFadeInMs = sanitized;
        emit mixAutomaticFadeInMsChanged();
        emit dspSettingsChanged();
        saveSettings();
    }
}

void DspSettingsManager::setSilenceRemovalEnabled(bool value)
{
    if (m_silenceRemovalEnabled != value) {
        m_silenceRemovalEnabled = value;
        emit silenceRemovalEnabledChanged();
        emit dspSettingsChanged();
        saveSettings();
    }
}

void DspSettingsManager::setSilenceRemovalMinimumDurationMs(int value)
{
    const auto *def = WaveFlux::Dsp::findParameterDefinition(QStringLiteral("silenceRemoval.minimumDurationMs"));
    const int sanitized = def ? WaveFlux::Dsp::sanitizeInt(*def, value) : std::clamp(value, 50, 5000);
    if (m_silenceRemovalMinimumDurationMs != sanitized) {
        m_silenceRemovalMinimumDurationMs = sanitized;
        emit silenceRemovalMinimumDurationMsChanged();
        emit dspSettingsChanged();
        saveSettings();
    }
}

void DspSettingsManager::setSilenceRemovalThresholdDbfs(double value)
{
    const auto *def = WaveFlux::Dsp::findParameterDefinition(QStringLiteral("silenceRemoval.thresholdDbfs"));
    const double sanitized = def ? WaveFlux::Dsp::sanitizeDouble(*def, value) : std::clamp(value, -90.0, -20.0);
    if (std::abs(m_silenceRemovalThresholdDbfs - sanitized) > 1e-4) {
        m_silenceRemovalThresholdDbfs = sanitized;
        emit silenceRemovalThresholdDbfsChanged();
        emit dspSettingsChanged();
        saveSettings();
    }
}

void DspSettingsManager::setSilenceRemovalTrimEdges(bool value)
{
    if (m_silenceRemovalTrimEdges != value) {
        m_silenceRemovalTrimEdges = value;
        emit silenceRemovalTrimEdgesChanged();
        emit dspSettingsChanged();
        saveSettings();
    }
}

void DspSettingsManager::setLastSelectedTab(const QString &tabId)
{
    if (m_lastSelectedTab != tabId && !tabId.isEmpty()) {
        m_lastSelectedTab = tabId;
        emit lastSelectedTabChanged();
        saveSettings();
    }
}

QVariant DspSettingsManager::getParameterValue(const QString &paramId) const
{
    const auto *def = WaveFlux::Dsp::findParameterDefinition(paramId);
    const QString id = def ? def->id : paramId;

    if (id == QStringLiteral("general.echoMix")) return m_echoMix;
    if (id == QStringLiteral("general.chorusMix")) return m_chorusMix;
    if (id == QStringLiteral("general.speed")) return m_speed;
    if (id == QStringLiteral("general.reverbMix")) return m_reverbMix;
    if (id == QStringLiteral("general.bass")) return m_bass;
    if (id == QStringLiteral("general.tempo")) return m_tempo;
    if (id == QStringLiteral("general.flangerMix")) return m_flangerMix;
    if (id == QStringLiteral("general.stereoWidth")) return m_stereoWidth;
    if (id == QStringLiteral("general.tonalitySemitones")) return m_tonalitySemitones;
    if (id == QStringLiteral("general.voiceSuppression")) return m_voiceSuppression;
    if (id == QStringLiteral("general.fadePauseResume")) return m_fadePauseResume;
    if (id == QStringLiteral("general.fadeTrackNavigation")) return m_fadeTrackNavigation;

    if (id == QStringLiteral("volume.smoothChanges")) return m_smoothChanges;
    if (id == QStringLiteral("volume.logarithmicControl")) return m_logarithmicControl;
    if (id == QStringLiteral("volume.loudnessCompensation")) return m_loudnessCompensation;
    if (id == QStringLiteral("volume.balance")) return m_balance;
    if (id == QStringLiteral("volume.amplitudeNormalization.enabled")) return m_amplitudeNormalizationEnabled;
    if (id == QStringLiteral("volume.amplitudeNormalization.targetPeakDbfs")) return m_amplitudeTargetPeakDbfs;
    if (id == QStringLiteral("volume.amplitudeNormalization.preampDb")) return m_amplitudePreampDb;
    if (id == QStringLiteral("volume.amplitudeNormalization.useTagValues")) return m_amplitudeUseTagValues;
    if (id == QStringLiteral("volume.replayGain.enabled")) return m_replayGainEnabled;
    if (id == QStringLiteral("volume.replayGain.mode")) return m_replayGainMode;
    if (id == QStringLiteral("volume.replayGain.fallbackDb")) return m_replayGainFallbackDb;
    if (id == QStringLiteral("volume.replayGain.analyzeOnTheFly")) return m_replayGainAnalyzeOnTheFly;
    if (id == QStringLiteral("volume.replayGain.preampDb")) return m_replayGainPreampDb;
    if (id == QStringLiteral("volume.replayGain.useTags")) return m_replayGainUseTags;
    if (id == QStringLiteral("volume.replayGain.tagSource")) return m_replayGainTagSource;

    if (id == QStringLiteral("mix.autoAdvance")) return m_mixAutoAdvance;
    if (id == QStringLiteral("mix.enabled")) return m_mixEnabled;
    if (id == QStringLiteral("mix.manual.crossfade")) return m_mixManualCrossfade;
    if (id == QStringLiteral("mix.manual.crossfadeMs")) return m_mixManualCrossfadeMs;
    if (id == QStringLiteral("mix.manual.fadeOut")) return m_mixManualFadeOut;
    if (id == QStringLiteral("mix.manual.fadeOutMs")) return m_mixManualFadeOutMs;
    if (id == QStringLiteral("mix.manual.fadeIn")) return m_mixManualFadeIn;
    if (id == QStringLiteral("mix.manual.fadeInMs")) return m_mixManualFadeInMs;
    if (id == QStringLiteral("mix.automatic.mode")) return m_mixAutomaticMode;
    if (id == QStringLiteral("mix.automatic.pauseMs")) return m_mixAutomaticPauseMs;
    if (id == QStringLiteral("mix.automatic.crossfadeMs")) return m_mixAutomaticCrossfadeMs;
    if (id == QStringLiteral("mix.automatic.fadeOutMs")) return m_mixAutomaticFadeOutMs;
    if (id == QStringLiteral("mix.automatic.fadeInMs")) return m_mixAutomaticFadeInMs;

    if (id == QStringLiteral("silenceRemoval.enabled")) return m_silenceRemovalEnabled;
    if (id == QStringLiteral("silenceRemoval.minimumDurationMs")) return m_silenceRemovalMinimumDurationMs;
    if (id == QStringLiteral("silenceRemoval.thresholdDbfs")) return m_silenceRemovalThresholdDbfs;
    if (id == QStringLiteral("silenceRemoval.trimEdges")) return m_silenceRemovalTrimEdges;

    return QVariant();
}

void DspSettingsManager::setParameterValue(const QString &paramId, const QVariant &value)
{
    const auto *def = WaveFlux::Dsp::findParameterDefinition(paramId);
    const QString id = def ? def->id : paramId;

    if (id == QStringLiteral("general.echoMix")) setEchoMix(value.toDouble());
    else if (id == QStringLiteral("general.chorusMix")) setChorusMix(value.toDouble());
    else if (id == QStringLiteral("general.speed")) setSpeed(value.toDouble());
    else if (id == QStringLiteral("general.reverbMix")) setReverbMix(value.toDouble());
    else if (id == QStringLiteral("general.bass")) setBass(value.toDouble());
    else if (id == QStringLiteral("general.tempo")) setTempo(value.toDouble());
    else if (id == QStringLiteral("general.flangerMix")) setFlangerMix(value.toDouble());
    else if (id == QStringLiteral("general.stereoWidth")) setStereoWidth(value.toDouble());
    else if (id == QStringLiteral("general.tonalitySemitones")) setTonalitySemitones(value.toDouble());
    else if (id == QStringLiteral("general.voiceSuppression")) setVoiceSuppression(value.toBool());
    else if (id == QStringLiteral("general.fadePauseResume")) setFadePauseResume(value.toBool());
    else if (id == QStringLiteral("general.fadeTrackNavigation")) setFadeTrackNavigation(value.toBool());

    else if (id == QStringLiteral("volume.smoothChanges")) setSmoothChanges(value.toBool());
    else if (id == QStringLiteral("volume.logarithmicControl")) setLogarithmicControl(value.toBool());
    else if (id == QStringLiteral("volume.loudnessCompensation")) setLoudnessCompensation(value.toBool());
    else if (id == QStringLiteral("volume.balance")) setBalance(value.toDouble());
    else if (id == QStringLiteral("volume.amplitudeNormalization.enabled")) setAmplitudeNormalizationEnabled(value.toBool());
    else if (id == QStringLiteral("volume.amplitudeNormalization.targetPeakDbfs")) setAmplitudeTargetPeakDbfs(value.toDouble());
    else if (id == QStringLiteral("volume.amplitudeNormalization.preampDb")) setAmplitudePreampDb(value.toDouble());
    else if (id == QStringLiteral("volume.amplitudeNormalization.useTagValues")) setAmplitudeUseTagValues(value.toBool());
    else if (id == QStringLiteral("volume.replayGain.enabled")) setReplayGainEnabled(value.toBool());
    else if (id == QStringLiteral("volume.replayGain.mode")) setReplayGainMode(value.toString());
    else if (id == QStringLiteral("volume.replayGain.fallbackDb")) setReplayGainFallbackDb(value.toDouble());
    else if (id == QStringLiteral("volume.replayGain.analyzeOnTheFly")) setReplayGainAnalyzeOnTheFly(value.toBool());
    else if (id == QStringLiteral("volume.replayGain.preampDb")) setReplayGainPreampDb(value.toDouble());
    else if (id == QStringLiteral("volume.replayGain.useTags")) setReplayGainUseTags(value.toBool());
    else if (id == QStringLiteral("volume.replayGain.tagSource")) setReplayGainTagSource(value.toString());

    else if (id == QStringLiteral("mix.autoAdvance")) setMixAutoAdvance(value.toBool());
    else if (id == QStringLiteral("mix.enabled")) setMixEnabled(value.toBool());
    else if (id == QStringLiteral("mix.manual.crossfade")) setMixManualCrossfade(value.toBool());
    else if (id == QStringLiteral("mix.manual.crossfadeMs")) setMixManualCrossfadeMs(value.toInt());
    else if (id == QStringLiteral("mix.manual.fadeOut")) setMixManualFadeOut(value.toBool());
    else if (id == QStringLiteral("mix.manual.fadeOutMs")) setMixManualFadeOutMs(value.toInt());
    else if (id == QStringLiteral("mix.manual.fadeIn")) setMixManualFadeIn(value.toBool());
    else if (id == QStringLiteral("mix.manual.fadeInMs")) setMixManualFadeInMs(value.toInt());
    else if (id == QStringLiteral("mix.automatic.mode")) setMixAutomaticMode(value.toString());
    else if (id == QStringLiteral("mix.automatic.pauseMs")) setMixAutomaticPauseMs(value.toInt());
    else if (id == QStringLiteral("mix.automatic.crossfadeMs")) setMixAutomaticCrossfadeMs(value.toInt());
    else if (id == QStringLiteral("mix.automatic.fadeOutMs")) setMixAutomaticFadeOutMs(value.toInt());
    else if (id == QStringLiteral("mix.automatic.fadeInMs")) setMixAutomaticFadeInMs(value.toInt());

    else if (id == QStringLiteral("silenceRemoval.enabled")) setSilenceRemovalEnabled(value.toBool());
    else if (id == QStringLiteral("silenceRemoval.minimumDurationMs")) setSilenceRemovalMinimumDurationMs(value.toInt());
    else if (id == QStringLiteral("silenceRemoval.thresholdDbfs")) setSilenceRemovalThresholdDbfs(value.toDouble());
    else if (id == QStringLiteral("silenceRemoval.trimEdges")) setSilenceRemovalTrimEdges(value.toBool());
}

QVariantMap DspSettingsManager::getParameterDefinition(const QString &paramId) const
{
    const auto *def = WaveFlux::Dsp::findParameterDefinition(paramId);
    return def ? def->toVariantMap() : QVariantMap();
}

QVariantList DspSettingsManager::listParametersForTab(const QString &tabId) const
{
    QVariantList list;
    const auto defs = WaveFlux::Dsp::parametersForTab(tabId);
    for (const auto &def : defs) {
        list.push_back(def.toVariantMap());
    }
    return list;
}

void DspSettingsManager::resetParameter(const QString &paramId)
{
    const auto *def = WaveFlux::Dsp::findParameterDefinition(paramId);
    if (!def) {
        return;
    }
    switch (def->type) {
    case WaveFlux::Dsp::ParameterType::Double:
        setParameterValue(def->id, def->defaultValue);
        break;
    case WaveFlux::Dsp::ParameterType::Int:
        setParameterValue(def->id, static_cast<int>(def->defaultValue));
        break;
    case WaveFlux::Dsp::ParameterType::Bool:
        setParameterValue(def->id, def->defaultValue > 0.5);
        break;
    case WaveFlux::Dsp::ParameterType::StringEnum:
        setParameterValue(def->id, def->enumValues.isEmpty() ? QString() : def->enumValues.first());
        break;
    }
}

void DspSettingsManager::resetGroup(const QString &groupId)
{
    const auto defs = WaveFlux::Dsp::parametersForGroup(groupId);
    for (const auto &def : defs) {
        resetParameter(def.id);
    }
}

void DspSettingsManager::resetTab(const QString &tabId)
{
    const auto defs = WaveFlux::Dsp::parametersForTab(tabId);
    for (const auto &def : defs) {
        resetParameter(def.id);
    }
}

void DspSettingsManager::resetAllDsp()
{
    setDefaults();
    emit echoMixChanged();
    emit chorusMixChanged();
    emit speedChanged();
    emit reverbMixChanged();
    emit bassChanged();
    emit tempoChanged();
    emit flangerMixChanged();
    emit stereoWidthChanged();
    emit tonalitySemitonesChanged();
    emit voiceSuppressionChanged();
    emit fadePauseResumeChanged();
    emit fadeTrackNavigationChanged();
    emit smoothChangesChanged();
    emit logarithmicControlChanged();
    emit loudnessCompensationChanged();
    emit balanceChanged();
    emit amplitudeNormalizationEnabledChanged();
    emit amplitudeTargetPeakDbfsChanged();
    emit amplitudePreampDbChanged();
    emit amplitudeUseTagValuesChanged();
    emit replayGainEnabledChanged();
    emit replayGainModeChanged();
    emit replayGainFallbackDbChanged();
    emit replayGainAnalyzeOnTheFlyChanged();
    emit replayGainPreampDbChanged();
    emit replayGainUseTagsChanged();
    emit replayGainTagSourceChanged();
    emit mixAutoAdvanceChanged();
    emit mixEnabledChanged();
    emit mixManualCrossfadeChanged();
    emit mixManualCrossfadeMsChanged();
    emit mixManualFadeOutChanged();
    emit mixManualFadeOutMsChanged();
    emit mixManualFadeInChanged();
    emit mixManualFadeInMsChanged();
    emit mixAutomaticModeChanged();
    emit mixAutomaticPauseMsChanged();
    emit mixAutomaticCrossfadeMsChanged();
    emit mixAutomaticFadeOutMsChanged();
    emit mixAutomaticFadeInMsChanged();
    emit silenceRemovalEnabledChanged();
    emit silenceRemovalMinimumDurationMsChanged();
    emit silenceRemovalThresholdDbfsChanged();
    emit silenceRemovalTrimEdgesChanged();
    emit dspSettingsChanged();
    saveSettings();
}

QVariantMap DspSettingsManager::exportDspProfileV1() const
{
    QVariantMap map;
    map.insert(QStringLiteral("schema"), QStringLiteral("waveflux.dsp.profiles.v1"));

    QVariantMap params;
    const auto &defs = WaveFlux::Dsp::allParameterDefinitions();
    for (const auto &def : defs) {
        params.insert(def.id, getParameterValue(def.id));
    }
    map.insert(QStringLiteral("parameters"), params);
    return map;
}

bool DspSettingsManager::importDspProfileV1(const QVariantMap &profileMap)
{
    if (profileMap.value(QStringLiteral("schema")).toString() != QStringLiteral("waveflux.dsp.profiles.v1")) {
        return false;
    }
    const QVariantMap params = profileMap.value(QStringLiteral("parameters")).toMap();
    const auto &defs = WaveFlux::Dsp::allParameterDefinitions();
    for (const auto &def : defs) {
        if (params.contains(def.id)) {
            const QVariant val = WaveFlux::Dsp::sanitizeValue(def, params.value(def.id));
            setParameterValue(def.id, val);
        }
    }
    saveSettings();
    return true;
}
