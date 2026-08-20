#ifndef DSPSETTINGSMANAGER_H
#define DSPSETTINGSMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <QSettings>
#include "dsp/DspParameters.h"

class DspSettingsManager : public QObject
{
    Q_OBJECT

    // --- General Tab Adjustments ---
    Q_PROPERTY(double echoMix READ echoMix WRITE setEchoMix NOTIFY echoMixChanged)
    Q_PROPERTY(double chorusMix READ chorusMix WRITE setChorusMix NOTIFY chorusMixChanged)
    Q_PROPERTY(double speed READ speed WRITE setSpeed NOTIFY speedChanged)
    Q_PROPERTY(double reverbMix READ reverbMix WRITE setReverbMix NOTIFY reverbMixChanged)
    Q_PROPERTY(double bass READ bass WRITE setBass NOTIFY bassChanged)
    Q_PROPERTY(double tempo READ tempo WRITE setTempo NOTIFY tempoChanged)
    Q_PROPERTY(double flangerMix READ flangerMix WRITE setFlangerMix NOTIFY flangerMixChanged)
    Q_PROPERTY(double stereoWidth READ stereoWidth WRITE setStereoWidth NOTIFY stereoWidthChanged)
    Q_PROPERTY(double tonalitySemitones READ tonalitySemitones WRITE setTonalitySemitones NOTIFY tonalitySemitonesChanged)

    // --- General Tab Switches ---
    Q_PROPERTY(bool voiceSuppression READ voiceSuppression WRITE setVoiceSuppression NOTIFY voiceSuppressionChanged)
    Q_PROPERTY(bool fadePauseResume READ fadePauseResume WRITE setFadePauseResume NOTIFY fadePauseResumeChanged)
    Q_PROPERTY(bool fadeTrackNavigation READ fadeTrackNavigation WRITE setFadeTrackNavigation NOTIFY fadeTrackNavigationChanged)

    // --- Volume Tab ---
    Q_PROPERTY(bool smoothChanges READ smoothChanges WRITE setSmoothChanges NOTIFY smoothChangesChanged)
    Q_PROPERTY(bool logarithmicControl READ logarithmicControl WRITE setLogarithmicControl NOTIFY logarithmicControlChanged)
    Q_PROPERTY(bool loudnessCompensation READ loudnessCompensation WRITE setLoudnessCompensation NOTIFY loudnessCompensationChanged)
    Q_PROPERTY(double balance READ balance WRITE setBalance NOTIFY balanceChanged)

    // --- Amplitude Normalization ---
    Q_PROPERTY(bool amplitudeNormalizationEnabled READ amplitudeNormalizationEnabled WRITE setAmplitudeNormalizationEnabled NOTIFY amplitudeNormalizationEnabledChanged)
    Q_PROPERTY(double amplitudeTargetPeakDbfs READ amplitudeTargetPeakDbfs WRITE setAmplitudeTargetPeakDbfs NOTIFY amplitudeTargetPeakDbfsChanged)
    Q_PROPERTY(double amplitudePreampDb READ amplitudePreampDb WRITE setAmplitudePreampDb NOTIFY amplitudePreampDbChanged)
    Q_PROPERTY(bool amplitudeUseTagValues READ amplitudeUseTagValues WRITE setAmplitudeUseTagValues NOTIFY amplitudeUseTagValuesChanged)

    // --- ReplayGain ---
    Q_PROPERTY(bool replayGainEnabled READ replayGainEnabled WRITE setReplayGainEnabled NOTIFY replayGainEnabledChanged)
    Q_PROPERTY(QString replayGainMode READ replayGainMode WRITE setReplayGainMode NOTIFY replayGainModeChanged)
    Q_PROPERTY(double replayGainFallbackDb READ replayGainFallbackDb WRITE setReplayGainFallbackDb NOTIFY replayGainFallbackDbChanged)
    Q_PROPERTY(bool replayGainAnalyzeOnTheFly READ replayGainAnalyzeOnTheFly WRITE setReplayGainAnalyzeOnTheFly NOTIFY replayGainAnalyzeOnTheFlyChanged)
    Q_PROPERTY(double replayGainPreampDb READ replayGainPreampDb WRITE setReplayGainPreampDb NOTIFY replayGainPreampDbChanged)
    Q_PROPERTY(bool replayGainUseTags READ replayGainUseTags WRITE setReplayGainUseTags NOTIFY replayGainUseTagsChanged)
    Q_PROPERTY(QString replayGainTagSource READ replayGainTagSource WRITE setReplayGainTagSource NOTIFY replayGainTagSourceChanged)
    Q_PROPERTY(QString effectiveReplayGainDiagnostic READ effectiveReplayGainDiagnostic NOTIFY effectiveReplayGainDiagnosticChanged)

    // --- Mix Tab ---
    Q_PROPERTY(bool mixAutoAdvance READ mixAutoAdvance WRITE setMixAutoAdvance NOTIFY mixAutoAdvanceChanged)
    Q_PROPERTY(bool mixEnabled READ mixEnabled WRITE setMixEnabled NOTIFY mixEnabledChanged)
    Q_PROPERTY(bool mixManualCrossfade READ mixManualCrossfade WRITE setMixManualCrossfade NOTIFY mixManualCrossfadeChanged)
    Q_PROPERTY(int mixManualCrossfadeMs READ mixManualCrossfadeMs WRITE setMixManualCrossfadeMs NOTIFY mixManualCrossfadeMsChanged)
    Q_PROPERTY(bool mixManualFadeOut READ mixManualFadeOut WRITE setMixManualFadeOut NOTIFY mixManualFadeOutChanged)
    Q_PROPERTY(int mixManualFadeOutMs READ mixManualFadeOutMs WRITE setMixManualFadeOutMs NOTIFY mixManualFadeOutMsChanged)
    Q_PROPERTY(bool mixManualFadeIn READ mixManualFadeIn WRITE setMixManualFadeIn NOTIFY mixManualFadeInChanged)
    Q_PROPERTY(int mixManualFadeInMs READ mixManualFadeInMs WRITE setMixManualFadeInMs NOTIFY mixManualFadeInMsChanged)
    Q_PROPERTY(QString mixAutomaticMode READ mixAutomaticMode WRITE setMixAutomaticMode NOTIFY mixAutomaticModeChanged)
    Q_PROPERTY(int mixAutomaticPauseMs READ mixAutomaticPauseMs WRITE setMixAutomaticPauseMs NOTIFY mixAutomaticPauseMsChanged)
    Q_PROPERTY(int mixAutomaticCrossfadeMs READ mixAutomaticCrossfadeMs WRITE setMixAutomaticCrossfadeMs NOTIFY mixAutomaticCrossfadeMsChanged)
    Q_PROPERTY(int mixAutomaticFadeOutMs READ mixAutomaticFadeOutMs WRITE setMixAutomaticFadeOutMs NOTIFY mixAutomaticFadeOutMsChanged)
    Q_PROPERTY(int mixAutomaticFadeInMs READ mixAutomaticFadeInMs WRITE setMixAutomaticFadeInMs NOTIFY mixAutomaticFadeInMsChanged)

    // --- Silence Removal Tab ---
    Q_PROPERTY(bool silenceRemovalEnabled READ silenceRemovalEnabled WRITE setSilenceRemovalEnabled NOTIFY silenceRemovalEnabledChanged)
    Q_PROPERTY(int silenceRemovalMinimumDurationMs READ silenceRemovalMinimumDurationMs WRITE setSilenceRemovalMinimumDurationMs NOTIFY silenceRemovalMinimumDurationMsChanged)
    Q_PROPERTY(double silenceRemovalThresholdDbfs READ silenceRemovalThresholdDbfs WRITE setSilenceRemovalThresholdDbfs NOTIFY silenceRemovalThresholdDbfsChanged)
    Q_PROPERTY(bool silenceRemovalTrimEdges READ silenceRemovalTrimEdges WRITE setSilenceRemovalTrimEdges NOTIFY silenceRemovalTrimEdgesChanged)

    // --- UI State ---
    Q_PROPERTY(QString lastSelectedTab READ lastSelectedTab WRITE setLastSelectedTab NOTIFY lastSelectedTabChanged)

public:
    explicit DspSettingsManager(QObject *parent = nullptr);
    ~DspSettingsManager() override;

    static DspSettingsManager *instance();

    // Getters
    double echoMix() const { return m_echoMix; }
    double chorusMix() const { return m_chorusMix; }
    double speed() const { return m_speed; }
    double reverbMix() const { return m_reverbMix; }
    double bass() const { return m_bass; }
    double tempo() const { return m_tempo; }
    double flangerMix() const { return m_flangerMix; }
    double stereoWidth() const { return m_stereoWidth; }
    double tonalitySemitones() const { return m_tonalitySemitones; }

    bool voiceSuppression() const { return m_voiceSuppression; }
    bool fadePauseResume() const { return m_fadePauseResume; }
    bool fadeTrackNavigation() const { return m_fadeTrackNavigation; }

    bool smoothChanges() const { return m_smoothChanges; }
    bool logarithmicControl() const { return m_logarithmicControl; }
    bool loudnessCompensation() const { return m_loudnessCompensation; }
    double balance() const { return m_balance; }

    bool amplitudeNormalizationEnabled() const { return m_amplitudeNormalizationEnabled; }
    double amplitudeTargetPeakDbfs() const { return m_amplitudeTargetPeakDbfs; }
    double amplitudePreampDb() const { return m_amplitudePreampDb; }
    bool amplitudeUseTagValues() const { return m_amplitudeUseTagValues; }

    bool replayGainEnabled() const { return m_replayGainEnabled; }
    QString replayGainMode() const { return m_replayGainMode; }
    double replayGainFallbackDb() const { return m_replayGainFallbackDb; }
    bool replayGainAnalyzeOnTheFly() const { return m_replayGainAnalyzeOnTheFly; }
    double replayGainPreampDb() const { return m_replayGainPreampDb; }
    bool replayGainUseTags() const { return m_replayGainUseTags; }
    QString replayGainTagSource() const { return m_replayGainTagSource; }
    QString effectiveReplayGainDiagnostic() const { return m_effectiveReplayGainDiagnostic; }

    bool mixAutoAdvance() const { return m_mixAutoAdvance; }
    bool mixEnabled() const { return m_mixEnabled; }
    bool mixManualCrossfade() const { return m_mixManualCrossfade; }
    int mixManualCrossfadeMs() const { return m_mixManualCrossfadeMs; }
    bool mixManualFadeOut() const { return m_mixManualFadeOut; }
    int mixManualFadeOutMs() const { return m_mixManualFadeOutMs; }
    bool mixManualFadeIn() const { return m_mixManualFadeIn; }
    int mixManualFadeInMs() const { return m_mixManualFadeInMs; }
    QString mixAutomaticMode() const { return m_mixAutomaticMode; }
    int mixAutomaticPauseMs() const { return m_mixAutomaticPauseMs; }
    int mixAutomaticCrossfadeMs() const { return m_mixAutomaticCrossfadeMs; }
    int mixAutomaticFadeOutMs() const { return m_mixAutomaticFadeOutMs; }
    int mixAutomaticFadeInMs() const { return m_mixAutomaticFadeInMs; }

    bool silenceRemovalEnabled() const { return m_silenceRemovalEnabled; }
    int silenceRemovalMinimumDurationMs() const { return m_silenceRemovalMinimumDurationMs; }
    double silenceRemovalThresholdDbfs() const { return m_silenceRemovalThresholdDbfs; }
    bool silenceRemovalTrimEdges() const { return m_silenceRemovalTrimEdges; }

    QString lastSelectedTab() const { return m_lastSelectedTab; }

    // Setters
    void setEchoMix(double value);
    void setChorusMix(double value);
    void setSpeed(double value);
    void setReverbMix(double value);
    void setBass(double value);
    void setTempo(double value);
    void setFlangerMix(double value);
    void setStereoWidth(double value);
    void setTonalitySemitones(double value);

    void setVoiceSuppression(bool value);
    void setFadePauseResume(bool value);
    void setFadeTrackNavigation(bool value);

    void setSmoothChanges(bool value);
    void setLogarithmicControl(bool value);
    void setLoudnessCompensation(bool value);
    void setBalance(double value);

    void setAmplitudeNormalizationEnabled(bool value);
    void setAmplitudeTargetPeakDbfs(double value);
    void setAmplitudePreampDb(double value);
    void setAmplitudeUseTagValues(bool value);

    void setReplayGainEnabled(bool value);
    void setReplayGainMode(const QString &value);
    void setReplayGainFallbackDb(double value);
    void setReplayGainAnalyzeOnTheFly(bool value);
    void setReplayGainPreampDb(double value);
    void setReplayGainUseTags(bool value);
    void setReplayGainTagSource(const QString &value);
    void setEffectiveReplayGainDiagnostic(const QString &diag);

    void setMixAutoAdvance(bool value);
    void setMixEnabled(bool value);
    void setMixManualCrossfade(bool value);
    void setMixManualCrossfadeMs(int value);
    void setMixManualFadeOut(bool value);
    void setMixManualFadeOutMs(int value);
    void setMixManualFadeIn(bool value);
    void setMixManualFadeInMs(int value);
    void setMixAutomaticMode(const QString &value);
    void setMixAutomaticPauseMs(int value);
    void setMixAutomaticCrossfadeMs(int value);
    void setMixAutomaticFadeOutMs(int value);
    void setMixAutomaticFadeInMs(int value);

    void setSilenceRemovalEnabled(bool value);
    void setSilenceRemovalMinimumDurationMs(int value);
    void setSilenceRemovalThresholdDbfs(double value);
    void setSilenceRemovalTrimEdges(bool value);

    void setLastSelectedTab(const QString &tabId);

    // Invokables
    Q_INVOKABLE void resetParameter(const QString &paramId);
    Q_INVOKABLE void resetGroup(const QString &groupId);
    Q_INVOKABLE void resetTab(const QString &tabId);
    Q_INVOKABLE void resetAllDsp();

    Q_INVOKABLE QVariant getParameterValue(const QString &paramId) const;
    Q_INVOKABLE void setParameterValue(const QString &paramId, const QVariant &value);
    Q_INVOKABLE QVariantMap getParameterDefinition(const QString &paramId) const;
    Q_INVOKABLE QVariantList listParametersForTab(const QString &tabId) const;

    Q_INVOKABLE QVariantMap exportDspProfileV1() const;
    Q_INVOKABLE bool importDspProfileV1(const QVariantMap &profileMap);

signals:
    void echoMixChanged();
    void chorusMixChanged();
    void speedChanged();
    void reverbMixChanged();
    void bassChanged();
    void tempoChanged();
    void flangerMixChanged();
    void stereoWidthChanged();
    void tonalitySemitonesChanged();

    void voiceSuppressionChanged();
    void fadePauseResumeChanged();
    void fadeTrackNavigationChanged();

    void smoothChangesChanged();
    void logarithmicControlChanged();
    void loudnessCompensationChanged();
    void balanceChanged();

    void amplitudeNormalizationEnabledChanged();
    void amplitudeTargetPeakDbfsChanged();
    void amplitudePreampDbChanged();
    void amplitudeUseTagValuesChanged();

    void replayGainEnabledChanged();
    void replayGainModeChanged();
    void replayGainFallbackDbChanged();
    void replayGainAnalyzeOnTheFlyChanged();
    void replayGainPreampDbChanged();
    void replayGainUseTagsChanged();
    void replayGainTagSourceChanged();
    void effectiveReplayGainDiagnosticChanged();

    void mixAutoAdvanceChanged();
    void mixEnabledChanged();
    void mixManualCrossfadeChanged();
    void mixManualCrossfadeMsChanged();
    void mixManualFadeOutChanged();
    void mixManualFadeOutMsChanged();
    void mixManualFadeInChanged();
    void mixManualFadeInMsChanged();
    void mixAutomaticModeChanged();
    void mixAutomaticPauseMsChanged();
    void mixAutomaticCrossfadeMsChanged();
    void mixAutomaticFadeOutMsChanged();
    void mixAutomaticFadeInMsChanged();

    void silenceRemovalEnabledChanged();
    void silenceRemovalMinimumDurationMsChanged();
    void silenceRemovalThresholdDbfsChanged();
    void silenceRemovalTrimEdgesChanged();

    void lastSelectedTabChanged();
    void dspSettingsChanged();

public slots:
    void loadSettings();
    void saveSettings();

private:
    void setDefaults();
    void checkAndPerformMigration();

    static inline DspSettingsManager *s_instance = nullptr;

    double m_echoMix = 0.0;
    double m_chorusMix = 0.0;
    double m_speed = 1.00;
    double m_reverbMix = 0.0;
    double m_bass = 1.00;
    double m_tempo = 1.00;
    double m_flangerMix = 0.0;
    double m_stereoWidth = 1.00;
    double m_tonalitySemitones = 0.00;

    bool m_voiceSuppression = false;
    bool m_fadePauseResume = false;
    bool m_fadeTrackNavigation = false;

    bool m_smoothChanges = false;
    bool m_logarithmicControl = false;
    bool m_loudnessCompensation = false;
    double m_balance = 0.00;

    bool m_amplitudeNormalizationEnabled = false;
    double m_amplitudeTargetPeakDbfs = -1.0;
    double m_amplitudePreampDb = 0.0;
    bool m_amplitudeUseTagValues = true;

    bool m_replayGainEnabled = false;
    QString m_replayGainMode = QStringLiteral("auto");
    double m_replayGainFallbackDb = 0.0;
    bool m_replayGainAnalyzeOnTheFly = false;
    double m_replayGainPreampDb = 0.0;
    bool m_replayGainUseTags = true;
    QString m_replayGainTagSource = QStringLiteral("auto");
    QString m_effectiveReplayGainDiagnostic;

    bool m_mixAutoAdvance = true;
    bool m_mixEnabled = false;
    bool m_mixManualCrossfade = false;
    int m_mixManualCrossfadeMs = 1000;
    bool m_mixManualFadeOut = true;
    int m_mixManualFadeOutMs = 500;
    bool m_mixManualFadeIn = true;
    int m_mixManualFadeInMs = 500;
    QString m_mixAutomaticMode = QStringLiteral("none");
    int m_mixAutomaticPauseMs = 1000;
    int m_mixAutomaticCrossfadeMs = 1000;
    int m_mixAutomaticFadeOutMs = 1000;
    int m_mixAutomaticFadeInMs = 1000;

    bool m_silenceRemovalEnabled = false;
    int m_silenceRemovalMinimumDurationMs = 500;
    double m_silenceRemovalThresholdDbfs = -60.0;
    bool m_silenceRemovalTrimEdges = true;

    QString m_lastSelectedTab = QStringLiteral("general");
};

#endif // DSPSETTINGSMANAGER_H
