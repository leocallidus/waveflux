#include <QtTest>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QSet>
#include <QVariantMap>

#include "EqualizerPresetManager.h"

namespace {
constexpr double kEpsilon = 0.001;

QVariantList flatGains(double value)
{
    QVariantList values;
    values.reserve(10);
    for (int i = 0; i < 10; ++i) {
        values.push_back(value);
    }
    return values;
}

QJsonArray gainsToJsonArray(const QVariantList &gains)
{
    QJsonArray array;
    for (const QVariant &value : gains) {
        array.push_back(value.toDouble());
    }
    return array;
}

QString buildBundleJson(const QJsonArray &presets,
                        const QString &schema = QStringLiteral("waveflux.eq.presets.v1"),
                        int bandCount = 10,
                        bool includeBandCount = true)
{
    QJsonObject root;
    root.insert(QStringLiteral("schema"), schema);
    if (includeBandCount) {
        root.insert(QStringLiteral("bandCount"), bandCount);
    }
    root.insert(QStringLiteral("presets"), presets);
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

QJsonObject makePresetObject(const QString &name, const QVariantList &gains, const QString &id = QString())
{
    QJsonObject preset;
    if (!id.trimmed().isEmpty()) {
        preset.insert(QStringLiteral("id"), id.trimmed());
    }
    preset.insert(QStringLiteral("name"), name);
    preset.insert(QStringLiteral("gains"), gainsToJsonArray(gains));
    return preset;
}

void compareGains(const QVariantList &actual, const QVariantList &expected)
{
    QCOMPARE(actual.size(), expected.size());
    for (int i = 0; i < actual.size(); ++i) {
        const double a = actual.at(i).toDouble();
        const double e = expected.at(i).toDouble();
        QVERIFY2(qAbs(a - e) <= kEpsilon,
                 qPrintable(QStringLiteral("Gain mismatch at index %1: actual=%2 expected=%3")
                                .arg(i)
                                .arg(a, 0, 'f', 3)
                                .arg(e, 0, 'f', 3)));
    }
}

QMap<QString, QVariantList> presetMapByName(const QVariantList &presets)
{
    QMap<QString, QVariantList> result;
    for (const QVariant &value : presets) {
        const QVariantMap preset = value.toMap();
        result.insert(preset.value(QStringLiteral("name")).toString(),
                      preset.value(QStringLiteral("gains")).toList());
    }
    return result;
}
} // namespace

class TestEqualizerPresetManager : public QObject
{
    Q_OBJECT

private slots:
    void normalizeClamp_onCreateUserPreset();
    void normalizeClamp_onImport();
    void importSchemaValidation_data();
    void importSchemaValidation();
    void conflictResolution_keepBoth();
    void conflictResolution_replaceExisting();
    void exportImportRoundtrip();
    void builtInPresets_catalogIntegrity();
};

void TestEqualizerPresetManager::normalizeClamp_onCreateUserPreset()
{
    EqualizerPresetManager manager;

    const QVariantList input = {
        -100.0,
        -24.0,
        -23.94,
        -5.05,
        0.0,
        11.95,
        12.0,
        15.0,
        QStringLiteral("not-a-number"),
        1.234,
        9.99,
        -50.0
    };
    const QString presetId = manager.createUserPreset(QStringLiteral("Clamp Test"), input);
    QVERIFY(!presetId.isEmpty());

    const QVariantMap preset = manager.getPreset(presetId);
    QVERIFY(!preset.isEmpty());

    const QVariantList expected = {
        -24.0,
        -24.0,
        -23.9,
        -5.1,
        0.0,
        12.0,
        12.0,
        12.0,
        0.0,
        1.2
    };
    compareGains(preset.value(QStringLiteral("gains")).toList(), expected);
}

void TestEqualizerPresetManager::normalizeClamp_onImport()
{
    EqualizerPresetManager manager;

    QJsonArray presets;
    presets.push_back(makePresetObject(QStringLiteral("Imported Clamp"),
                                       {30.0, -30.0, 1.26, 0.04, -0.04}));

    const QVariantMap result = manager.importPresetsFromJson(buildBundleJson(presets));
    QVERIFY(result.value(QStringLiteral("success")).toBool());
    QCOMPARE(result.value(QStringLiteral("importedCount")).toInt(), 1);

    const QVariantList users = manager.listUserPresets();
    QCOMPARE(users.size(), 1);
    const QVariantList gains = users.constFirst().toMap().value(QStringLiteral("gains")).toList();
    const QVariantList expected = {12.0, -24.0, 1.3, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    compareGains(gains, expected);
}

void TestEqualizerPresetManager::importSchemaValidation_data()
{
    QTest::addColumn<QString>("payload");
    QTest::addColumn<QString>("expectedErrorContains");

    QTest::newRow("empty") << QString() << QStringLiteral("empty");
    QTest::newRow("invalid-json") << QStringLiteral("{\"schema\":") << QStringLiteral("parse error");
    QTest::newRow("invalid-root") << QStringLiteral("[1,2,3]") << QStringLiteral("root");
    QTest::newRow("unsupported-schema")
        << buildBundleJson(QJsonArray(), QStringLiteral("waveflux.eq.presets.v999"))
        << QStringLiteral("schema");
    QTest::newRow("unsupported-band-count")
        << buildBundleJson(QJsonArray(), QStringLiteral("waveflux.eq.presets.v1"), 9, true)
        << QStringLiteral("bandcount");
    QTest::newRow("missing-presets-array")
        << QStringLiteral("{\"schema\":\"waveflux.eq.presets.v1\",\"bandCount\":10}")
        << QStringLiteral("presets");
}

void TestEqualizerPresetManager::importSchemaValidation()
{
    QFETCH(QString, payload);
    QFETCH(QString, expectedErrorContains);

    EqualizerPresetManager manager;
    const QVariantMap result = manager.importPresetsFromJson(payload);

    QVERIFY(!result.value(QStringLiteral("success")).toBool());
    const QStringList errors = result.value(QStringLiteral("errors")).toStringList();
    QVERIFY(!errors.isEmpty());

    const QString haystack = (errors.join(QLatin1Char(' ')) + QLatin1Char(' ') + manager.lastError()).toLower();
    QVERIFY2(haystack.contains(expectedErrorContains.toLower()),
             qPrintable(QStringLiteral("Expected error fragment '%1' was not found in '%2'")
                            .arg(expectedErrorContains, haystack)));
}

void TestEqualizerPresetManager::conflictResolution_keepBoth()
{
    EqualizerPresetManager manager;
    QVERIFY(!manager.createUserPreset(QStringLiteral("Rock"), flatGains(1.0)).isEmpty());

    QJsonArray presets;
    presets.push_back(makePresetObject(QStringLiteral("Rock"), flatGains(2.0), QStringLiteral("user:777")));
    const QVariantMap result = manager.importPresetsFromJson(buildBundleJson(presets), QStringLiteral("keep_both"));

    QVERIFY(result.value(QStringLiteral("success")).toBool());
    QCOMPARE(result.value(QStringLiteral("importedCount")).toInt(), 1);
    QCOMPARE(result.value(QStringLiteral("replacedCount")).toInt(), 0);

    const QVariantList users = manager.listUserPresets();
    QCOMPARE(users.size(), 2);

    int countOriginal = 0;
    int countImported = 0;
    for (const QVariant &value : users) {
        const QVariantList gains = value.toMap().value(QStringLiteral("gains")).toList();
        if (qAbs(gains.constFirst().toDouble() - 1.0) <= kEpsilon) {
            ++countOriginal;
        } else if (qAbs(gains.constFirst().toDouble() - 2.0) <= kEpsilon) {
            ++countImported;
        }
    }
    QCOMPARE(countOriginal, 1);
    QCOMPARE(countImported, 1);
}

void TestEqualizerPresetManager::conflictResolution_replaceExisting()
{
    EqualizerPresetManager manager;
    const QString existingId = manager.createUserPreset(QStringLiteral("Rock"), flatGains(1.0));
    QVERIFY(!existingId.isEmpty());

    QJsonArray presets;
    presets.push_back(makePresetObject(QStringLiteral("Rock"), flatGains(2.0), QStringLiteral("user:777")));
    const QVariantMap result = manager.importPresetsFromJson(buildBundleJson(presets),
                                                             QStringLiteral("replace_existing"));

    QVERIFY(result.value(QStringLiteral("success")).toBool());
    QCOMPARE(result.value(QStringLiteral("importedCount")).toInt(), 0);
    QCOMPARE(result.value(QStringLiteral("replacedCount")).toInt(), 1);
    QCOMPARE(result.value(QStringLiteral("skippedCount")).toInt(), 0);

    const QVariantList users = manager.listUserPresets();
    QCOMPARE(users.size(), 1);
    const QVariantMap onlyPreset = users.constFirst().toMap();
    QCOMPARE(onlyPreset.value(QStringLiteral("name")).toString(), QStringLiteral("Rock"));
    compareGains(onlyPreset.value(QStringLiteral("gains")).toList(), flatGains(2.0));
}

void TestEqualizerPresetManager::exportImportRoundtrip()
{
    EqualizerPresetManager source;
    QVERIFY(!source.createUserPreset(QStringLiteral("Warm"),
                                     {1.25, 0.5, -0.5, -1.5, -1.0, 0.0, 1.0, 1.5, 2.0, 2.5}).isEmpty());
    QVERIFY(!source.createUserPreset(QStringLiteral("Bright"),
                                     {-2.0, -1.0, -0.5, 0.0, 0.5, 1.5, 3.0, 4.5, 5.5, 6.0}).isEmpty());

    const QVariantMap exported = source.exportUserPresetsToJson();
    QVERIFY(exported.value(QStringLiteral("success")).toBool());
    QCOMPARE(exported.value(QStringLiteral("exportedCount")).toInt(), 2);

    EqualizerPresetManager target;
    const QVariantMap imported = target.importPresetsFromJson(exported.value(QStringLiteral("json")).toString(),
                                                              QStringLiteral("keep_both"));
    QVERIFY(imported.value(QStringLiteral("success")).toBool());
    QCOMPARE(imported.value(QStringLiteral("importedCount")).toInt(), 2);
    QCOMPARE(imported.value(QStringLiteral("replacedCount")).toInt(), 0);

    const QMap<QString, QVariantList> sourceByName = presetMapByName(source.listUserPresets());
    const QMap<QString, QVariantList> targetByName = presetMapByName(target.listUserPresets());

    QCOMPARE(targetByName.keys(), sourceByName.keys());
    for (auto it = sourceByName.constBegin(); it != sourceByName.constEnd(); ++it) {
        QVERIFY(targetByName.contains(it.key()));
        compareGains(targetByName.value(it.key()), it.value());
    }
}

void TestEqualizerPresetManager::builtInPresets_catalogIntegrity()
{
    EqualizerPresetManager manager;
    const QVariantList builtIn = manager.listBuiltInPresets();

    QCOMPARE(builtIn.size(), 9);

    const QSet<QString> expectedIds = {
        QStringLiteral("builtin:flat"),
        QStringLiteral("builtin:bass_boost"),
        QStringLiteral("builtin:vocal"),
        QStringLiteral("builtin:high_boost"),
        QStringLiteral("builtin:rock"),
        QStringLiteral("builtin:pop"),
        QStringLiteral("builtin:jazz"),
        QStringLiteral("builtin:electronic"),
        QStringLiteral("builtin:classical")
    };

    QSet<QString> actualIds;
    for (const QVariant &presetValue : builtIn) {
        const QVariantMap preset = presetValue.toMap();
        const QString id = preset.value(QStringLiteral("id")).toString();
        const QString name = preset.value(QStringLiteral("name")).toString();
        const QVariantList gains = preset.value(QStringLiteral("gains")).toList();

        QVERIFY2(id.startsWith(QStringLiteral("builtin:")), qPrintable(id));
        QVERIFY(!id.isEmpty());
        QVERIFY(!name.trimmed().isEmpty());
        QVERIFY(preset.value(QStringLiteral("builtIn")).toBool());
        QCOMPARE(gains.size(), 10);

        for (const QVariant &gain : gains) {
            const double value = gain.toDouble();
            QVERIFY2(value >= -24.0 && value <= 12.0,
                     qPrintable(QStringLiteral("gain out of range: %1").arg(value)));
        }

        QVERIFY2(!actualIds.contains(id), qPrintable(QStringLiteral("duplicate id: %1").arg(id)));
        actualIds.insert(id);
    }

    QCOMPARE(actualIds, expectedIds);
}

QTEST_MAIN(TestEqualizerPresetManager)
#include "tst_EqualizerPresetManager.moc"
