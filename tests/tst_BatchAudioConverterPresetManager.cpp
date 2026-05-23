#include <QtTest>

#include <QSignalSpy>

#include "BatchAudioConverterPresetManager.h"
#include "AppSettingsManager.h"

class BatchAudioConverterPresetManagerTest : public QObject
{
    Q_OBJECT

private slots:
    void normalizesCreatedPresets();
    void renamesDeletesAndRestoresPresets();
    void rejectsEmptyNames();
};

void BatchAudioConverterPresetManagerTest::normalizesCreatedPresets()
{
    BatchAudioConverterPresetManager manager;
    QSignalSpy presetsSpy(&manager, &BatchAudioConverterPresetManager::presetsChanged);

    QVariantMap settings;
    settings.insert(QStringLiteral("outputDirectory"), QStringLiteral("  /tmp/out  "));
    settings.insert(QStringLiteral("namingPolicy"), QStringLiteral("artist-title"));
    settings.insert(QStringLiteral("format"), QStringLiteral("wav"));
    settings.insert(QStringLiteral("conflictPolicy"), QStringLiteral("fail-on-conflict"));
    settings.insert(QStringLiteral("playlistAddMode"), QStringLiteral("deferred"));
    settings.insert(QStringLiteral("retryPolicy"), QStringLiteral("retry-failed-only"));
    settings.insert(QStringLiteral("bitrate"), -12);
    settings.insert(QStringLiteral("sampleRate"), 48000);
    settings.insert(QStringLiteral("channelMode"), QStringLiteral("mono"));
    settings.insert(QStringLiteral("playbackRate"), 99.0);
    settings.insert(QStringLiteral("pitchSemitones"), 99);
    settings.insert(QStringLiteral("applyEqualizer"), true);
    settings.insert(QStringLiteral("equalizerBandGains"),
                    QVariantList({-99.0, -3.0, 0.0, 4.5, 99.0}));
    settings.insert(QStringLiteral("addResultsToPlaylist"), false);

    const QString presetId = manager.createUserPreset(QStringLiteral("  Queue Default  "), settings);
    QVERIFY(!presetId.isEmpty());
    QCOMPARE(presetsSpy.count(), 1);

    const QVariantMap preset = manager.getPreset(presetId);
    QCOMPARE(preset.value(QStringLiteral("name")).toString(), QStringLiteral("Queue Default"));

    const QVariantMap normalizedSettings = preset.value(QStringLiteral("settings")).toMap();
    QCOMPARE(normalizedSettings.value(QStringLiteral("outputDirectory")).toString(), QStringLiteral("/tmp/out"));
    QCOMPARE(normalizedSettings.value(QStringLiteral("namingPolicy")).toString(), QStringLiteral("artist-title"));
    QCOMPARE(normalizedSettings.value(QStringLiteral("format")).toString(), QStringLiteral("wav"));
    QCOMPARE(normalizedSettings.value(QStringLiteral("conflictPolicy")).toString(), QStringLiteral("fail-on-conflict"));
    QCOMPARE(normalizedSettings.value(QStringLiteral("playlistAddMode")).toString(), QStringLiteral("deferred"));
    QCOMPARE(normalizedSettings.value(QStringLiteral("bitrate")).toInt(), 0);
    QCOMPARE(normalizedSettings.value(QStringLiteral("sampleRate")).toInt(), 48000);
    QCOMPARE(normalizedSettings.value(QStringLiteral("channelMode")).toString(), QStringLiteral("mono"));
    QCOMPARE(normalizedSettings.value(QStringLiteral("playbackRate")).toDouble(), 4.0);
    QCOMPARE(normalizedSettings.value(QStringLiteral("pitchSemitones")).toInt(), 24);
    QCOMPARE(normalizedSettings.value(QStringLiteral("applyEqualizer")).toBool(), true);
    const QVariantList equalizerBandGains =
        normalizedSettings.value(QStringLiteral("equalizerBandGains")).toList();
    QCOMPARE(equalizerBandGains.size(), 10);
    QCOMPARE(equalizerBandGains.at(0).toDouble(), -24.0);
    QCOMPARE(equalizerBandGains.at(1).toDouble(), -3.0);
    QCOMPARE(equalizerBandGains.at(3).toDouble(), 4.5);
    QCOMPARE(equalizerBandGains.at(4).toDouble(), 12.0);
    QCOMPARE(normalizedSettings.value(QStringLiteral("addResultsToPlaylist")).toBool(), true);
    QVERIFY(!normalizedSettings.contains(QStringLiteral("retryPolicy")));
}

void BatchAudioConverterPresetManagerTest::renamesDeletesAndRestoresPresets()
{
    BatchAudioConverterPresetManager source;
    const QString firstId = source.createUserPreset(QStringLiteral("Mixdown"), QVariantMap());
    const QString secondId = source.createUserPreset(QStringLiteral("Archive"), QVariantMap());
    QVERIFY(!firstId.isEmpty());
    QVERIFY(!secondId.isEmpty());

    QVERIFY(source.renameUserPreset(firstId, QStringLiteral("Mixdown Ready")));
    QCOMPARE(source.getPreset(firstId).value(QStringLiteral("name")).toString(),
             QStringLiteral("Mixdown Ready"));

    const QVariantList snapshot = source.exportUserPresetsSnapshot();
    QCOMPARE(snapshot.size(), 2);

    BatchAudioConverterPresetManager restored;
    QVERIFY(restored.replaceUserPresets(snapshot));
    QCOMPARE(restored.listUserPresets().size(), 2);
    QCOMPARE(restored.getPreset(firstId).value(QStringLiteral("name")).toString(),
             QStringLiteral("Mixdown Ready"));

    QVERIFY(restored.deleteUserPreset(secondId));
    QCOMPARE(restored.listUserPresets().size(), 1);
    QCOMPARE(restored.getPreset(secondId), QVariantMap());
}

void BatchAudioConverterPresetManagerTest::rejectsEmptyNames()
{
    BatchAudioConverterPresetManager manager;
    QVERIFY(manager.createUserPreset(QStringLiteral("   "), QVariantMap()).isEmpty());
    QCOMPARE(manager.lastError(), AppSettingsManager::translateForCurrentLanguage(QStringLiteral("error.presetNameEmpty")));
    QVERIFY(!manager.renameUserPreset(QStringLiteral("missing"), QStringLiteral("Anything")));
    QCOMPARE(manager.lastError(), AppSettingsManager::translateForCurrentLanguage(QStringLiteral("error.userPresetNotFound")));
}

QTEST_MAIN(BatchAudioConverterPresetManagerTest)
#include "tst_BatchAudioConverterPresetManager.moc"
