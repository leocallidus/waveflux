#include <QtTest>
#include <QSet>
#include <QStringList>
#include <QVariantMap>

#include "SettingsRegistry.h"
#include "AppSettingsManager.h"

class SettingsRegistryTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testCategoriesIntegrity();
    void testGroupsIntegrity();
    void testSettingsIntegrity();
    void testLegacySectionMapping();
    void testSearchRankingEnglish();
    void testSearchRankingRussian();
    void testSearchEmptyAndNoMatch();
};

void SettingsRegistryTest::initTestCase()
{
}

void SettingsRegistryTest::testCategoriesIntegrity()
{
    auto registry = SettingsRegistry::instance();
    QVERIFY(registry);

    const auto &categories = registry->categoryDescriptors();
    QCOMPARE(categories.size(), 9);

    const QStringList expectedIds = {
        QStringLiteral("general"),
        QStringLiteral("appearance"),
        QStringLiteral("playlist"),
        QStringLiteral("playback"),
        QStringLiteral("waveform"),
        QStringLiteral("trackInfo"),
        QStringLiteral("system"),
        QStringLiteral("shortcuts"),
        QStringLiteral("advanced")
    };

    for (int i = 0; i < expectedIds.size(); ++i) {
        QCOMPARE(categories[i].id, expectedIds[i]);
        QCOMPARE(categories[i].order, i);
        QVERIFY(!categories[i].titleKey.isEmpty());
        QVERIFY(!categories[i].descriptionKey.isEmpty());
        QVERIFY(!categories[i].iconName.isEmpty());
        QVERIFY(!categories[i].pageComponentUrl.isEmpty());

        const auto cat = registry->category(expectedIds[i]);
        QCOMPARE(cat.value(QStringLiteral("id")).toString(), expectedIds[i]);
    }
}

void SettingsRegistryTest::testGroupsIntegrity()
{
    auto registry = SettingsRegistry::instance();
    QVERIFY(registry);

    const auto &groups = registry->groupDescriptors();
    QVERIFY(groups.size() >= 29);

    QSet<QString> groupIds;
    for (const auto &group : groups) {
        QVERIFY(!groupIds.contains(group.id));
        groupIds.insert(group.id);

        QVERIFY(!group.categoryId.isEmpty());
        QVERIFY(!group.titleKey.isEmpty());
        QVERIFY(!registry->category(group.categoryId).isEmpty());

        const auto found = registry->group(group.id);
        QCOMPARE(found.value(QStringLiteral("id")).toString(), group.id);
    }
}

void SettingsRegistryTest::testSettingsIntegrity()
{
    auto registry = SettingsRegistry::instance();
    QVERIFY(registry);

    const auto &settings = registry->settingDescriptors();
    QCOMPARE(settings.size(), 71);

    QSet<QString> settingIds;
    for (const auto &setting : settings) {
        QVERIFY(!settingIds.contains(setting.id));
        settingIds.insert(setting.id);

        QVERIFY(!setting.categoryId.isEmpty());
        QVERIFY(!setting.groupId.isEmpty());
        QVERIFY(!setting.titleKey.isEmpty());
        QVERIFY(!setting.controlKind.isEmpty());

        QVERIFY(!registry->category(setting.categoryId).isEmpty());
        QVERIFY(!registry->group(setting.groupId).isEmpty());

        const auto found = registry->setting(setting.id);
        QCOMPARE(found.value(QStringLiteral("id")).toString(), setting.id);
    }

    const QStringList integrityErrors = registry->validateIntegrity();
    QVERIFY2(integrityErrors.isEmpty(), qPrintable(integrityErrors.join(QLatin1Char('\n'))));
}

void SettingsRegistryTest::testLegacySectionMapping()
{
    auto registry = SettingsRegistry::instance();
    QVERIFY(registry);

    QCOMPARE(registry->mapLegacySectionId(QStringLiteral("appearance")), QStringLiteral("appearance"));
    QCOMPARE(registry->mapLegacySectionId(QStringLiteral("theme")), QStringLiteral("appearance"));
    QCOMPARE(registry->mapLegacySectionId(QStringLiteral("colors")), QStringLiteral("appearance"));
    QCOMPARE(registry->mapLegacySectionId(QStringLiteral("audio")), QStringLiteral("playback"));
    QCOMPARE(registry->mapLegacySectionId(QStringLiteral("playback")), QStringLiteral("playback"));
    QCOMPARE(registry->mapLegacySectionId(QStringLiteral("waveform")), QStringLiteral("waveform"));
    QCOMPARE(registry->mapLegacySectionId(QStringLiteral("trackInfo")), QStringLiteral("trackInfo"));
    QCOMPARE(registry->mapLegacySectionId(QStringLiteral("system")), QStringLiteral("system"));
    QCOMPARE(registry->mapLegacySectionId(QStringLiteral("shortcuts")), QStringLiteral("shortcuts"));
    QCOMPARE(registry->mapLegacySectionId(QStringLiteral("advanced")), QStringLiteral("advanced"));
    QCOMPARE(registry->mapLegacySectionId(QStringLiteral("unknown-anything")), QStringLiteral("general"));
}

void SettingsRegistryTest::testSearchRankingEnglish()
{
    auto registry = SettingsRegistry::instance();
    QVERIFY(registry);

    // Exact title / prefix match should rank first
    const auto resultsLanguage = registry->search(QStringLiteral("Language"), QStringLiteral("en"));
    QVERIFY(!resultsLanguage.isEmpty());
    QCOMPARE(resultsLanguage[0].toMap().value(QStringLiteral("id")).toString(), QStringLiteral("general.language"));

    const auto resultsVolume = registry->search(QStringLiteral("decibels"), QStringLiteral("en"));
    QVERIFY(!resultsVolume.isEmpty());
    QCOMPARE(resultsVolume[0].toMap().value(QStringLiteral("id")).toString(), QStringLiteral("playback.presentation.volumeDecibels"));

    const auto resultsShuffle = registry->search(QStringLiteral("shuffle"), QStringLiteral("en"));
    QVERIFY(resultsShuffle.size() >= 3);
    bool hasDeterministicShuffle = false;
    for (const auto &res : resultsShuffle) {
        if (res.toMap().value(QStringLiteral("id")).toString() == QStringLiteral("playback.shuffle.deterministic")) {
            hasDeterministicShuffle = true;
        }
    }
    QVERIFY(hasDeterministicShuffle);
}

void SettingsRegistryTest::testSearchRankingRussian()
{
    auto registry = SettingsRegistry::instance();
    QVERIFY(registry);

    // Search in Russian
    const auto resultsLangRu = registry->search(QStringLiteral("Язык"), QStringLiteral("ru"));
    QVERIFY(!resultsLangRu.isEmpty());
    QCOMPARE(resultsLangRu[0].toMap().value(QStringLiteral("id")).toString(), QStringLiteral("general.language"));

    const auto resultsVolumeRu = registry->search(QStringLiteral("децибел"), QStringLiteral("ru"));
    QVERIFY(!resultsVolumeRu.isEmpty());
    QCOMPARE(resultsVolumeRu[0].toMap().value(QStringLiteral("id")).toString(), QStringLiteral("playback.presentation.volumeDecibels"));
}

void SettingsRegistryTest::testSearchEmptyAndNoMatch()
{
    auto registry = SettingsRegistry::instance();
    QVERIFY(registry);

    const auto emptyResults = registry->search(QStringLiteral(""), QStringLiteral("en"));
    QVERIFY(emptyResults.isEmpty());

    const auto noMatchResults = registry->search(QStringLiteral("xyznonexistentterm999"), QStringLiteral("en"));
    QVERIFY(noMatchResults.isEmpty());
}

QTEST_MAIN(SettingsRegistryTest)
#include "tst_SettingsRegistry.moc"
