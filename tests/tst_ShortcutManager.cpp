#include <QtTest>

#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <QSignalSpy>

#include "ShortcutManager.h"

namespace {
constexpr int kExpectedShortcutsSettingsVersion = 1;

void clearSettings()
{
    QSettings settings(QStringLiteral("WaveFlux"), QStringLiteral("WaveFlux"));
    settings.clear();
    settings.sync();
}

void writeShortcutOverrides(const QVariantMap &overrides, bool writeVersion = true)
{
    QSettings settings(QStringLiteral("WaveFlux"), QStringLiteral("WaveFlux"));
    settings.beginGroup(QStringLiteral("App"));
    if (writeVersion) {
        settings.setValue(QStringLiteral("shortcuts.version"), kExpectedShortcutsSettingsVersion);
    }
    settings.setValue(QStringLiteral("shortcuts.overrides"), overrides);
    settings.endGroup();
    settings.sync();
}

QVariantMap readShortcutOverrides()
{
    QSettings settings(QStringLiteral("WaveFlux"), QStringLiteral("WaveFlux"));
    settings.beginGroup(QStringLiteral("App"));
    const QVariantMap overrides =
        settings.value(QStringLiteral("shortcuts.overrides"), QVariantMap()).toMap();
    settings.endGroup();
    return overrides;
}
} // namespace

class ShortcutManagerTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void exposesDefinitionsAndDefaults();
    void normalizesAndPersistsOverrides();
    void resetShortcutRestoresDefault();
    void clearOptionalShortcutDisablesIt();
    void rejectsInvalidSequencesWithoutChangingExistingOverride();
    void enforcesEmptySequenceRules();
    void rejectsUnknownShortcutIds();
    void rejectsNonAssignableShortcutChanges();
    void tapHoldShortcutAllowsOnlySingleKeySequences();
    void detectsConflictsByContext();
    void reportsNonReplaceableReservedConflicts();
    void replacesConflictingAssignableShortcuts();
    void resetGroupAndAllRemoveOverrides();
    void missingVersionIgnoresStoredOverrides();
    void invalidStoredOverridesAreIgnoredAndDroppedOnNextSave();
    void storedOverridesSurviveDefaultLookup();
};

void ShortcutManagerTest::initTestCase()
{
    const QString settingsDir =
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("shortcut_test_settings"));
    QDir().mkpath(settingsDir);
    qputenv("XDG_CONFIG_HOME", settingsDir.toUtf8());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, settingsDir);
    QSettings::setPath(QSettings::NativeFormat, QSettings::SystemScope, settingsDir);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir);
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, settingsDir);
    clearSettings();
}

void ShortcutManagerTest::init()
{
    clearSettings();
}

void ShortcutManagerTest::cleanup()
{
    clearSettings();
}

void ShortcutManagerTest::exposesDefinitionsAndDefaults()
{
    ShortcutManager manager;

    QVERIFY(manager.hasShortcut(QStringLiteral("file.openFiles")));
    QVERIFY(manager.hasShortcut(QStringLiteral("playback.playPause")));
    QVERIFY(manager.hasShortcut(QStringLiteral("playback.spaceTapHold")));
    QVERIFY(manager.hasShortcut(QStringLiteral("dialog.audioConverter.close")));
    QCOMPARE(manager.defaultSequence(QStringLiteral("file.openFiles")), QStringLiteral("Ctrl+O"));
    QCOMPARE(manager.defaultSequence(QStringLiteral("playback.spaceTapHold")), QStringLiteral("Space"));
    QCOMPARE(manager.defaultSequence(QStringLiteral("playback.toggleMute")), QStringLiteral("M"));
    QCOMPARE(manager.defaultSequence(QStringLiteral("playlist.scrollToBeginning")), QStringLiteral("Home"));
    QCOMPARE(manager.defaultSequence(QStringLiteral("playlist.scrollToEnd")), QStringLiteral("End"));
    QCOMPARE(manager.defaultSequence(QStringLiteral("playlist.pageUp")), QStringLiteral("PgUp"));
    QCOMPARE(manager.defaultSequence(QStringLiteral("playlist.pageDown")), QStringLiteral("PgDown"));
    QCOMPARE(manager.effectiveSequence(QStringLiteral("file.openFiles")), QStringLiteral("Ctrl+O"));
    QCOMPARE(manager.shortcutDefinitions().size(), ShortcutRegistry::definitions().size());
    QCOMPARE(manager.shortcutRows().size(), ShortcutRegistry::definitions().size());

    const QVariantMap validation =
        manager.validateSequence(QStringLiteral("file.openFiles"), QStringLiteral("Ctrl+Alt+O"));
    QVERIFY(validation.value(QStringLiteral("ok")).toBool());
    QCOMPARE(validation.value(QStringLiteral("normalizedSequence")).toString(), QStringLiteral("Ctrl+Alt+O"));
}

void ShortcutManagerTest::normalizesAndPersistsOverrides()
{
    {
        ShortcutManager manager;
        QSignalSpy changedSpy(&manager, &ShortcutManager::shortcutsChanged);
        QVERIFY(manager.setCustomSequence(QStringLiteral("file.openFiles"), QStringLiteral("Ctrl+Alt+O")));
        QCOMPARE(changedSpy.count(), 1);
        QCOMPARE(manager.effectiveSequence(QStringLiteral("file.openFiles")), QStringLiteral("Ctrl+Alt+O"));
        QVERIFY(manager.customSequence(QStringLiteral("file.openFiles")) == QStringLiteral("Ctrl+Alt+O"));
    }

    ShortcutManager reloaded;
    QCOMPARE(reloaded.effectiveSequence(QStringLiteral("file.openFiles")), QStringLiteral("Ctrl+Alt+O"));
    QCOMPARE(reloaded.customSequence(QStringLiteral("file.openFiles")), QStringLiteral("Ctrl+Alt+O"));
}

void ShortcutManagerTest::resetShortcutRestoresDefault()
{
    ShortcutManager manager;
    QVERIFY(manager.setCustomSequence(QStringLiteral("file.openFiles"), QStringLiteral("Ctrl+Alt+O")));
    QVERIFY(manager.resetShortcut(QStringLiteral("file.openFiles")));
    QCOMPARE(manager.effectiveSequence(QStringLiteral("file.openFiles")), QStringLiteral("Ctrl+O"));
    QVERIFY(manager.customSequence(QStringLiteral("file.openFiles")).isEmpty());
}

void ShortcutManagerTest::clearOptionalShortcutDisablesIt()
{
    ShortcutManager manager;
    QVERIFY(manager.clearCustomSequence(QStringLiteral("file.openFiles")));
    QVERIFY(!manager.shortcutEnabled(QStringLiteral("file.openFiles")));
    QVERIFY(manager.effectiveSequence(QStringLiteral("file.openFiles")).isEmpty());

    ShortcutManager reloaded;
    QVERIFY(!reloaded.shortcutEnabled(QStringLiteral("file.openFiles")));
}

void ShortcutManagerTest::rejectsInvalidSequencesWithoutChangingExistingOverride()
{
    ShortcutManager manager;
    QVERIFY(manager.setCustomSequence(QStringLiteral("file.openFiles"), QStringLiteral("Ctrl+Alt+O")));

    const QVariantMap validation =
        manager.validateSequence(QStringLiteral("file.openFiles"), QStringLiteral("not a shortcut"));
    QVERIFY(!validation.value(QStringLiteral("ok")).toBool());
    QCOMPARE(validation.value(QStringLiteral("reason")).toString(), QStringLiteral("invalid-sequence"));

    QVERIFY(!manager.setCustomSequence(QStringLiteral("file.openFiles"), QStringLiteral("not a shortcut")));
    QCOMPARE(manager.lastError(), QStringLiteral("invalid-sequence"));
    QCOMPARE(manager.effectiveSequence(QStringLiteral("file.openFiles")), QStringLiteral("Ctrl+Alt+O"));

    ShortcutManager reloaded;
    QCOMPARE(reloaded.effectiveSequence(QStringLiteral("file.openFiles")), QStringLiteral("Ctrl+Alt+O"));
}

void ShortcutManagerTest::enforcesEmptySequenceRules()
{
    ShortcutManager manager;

    QVariantMap validation = manager.validateSequence(QStringLiteral("file.openFiles"), QString());
    QVERIFY(validation.value(QStringLiteral("ok")).toBool());
    QCOMPARE(validation.value(QStringLiteral("normalizedSequence")).toString(), QString());
    QVERIFY(manager.clearCustomSequence(QStringLiteral("file.openFiles")));
    QVERIFY(!manager.shortcutEnabled(QStringLiteral("file.openFiles")));

    validation = manager.validateSequence(QStringLiteral("playback.spaceTapHold"), QString());
    QVERIFY(!validation.value(QStringLiteral("ok")).toBool());
    QCOMPARE(validation.value(QStringLiteral("reason")).toString(), QStringLiteral("empty-not-allowed"));
    QVERIFY(!manager.clearCustomSequence(QStringLiteral("playback.spaceTapHold")));
    QCOMPARE(manager.lastError(), QStringLiteral("empty-not-allowed"));
    QCOMPARE(manager.effectiveSequence(QStringLiteral("playback.spaceTapHold")), QStringLiteral("Space"));
}

void ShortcutManagerTest::rejectsUnknownShortcutIds()
{
    ShortcutManager manager;
    const QString unknownId = QStringLiteral("missing.shortcut");

    QVERIFY(!manager.hasShortcut(unknownId));
    QVERIFY(manager.defaultSequence(unknownId).isEmpty());
    QVERIFY(manager.effectiveSequence(unknownId).isEmpty());
    QVERIFY(!manager.shortcutEnabled(unknownId));

    QVariantMap validation = manager.validateSequence(unknownId, QStringLiteral("Ctrl+Alt+M"));
    QVERIFY(!validation.value(QStringLiteral("ok")).toBool());
    QCOMPARE(validation.value(QStringLiteral("reason")).toString(), QStringLiteral("unknown-id"));

    QVERIFY(!manager.setCustomSequence(unknownId, QStringLiteral("Ctrl+Alt+M")));
    QCOMPARE(manager.lastError(), QStringLiteral("unknown-id"));
    QVERIFY(!manager.clearCustomSequence(unknownId));
    QCOMPARE(manager.lastError(), QStringLiteral("unknown-id"));
    QVERIFY(!manager.resetShortcut(unknownId));
    QCOMPARE(manager.lastError(), QStringLiteral("unknown-id"));

    const QVariantMap report = manager.conflictReportForSequence(unknownId, QStringLiteral("Ctrl+Alt+M"));
    QVERIFY(!report.value(QStringLiteral("ok")).toBool());
    QCOMPARE(report.value(QStringLiteral("reason")).toString(), QStringLiteral("unknown-id"));

    const QVariantMap result =
        manager.setCustomSequenceResolvingConflicts(unknownId, QStringLiteral("Ctrl+Alt+M"), true);
    QVERIFY(!result.value(QStringLiteral("ok")).toBool());
    QCOMPARE(result.value(QStringLiteral("reason")).toString(), QStringLiteral("unknown-id"));
}

void ShortcutManagerTest::rejectsNonAssignableShortcutChanges()
{
    ShortcutManager manager;
    QVERIFY(!manager.setCustomSequence(QStringLiteral("dialog.audioConverter.close"),
                                       QStringLiteral("Ctrl+Escape")));
    QCOMPARE(manager.lastError(), QStringLiteral("not-assignable"));
    QCOMPARE(manager.effectiveSequence(QStringLiteral("dialog.audioConverter.close")), QStringLiteral("Escape"));

    QVERIFY(!manager.clearCustomSequence(QStringLiteral("playback.spaceTapHold")));
    QCOMPARE(manager.lastError(), QStringLiteral("empty-not-allowed"));
}

void ShortcutManagerTest::tapHoldShortcutAllowsOnlySingleKeySequences()
{
    ShortcutManager manager;

    QVERIFY(manager.setCustomSequence(QStringLiteral("playback.spaceTapHold"), QStringLiteral("K")));
    QCOMPARE(manager.effectiveSequence(QStringLiteral("playback.spaceTapHold")), QStringLiteral("K"));

    QVERIFY(manager.setCustomSequence(QStringLiteral("playback.spaceTapHold"), QStringLiteral("Ctrl+Alt+K")));
    QCOMPARE(manager.effectiveSequence(QStringLiteral("playback.spaceTapHold")), QStringLiteral("Ctrl+Alt+K"));

    QVERIFY(!manager.setCustomSequence(QStringLiteral("playback.spaceTapHold"),
                                       QStringLiteral("Ctrl+K, Ctrl+C")));
    QCOMPARE(manager.lastError(), QStringLiteral("invalid-sequence"));
    QCOMPARE(manager.effectiveSequence(QStringLiteral("playback.spaceTapHold")), QStringLiteral("Ctrl+Alt+K"));
}

void ShortcutManagerTest::detectsConflictsByContext()
{
    ShortcutManager manager;

    QVariantList conflicts =
        manager.conflictsForSequence(QStringLiteral("file.openFiles"), QStringLiteral("Ctrl+Shift+O"));
    QCOMPARE(conflicts.size(), 1);
    QCOMPARE(conflicts.first().toMap().value(QStringLiteral("id")).toString(),
             QStringLiteral("file.addFolder"));

    conflicts = manager.conflictsForSequence(QStringLiteral("playback.seekBackward"), QStringLiteral("Left"));
    bool hasCompactConflict = false;
    for (const QVariant &conflict : conflicts) {
        if (conflict.toMap().value(QStringLiteral("id")).toString() == QStringLiteral("compact.seekBackward")) {
            hasCompactConflict = true;
        }
    }
    QVERIFY(!hasCompactConflict);

    conflicts = manager.conflictsForSequence(QStringLiteral("file.openFiles"), QStringLiteral("Escape"));
    bool hasDialogConflict = false;
    for (const QVariant &conflict : conflicts) {
        if (conflict.toMap().value(QStringLiteral("context")).toString() == QStringLiteral("dialog")) {
            hasDialogConflict = true;
        }
    }
    QVERIFY(!hasDialogConflict);
}

void ShortcutManagerTest::reportsNonReplaceableReservedConflicts()
{
    ShortcutManager manager;

    const QVariantMap report =
        manager.conflictReportForSequence(QStringLiteral("file.openFiles"), QStringLiteral("Escape"));
    QVERIFY(report.value(QStringLiteral("ok")).toBool());
    QVERIFY(report.value(QStringLiteral("hasConflicts")).toBool());
    QVERIFY(!report.value(QStringLiteral("canReplaceAll")).toBool());

    const QVariantList conflicts = report.value(QStringLiteral("conflicts")).toList();
    bool foundFullscreenEscape = false;
    bool foundDialogEscape = false;
    for (const QVariant &conflictVariant : conflicts) {
        const QVariantMap conflict = conflictVariant.toMap();
        if (conflict.value(QStringLiteral("id")).toString() == QStringLiteral("view.exitFullscreen")) {
            foundFullscreenEscape = true;
            QCOMPARE(conflict.value(QStringLiteral("canReplace")).toBool(), false);
            QCOMPARE(conflict.value(QStringLiteral("blockingReason")).toString(),
                     QStringLiteral("not-assignable"));
        }
        if (conflict.value(QStringLiteral("context")).toString() == QStringLiteral("dialog")) {
            foundDialogEscape = true;
        }
    }
    QVERIFY(foundFullscreenEscape);
    QVERIFY(!foundDialogEscape);

    const QVariantMap result =
        manager.setCustomSequenceResolvingConflicts(QStringLiteral("file.openFiles"),
                                                    QStringLiteral("Escape"),
                                                    true);
    QVERIFY(!result.value(QStringLiteral("ok")).toBool());
    QCOMPARE(result.value(QStringLiteral("reason")).toString(),
             QStringLiteral("non-replaceable-conflict"));
}

void ShortcutManagerTest::replacesConflictingAssignableShortcuts()
{
    ShortcutManager manager;

    const QVariantMap blocked =
        manager.setCustomSequenceResolvingConflicts(QStringLiteral("file.openFiles"),
                                                    QStringLiteral("Ctrl+Shift+O"),
                                                    false);
    QVERIFY(!blocked.value(QStringLiteral("ok")).toBool());
    QCOMPARE(blocked.value(QStringLiteral("reason")).toString(), QStringLiteral("conflict"));
    QCOMPARE(manager.effectiveSequence(QStringLiteral("file.openFiles")), QStringLiteral("Ctrl+O"));
    QCOMPARE(manager.effectiveSequence(QStringLiteral("file.addFolder")), QStringLiteral("Ctrl+Shift+O"));

    const QVariantMap replaced =
        manager.setCustomSequenceResolvingConflicts(QStringLiteral("file.openFiles"),
                                                    QStringLiteral("Ctrl+Shift+O"),
                                                    true);
    QVERIFY(replaced.value(QStringLiteral("ok")).toBool());
    QCOMPARE(replaced.value(QStringLiteral("replacedCount")).toInt(), 1);
    QCOMPARE(manager.effectiveSequence(QStringLiteral("file.openFiles")),
             QStringLiteral("Ctrl+Shift+O"));
    QVERIFY(manager.effectiveSequence(QStringLiteral("file.addFolder")).isEmpty());
    QVERIFY(!manager.shortcutEnabled(QStringLiteral("file.addFolder")));
}

void ShortcutManagerTest::resetGroupAndAllRemoveOverrides()
{
    ShortcutManager manager;
    QVERIFY(manager.setCustomSequence(QStringLiteral("file.openFiles"), QStringLiteral("Ctrl+Alt+O")));
    QVERIFY(manager.setCustomSequence(QStringLiteral("playback.repeatOff"), QStringLiteral("Alt+1")));

    QVERIFY(manager.resetGroup(QStringLiteral("playback")));
    QCOMPARE(manager.effectiveSequence(QStringLiteral("playback.repeatOff")), QStringLiteral("Ctrl+1"));
    QCOMPARE(manager.effectiveSequence(QStringLiteral("file.openFiles")), QStringLiteral("Ctrl+Alt+O"));

    QVERIFY(manager.resetAll());
    QCOMPARE(manager.effectiveSequence(QStringLiteral("file.openFiles")), QStringLiteral("Ctrl+O"));
}

void ShortcutManagerTest::missingVersionIgnoresStoredOverrides()
{
    QVariantMap overrides;
    overrides.insert(QStringLiteral("file.openFiles"), QStringLiteral("Ctrl+Alt+O"));
    writeShortcutOverrides(overrides, false);

    ShortcutManager manager;
    QCOMPARE(manager.effectiveSequence(QStringLiteral("file.openFiles")), QStringLiteral("Ctrl+O"));
    QVERIFY(manager.customSequence(QStringLiteral("file.openFiles")).isEmpty());
}

void ShortcutManagerTest::invalidStoredOverridesAreIgnoredAndDroppedOnNextSave()
{
    QVariantMap overrides;
    overrides.insert(QStringLiteral("file.openFiles"), QStringLiteral("Ctrl+Alt+O"));
    overrides.insert(QStringLiteral("unknown.shortcut"), QStringLiteral("Ctrl+Alt+U"));
    overrides.insert(QStringLiteral("file.addFolder"), QStringLiteral("not a shortcut"));
    overrides.insert(QStringLiteral("dialog.audioConverter.close"), QStringLiteral("Ctrl+Escape"));
    overrides.insert(QStringLiteral("playback.spaceTapHold"), QStringLiteral("Ctrl+K, Ctrl+C"));
    writeShortcutOverrides(overrides);

    ShortcutManager manager;
    QCOMPARE(manager.effectiveSequence(QStringLiteral("file.openFiles")), QStringLiteral("Ctrl+Alt+O"));
    QCOMPARE(manager.effectiveSequence(QStringLiteral("file.addFolder")), QStringLiteral("Ctrl+Shift+O"));
    QCOMPARE(manager.effectiveSequence(QStringLiteral("dialog.audioConverter.close")), QStringLiteral("Escape"));
    QCOMPARE(manager.effectiveSequence(QStringLiteral("playback.spaceTapHold")), QStringLiteral("Space"));

    QVERIFY(manager.setCustomSequence(QStringLiteral("playback.repeatOff"), QStringLiteral("Alt+1")));

    const QVariantMap persisted = readShortcutOverrides();
    QCOMPARE(persisted.value(QStringLiteral("file.openFiles")).toString(), QStringLiteral("Ctrl+Alt+O"));
    QCOMPARE(persisted.value(QStringLiteral("playback.repeatOff")).toString(), QStringLiteral("Alt+1"));
    QVERIFY(!persisted.contains(QStringLiteral("unknown.shortcut")));
    QVERIFY(!persisted.contains(QStringLiteral("file.addFolder")));
    QVERIFY(!persisted.contains(QStringLiteral("dialog.audioConverter.close")));
    QVERIFY(!persisted.contains(QStringLiteral("playback.spaceTapHold")));
}

void ShortcutManagerTest::storedOverridesSurviveDefaultLookup()
{
    QVariantMap overrides;
    overrides.insert(QStringLiteral("file.openFiles"), QStringLiteral("Ctrl+Alt+O"));
    writeShortcutOverrides(overrides);

    ShortcutManager manager;
    QCOMPARE(manager.defaultSequence(QStringLiteral("file.openFiles")), QStringLiteral("Ctrl+O"));
    QCOMPARE(manager.effectiveSequence(QStringLiteral("file.openFiles")), QStringLiteral("Ctrl+Alt+O"));
    QCOMPARE(manager.effectiveSequence(QStringLiteral("file.addFolder")), QStringLiteral("Ctrl+Shift+O"));
}

QTEST_MAIN(ShortcutManagerTest)
#include "tst_ShortcutManager.moc"
