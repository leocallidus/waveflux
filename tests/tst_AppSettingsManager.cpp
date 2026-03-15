#include <QtTest>

#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <QSignalSpy>
#include <limits>

#include "AppSettingsManager.h"

namespace {
constexpr quint32 kDefaultShuffleSeed = 0xC4E5D2A1u;

void clearSettings()
{
    QSettings settings(QStringLiteral("WaveFlux"), QStringLiteral("WaveFlux"));
    settings.clear();
    settings.sync();
}

QVariantList gains(const QList<double> &values)
{
    QVariantList result;
    result.reserve(values.size());
    for (double value : values) {
        result.push_back(value);
    }
    return result;
}
} // namespace

class AppSettingsManagerTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void persistsAndReloadsSettings();
    void sanitizesInvalidStoredValues();
    void signalsOnlyOnEffectiveChangesAndPersistsBurstUpdates();
};

void AppSettingsManagerTest::initTestCase()
{
    const QString settingsDir =
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("test_settings"));
    QDir().mkpath(settingsDir);
    qputenv("XDG_CONFIG_HOME", settingsDir.toUtf8());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, settingsDir);
    QSettings::setPath(QSettings::NativeFormat, QSettings::SystemScope, settingsDir);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir);
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, settingsDir);
    clearSettings();
}

void AppSettingsManagerTest::init()
{
    clearSettings();
}

void AppSettingsManagerTest::cleanup()
{
    clearSettings();
}

void AppSettingsManagerTest::persistsAndReloadsSettings()
{
    const QVariantList firstBandGains =
        gains({-3.0, -1.0, 0.0, 1.0, 2.0, -2.0, 0.5, 1.5, -0.5, 3.0});
    const QVariantList secondBandGains =
        gains({-4.0, -2.0, -1.0, 0.0, 1.0, 2.5, 3.0, 1.0, -1.5, 2.0});

    QVariantMap userPreset;
    userPreset.insert(QStringLiteral("id"), QStringLiteral("user:test"));
    userPreset.insert(QStringLiteral("name"), QStringLiteral("Test Preset"));
    userPreset.insert(QStringLiteral("gains"), firstBandGains);
    userPreset.insert(QStringLiteral("builtIn"), false);
    userPreset.insert(QStringLiteral("updatedAtMs"), static_cast<qint64>(123456));
    const QVariantList userPresets = {userPreset};

    {
        AppSettingsManager settings;
        settings.setLanguage(QStringLiteral("ru"));
        settings.setTrayEnabled(true);
        settings.setSidebarVisible(false);
        settings.setCollectionsSidebarVisible(false);
        settings.setSkinMode(QStringLiteral("compact"));
        settings.setWaveformHeight(180);
        settings.setCompactWaveformHeight(72);
        settings.setWaveformZoomHintsVisible(false);
        settings.setCueWaveformOverlayEnabled(false);
        settings.setCueWaveformOverlayLabelsEnabled(false);
        settings.setCueWaveformOverlayAutoHideOnZoom(false);
        settings.setShowSpeedPitchControls(true);
        settings.setReversePlayback(true);
        settings.setAudioQualityProfile(QStringLiteral("studio"));
        settings.setDynamicSpectrum(true);
        settings.setConfirmTrashDeletion(false);
        settings.setDeterministicShuffleEnabled(true);
        settings.setShuffleSeed(123456789u);
        settings.setRepeatableShuffle(false);
        settings.setSqliteLibraryEnabled(false);
        settings.setEqualizerBandGains(firstBandGains);
        settings.setEqualizerLastManualGains(secondBandGains);
        settings.setEqualizerUserPresets(userPresets);
        settings.setEqualizerActivePresetId(QStringLiteral("user:test"));
    }

    QSettings persisted(QStringLiteral("WaveFlux"), QStringLiteral("WaveFlux"));
    persisted.beginGroup(QStringLiteral("App"));
    const QStringList keys = persisted.allKeys();
    const QStringList expectedKeys = {
        QStringLiteral("language"),
        QStringLiteral("trayEnabled"),
        QStringLiteral("sidebarVisible"),
        QStringLiteral("collectionsSidebarVisible"),
        QStringLiteral("skinMode"),
        QStringLiteral("waveformHeight"),
        QStringLiteral("compactWaveformHeight"),
        QStringLiteral("waveform.zoomHintsVisible"),
        QStringLiteral("waveform.cueOverlayEnabled"),
        QStringLiteral("waveform.cueOverlayLabelsEnabled"),
        QStringLiteral("waveform.cueOverlayAutoHideOnZoom"),
        QStringLiteral("showSpeedPitchControls"),
        QStringLiteral("reversePlayback"),
        QStringLiteral("audioQualityProfile"),
        QStringLiteral("dynamicSpectrum"),
        QStringLiteral("confirmTrashDeletion"),
        QStringLiteral("deterministicShuffleEnabled"),
        QStringLiteral("shuffleSeed"),
        QStringLiteral("repeatableShuffle"),
        QStringLiteral("library.sqlite.enabled"),
        QStringLiteral("equalizerBandGains"),
        QStringLiteral("equalizer.lastManualGains"),
        QStringLiteral("equalizer.userPresets"),
        QStringLiteral("equalizer.activePresetId")
    };
    for (const QString &key : expectedKeys) {
        QVERIFY2(keys.contains(key), qPrintable(QStringLiteral("missing key: %1").arg(key)));
    }
    persisted.endGroup();

    AppSettingsManager reloaded;
    QCOMPARE(reloaded.language(), QStringLiteral("ru"));
    QCOMPARE(reloaded.trayEnabled(), true);
    QCOMPARE(reloaded.sidebarVisible(), false);
    QCOMPARE(reloaded.collectionsSidebarVisible(), false);
    QCOMPARE(reloaded.skinMode(), QStringLiteral("compact"));
    QCOMPARE(reloaded.waveformHeight(), 180);
    QCOMPARE(reloaded.compactWaveformHeight(), 72);
    QCOMPARE(reloaded.waveformZoomHintsVisible(), false);
    QCOMPARE(reloaded.cueWaveformOverlayEnabled(), false);
    QCOMPARE(reloaded.cueWaveformOverlayLabelsEnabled(), false);
    QCOMPARE(reloaded.cueWaveformOverlayAutoHideOnZoom(), false);
    QCOMPARE(reloaded.showSpeedPitchControls(), true);
    QCOMPARE(reloaded.reversePlayback(), true);
    QCOMPARE(reloaded.audioQualityProfile(), QStringLiteral("studio"));
    QCOMPARE(reloaded.dynamicSpectrum(), true);
    QCOMPARE(reloaded.confirmTrashDeletion(), false);
    QCOMPARE(reloaded.deterministicShuffleEnabled(), true);
    QCOMPARE(reloaded.shuffleSeed(), 123456789u);
    QCOMPARE(reloaded.repeatableShuffle(), false);
    QCOMPARE(reloaded.sqliteLibraryEnabled(), false);
    QCOMPARE(reloaded.equalizerBandGains(), secondBandGains);
    QCOMPARE(reloaded.equalizerLastManualGains(), secondBandGains);
    QCOMPARE(reloaded.equalizerUserPresets().size(), 1);
    QCOMPARE(reloaded.equalizerUserPresets().constFirst().toMap().value(QStringLiteral("id")).toString(),
             QStringLiteral("user:test"));
    QCOMPARE(reloaded.equalizerActivePresetId(), QStringLiteral("user:test"));
}

void AppSettingsManagerTest::sanitizesInvalidStoredValues()
{
    {
        QSettings settings(QStringLiteral("WaveFlux"), QStringLiteral("WaveFlux"));
        settings.beginGroup(QStringLiteral("App"));
        settings.setValue(QStringLiteral("language"), QStringLiteral("de"));
        settings.setValue(QStringLiteral("skinMode"), QStringLiteral("weird"));
        settings.setValue(QStringLiteral("audioQualityProfile"), QStringLiteral("broken"));
        settings.setValue(QStringLiteral("waveformHeight"), -999);
        settings.setValue(QStringLiteral("compactWaveformHeight"), 999);
        settings.setValue(QStringLiteral("shuffleSeed"), QStringLiteral("not-a-number"));
        settings.endGroup();
        settings.sync();
    }

    AppSettingsManager fromBrokenSeed;
    QCOMPARE(fromBrokenSeed.language(), QStringLiteral("auto"));
    QCOMPARE(fromBrokenSeed.skinMode(), QStringLiteral("normal"));
    QCOMPARE(fromBrokenSeed.audioQualityProfile(), QStringLiteral("standard"));
    QCOMPARE(fromBrokenSeed.waveformHeight(), 40);
    QCOMPARE(fromBrokenSeed.compactWaveformHeight(), 999);
    QCOMPARE(fromBrokenSeed.shuffleSeed(), kDefaultShuffleSeed);

    clearSettings();

    {
        QSettings settings(QStringLiteral("WaveFlux"), QStringLiteral("WaveFlux"));
        settings.beginGroup(QStringLiteral("App"));
        settings.setValue(QStringLiteral("shuffleSeed"), static_cast<qulonglong>(9999999999ULL));
        settings.endGroup();
        settings.sync();
    }

    AppSettingsManager fromHugeSeed;
    QCOMPARE(fromHugeSeed.shuffleSeed(), std::numeric_limits<quint32>::max());
}

void AppSettingsManagerTest::signalsOnlyOnEffectiveChangesAndPersistsBurstUpdates()
{
    AppSettingsManager settings;

    QSignalSpy waveformSpy(&settings, &AppSettingsManager::waveformHeightChanged);
    QSignalSpy seedSpy(&settings, &AppSettingsManager::shuffleSeedChanged);

    settings.setWaveformHeight(settings.waveformHeight());
    QCOMPARE(waveformSpy.count(), 0);

    settings.setWaveformHeight(101);
    QCOMPARE(waveformSpy.count(), 1);
    QCOMPARE(settings.waveformHeight(), 101);

    settings.setWaveformHeight(101);
    QCOMPARE(waveformSpy.count(), 1);

    settings.setWaveformHeight(999);
    QCOMPARE(settings.waveformHeight(), 999);
    QCOMPARE(waveformSpy.count(), 2);

    settings.setWaveformHeight(1001);
    QCOMPARE(settings.waveformHeight(), 1000);
    QCOMPARE(waveformSpy.count(), 3);

    settings.setShuffleSeed(settings.shuffleSeed());
    QCOMPARE(seedSpy.count(), 0);

    settings.setShuffleSeed(42u);
    QCOMPARE(seedSpy.count(), 1);
    settings.setShuffleSeed(42u);
    QCOMPARE(seedSpy.count(), 1);

    settings.setWaveformHeight(120);
    settings.setWaveformHeight(130);
    settings.setWaveformHeight(140);
    QTest::qWait(220);

    QSettings persisted(QStringLiteral("WaveFlux"), QStringLiteral("WaveFlux"));
    persisted.beginGroup(QStringLiteral("App"));
    QCOMPARE(persisted.value(QStringLiteral("waveformHeight")).toInt(), 140);
    QCOMPARE(static_cast<quint32>(persisted.value(QStringLiteral("shuffleSeed")).toULongLong()), 42u);
    persisted.endGroup();
}

QTEST_MAIN(AppSettingsManagerTest)
#include "tst_AppSettingsManager.moc"
