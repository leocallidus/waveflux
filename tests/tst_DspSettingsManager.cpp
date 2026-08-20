#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QSettings>
#include <QSet>
#include <QSignalSpy>
#include "DspSettingsManager.h"
#include "dsp/DspParameters.h"
#include "dsp/DspCapabilities.h"

class tst_DspSettingsManager : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void testDefaultsAndNeutralValues();
    void testParameterDefinitionsMatchSpecification();
    void testParameterClamping();
    void testMutualExclusivityAmplitudeAndReplayGain();
    void testResetParameter();
    void testResetTab();
    void testResetAllDsp();
    void testExportAndImportProfile();
    void testCapabilities();
};

void tst_DspSettingsManager::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral("WaveFluxTest"));
    QCoreApplication::setApplicationName(QStringLiteral("tst_DspSettingsManager"));
}

void tst_DspSettingsManager::cleanupTestCase()
{
    QSettings settings;
    settings.clear();
}

void tst_DspSettingsManager::init()
{
    QSettings settings;
    settings.clear();
}

void tst_DspSettingsManager::cleanup()
{
    QSettings settings;
    settings.clear();
}

void tst_DspSettingsManager::testDefaultsAndNeutralValues()
{
    DspSettingsManager manager;

    // General Tab
    QCOMPARE(manager.echoMix(), 0.0);
    QCOMPARE(manager.chorusMix(), 0.0);
    QCOMPARE(manager.speed(), 1.0);
    QCOMPARE(manager.reverbMix(), 0.0);
    QCOMPARE(manager.bass(), 1.0);
    QCOMPARE(manager.tempo(), 1.0);
    QCOMPARE(manager.flangerMix(), 0.0);
    QCOMPARE(manager.stereoWidth(), 1.0);
    QCOMPARE(manager.tonalitySemitones(), 0.0);
    QCOMPARE(manager.voiceSuppression(), false);
    QCOMPARE(manager.fadePauseResume(), false);
    QCOMPARE(manager.fadeTrackNavigation(), false);

    // Volume Tab
    QCOMPARE(manager.smoothChanges(), false);
    QCOMPARE(manager.logarithmicControl(), false);
    QCOMPARE(manager.loudnessCompensation(), false);
    QCOMPARE(manager.balance(), 0.0);
    QCOMPARE(manager.amplitudeNormalizationEnabled(), false);
    QCOMPARE(manager.amplitudeTargetPeakDbfs(), -1.0);
    QCOMPARE(manager.amplitudePreampDb(), 0.0);
    QCOMPARE(manager.amplitudeUseTagValues(), true);
    QCOMPARE(manager.replayGainEnabled(), false);
    QCOMPARE(manager.replayGainMode(), QStringLiteral("auto"));
    QCOMPARE(manager.replayGainFallbackDb(), 0.0);
    QCOMPARE(manager.replayGainAnalyzeOnTheFly(), false);
    QCOMPARE(manager.replayGainPreampDb(), 0.0);
    QCOMPARE(manager.replayGainUseTags(), true);
    QCOMPARE(manager.replayGainTagSource(), QStringLiteral("auto"));

    // Mix Tab
    QCOMPARE(manager.mixAutoAdvance(), true);
    QCOMPARE(manager.mixEnabled(), false);
    QCOMPARE(manager.mixManualCrossfade(), false);
    QCOMPARE(manager.mixManualCrossfadeMs(), 1000);
    QCOMPARE(manager.mixManualFadeOut(), true);
    QCOMPARE(manager.mixManualFadeOutMs(), 500);
    QCOMPARE(manager.mixManualFadeIn(), true);
    QCOMPARE(manager.mixManualFadeInMs(), 500);
    QCOMPARE(manager.mixAutomaticMode(), QStringLiteral("none"));
    QCOMPARE(manager.mixAutomaticPauseMs(), 1000);
    QCOMPARE(manager.mixAutomaticCrossfadeMs(), 1000);
    QCOMPARE(manager.mixAutomaticFadeOutMs(), 1000);
    QCOMPARE(manager.mixAutomaticFadeInMs(), 1000);

    // Silence Removal Tab
    QCOMPARE(manager.silenceRemovalEnabled(), false);
    QCOMPARE(manager.silenceRemovalMinimumDurationMs(), 500);
    QCOMPARE(manager.silenceRemovalThresholdDbfs(), -60.0);
    QCOMPARE(manager.silenceRemovalTrimEdges(), true);
}

void tst_DspSettingsManager::testParameterDefinitionsMatchSpecification()
{
    const auto &defs = WaveFlux::Dsp::allParameterDefinitions();
    QVERIFY(!defs.empty());

    QSet<QString> ids;
    QStringList expectedTabs = {
        QStringLiteral("general"),
        QStringLiteral("eq"),
        QStringLiteral("volume"),
        QStringLiteral("mix"),
        QStringLiteral("silenceRemoval")
    };

    for (const auto &def : defs) {
        QVERIFY2(!ids.contains(def.id), qPrintable(def.id));
        ids.insert(def.id);
        QVERIFY(expectedTabs.contains(def.tabId) || def.tabId == QStringLiteral("eq"));
        QVERIFY(def.maxValue >= def.minValue);
        QVERIFY(def.step > 0.0 || def.type == WaveFlux::Dsp::ParameterType::Bool
                || def.type == WaveFlux::Dsp::ParameterType::StringEnum);
        QVERIFY(def.neutralValue >= def.minValue - 1e-9);
        QVERIFY(def.neutralValue <= def.maxValue + 1e-9);
    }

    QVERIFY(WaveFlux::Dsp::findParameterDefinition(QStringLiteral("general.echoMix")));
    QVERIFY(WaveFlux::Dsp::findParameterDefinition(QStringLiteral("general.speed")));
    QVERIFY(WaveFlux::Dsp::findParameterDefinition(QStringLiteral("general.tempo")));
    QVERIFY(WaveFlux::Dsp::findParameterDefinition(QStringLiteral("general.tonalitySemitones")));
    QVERIFY(WaveFlux::Dsp::findParameterDefinition(QStringLiteral("volume.balance")));
    QVERIFY(WaveFlux::Dsp::findParameterDefinition(QStringLiteral("mix.enabled")));
    QVERIFY(WaveFlux::Dsp::findParameterDefinition(QStringLiteral("silenceRemoval.enabled")));
}

void tst_DspSettingsManager::testParameterClamping()
{
    DspSettingsManager manager;

    manager.setEchoMix(150.0);
    QCOMPARE(manager.echoMix(), 100.0);
    manager.setEchoMix(-10.0);
    QCOMPARE(manager.echoMix(), 0.0);

    manager.setSpeed(5.0);
    QCOMPARE(manager.speed(), 3.0);
    manager.setSpeed(0.05);
    QCOMPARE(manager.speed(), 0.25);

    manager.setBass(10.0);
    QCOMPARE(manager.bass(), 2.0);
    manager.setBass(-1.0);
    QCOMPARE(manager.bass(), 0.0);

    manager.setTonalitySemitones(15.0);
    QCOMPARE(manager.tonalitySemitones(), 10.0);
    manager.setTonalitySemitones(-25.0);
    QCOMPARE(manager.tonalitySemitones(), -10.0);

    manager.setBalance(2.5);
    QCOMPARE(manager.balance(), 1.0);
    manager.setBalance(-3.0);
    QCOMPARE(manager.balance(), -1.0);

    manager.setSilenceRemovalThresholdDbfs(0.0);
    QCOMPARE(manager.silenceRemovalThresholdDbfs(), -20.0);
    manager.setSilenceRemovalThresholdDbfs(-120.0);
    QCOMPARE(manager.silenceRemovalThresholdDbfs(), -90.0);
}

void tst_DspSettingsManager::testMutualExclusivityAmplitudeAndReplayGain()
{
    DspSettingsManager manager;

    manager.setAmplitudeNormalizationEnabled(true);
    QVERIFY(manager.amplitudeNormalizationEnabled());
    QVERIFY(!manager.replayGainEnabled());

    // Enabling ReplayGain should disable Amplitude Normalization
    manager.setReplayGainEnabled(true);
    QVERIFY(manager.replayGainEnabled());
    QVERIFY(!manager.amplitudeNormalizationEnabled());

    // Enabling Amplitude Normalization should disable ReplayGain
    manager.setAmplitudeNormalizationEnabled(true);
    QVERIFY(manager.amplitudeNormalizationEnabled());
    QVERIFY(!manager.replayGainEnabled());
}

void tst_DspSettingsManager::testResetParameter()
{
    DspSettingsManager manager;

    manager.setEchoMix(45.0);
    manager.setSpeed(1.75);
    manager.setBalance(-0.5);

    QCOMPARE(manager.echoMix(), 45.0);
    manager.resetParameter(QStringLiteral("general.echoMix"));
    QCOMPARE(manager.echoMix(), 0.0);

    QCOMPARE(manager.speed(), 1.75);
    manager.resetParameter(QStringLiteral("general.speed"));
    QCOMPARE(manager.speed(), 1.0);

    QCOMPARE(manager.balance(), -0.5);
    manager.resetParameter(QStringLiteral("volume.balance"));
    QCOMPARE(manager.balance(), 0.0);
}

void tst_DspSettingsManager::testResetTab()
{
    DspSettingsManager manager;

    manager.setEchoMix(50.0);
    manager.setChorusMix(40.0);
    manager.setSpeed(2.0);
    manager.setBalance(0.8);
    manager.setAmplitudeNormalizationEnabled(true);

    manager.resetTab(QStringLiteral("general"));
    QCOMPARE(manager.echoMix(), 0.0);
    QCOMPARE(manager.chorusMix(), 0.0);
    QCOMPARE(manager.speed(), 1.0);
    // Volume tab properties should remain untouched
    QCOMPARE(manager.balance(), 0.8);
    QCOMPARE(manager.amplitudeNormalizationEnabled(), true);

    manager.resetTab(QStringLiteral("volume"));
    QCOMPARE(manager.balance(), 0.0);
    QCOMPARE(manager.amplitudeNormalizationEnabled(), false);
}

void tst_DspSettingsManager::testResetAllDsp()
{
    DspSettingsManager manager;

    manager.setEchoMix(30.0);
    manager.setReverbMix(50.0);
    manager.setSpeed(1.5);
    manager.setBalance(0.4);
    manager.setMixEnabled(true);
    manager.setSilenceRemovalEnabled(true);

    manager.resetAllDsp();

    QCOMPARE(manager.echoMix(), 0.0);
    QCOMPARE(manager.reverbMix(), 0.0);
    QCOMPARE(manager.speed(), 1.0);
    QCOMPARE(manager.balance(), 0.0);
    QCOMPARE(manager.mixEnabled(), false);
    QCOMPARE(manager.silenceRemovalEnabled(), false);
}

void tst_DspSettingsManager::testExportAndImportProfile()
{
    DspSettingsManager manager;

    manager.setEchoMix(25.0);
    manager.setChorusMix(35.0);
    manager.setSpeed(1.25);
    manager.setBass(1.4);
    manager.setStereoWidth(2.5);
    manager.setTonalitySemitones(-3.5);
    manager.setBalance(0.25);
    manager.setMixAutoAdvance(false);
    manager.setSilenceRemovalEnabled(true);
    manager.setSilenceRemovalThresholdDbfs(-55.0);

    const QVariantMap profileMap = manager.exportDspProfileV1();
    QVERIFY(!profileMap.isEmpty());

    // Reset everything
    manager.resetAllDsp();
    QCOMPARE(manager.echoMix(), 0.0);
    QCOMPARE(manager.speed(), 1.0);
    QCOMPARE(manager.mixAutoAdvance(), true);
    QCOMPARE(manager.silenceRemovalEnabled(), false);

    // Import the exported profile
    const bool importOk = manager.importDspProfileV1(profileMap);
    QVERIFY(importOk);

    QCOMPARE(manager.echoMix(), 25.0);
    QCOMPARE(manager.chorusMix(), 35.0);
    QCOMPARE(manager.speed(), 1.25);
    QCOMPARE(manager.bass(), 1.4);
    QCOMPARE(manager.stereoWidth(), 2.5);
    QCOMPARE(manager.tonalitySemitones(), -3.5);
    QCOMPARE(manager.balance(), 0.25);
    QCOMPARE(manager.mixAutoAdvance(), false);
    QCOMPARE(manager.silenceRemovalEnabled(), true);
    QCOMPARE(manager.silenceRemovalThresholdDbfs(), -55.0);
}

void tst_DspSettingsManager::testCapabilities()
{
    WaveFlux::Dsp::SourceAudioContext stereoContext;
    stereoContext.backendKind = WaveFlux::PlaybackBackendKind::GStreamer;
    stereoContext.channelCount = 2;
    stereoContext.isLiveStream = false;

    QVERIFY(WaveFlux::Dsp::DspCapabilities::isCapabilitySupported(QStringLiteral("dsp.voiceSuppression"), stereoContext));
    QVERIFY(WaveFlux::Dsp::DspCapabilities::isCapabilitySupported(QStringLiteral("dsp.stereoWidth"), stereoContext));
    QVERIFY(WaveFlux::Dsp::DspCapabilities::isCapabilitySupported(QStringLiteral("dsp.silenceRemoval"), stereoContext));

    WaveFlux::Dsp::SourceAudioContext monoContext;
    monoContext.backendKind = WaveFlux::PlaybackBackendKind::GStreamer;
    monoContext.channelCount = 1;
    monoContext.isLiveStream = false;

    QVERIFY(!WaveFlux::Dsp::DspCapabilities::isCapabilitySupported(QStringLiteral("dsp.voiceSuppression"), monoContext));
    QVERIFY(!WaveFlux::Dsp::DspCapabilities::isCapabilitySupported(QStringLiteral("dsp.stereoWidth"), monoContext));

    WaveFlux::Dsp::SourceAudioContext liveContext;
    liveContext.backendKind = WaveFlux::PlaybackBackendKind::GStreamer;
    liveContext.channelCount = 2;
    liveContext.isLiveStream = true;

    QVERIFY(!WaveFlux::Dsp::DspCapabilities::isCapabilitySupported(QStringLiteral("dsp.silenceRemoval"), liveContext));
}

QTEST_MAIN(tst_DspSettingsManager)
#include "tst_DspSettingsManager.moc"
