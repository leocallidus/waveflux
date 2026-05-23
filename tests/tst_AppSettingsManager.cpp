#include <QtTest>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimeZone>
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

QString writeTextFile(const QString &directoryPath, const QString &fileName, const QByteArray &contents)
{
    const QString filePath = QDir(directoryPath).filePath(fileName);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return QString();
    }
    file.write(contents);
    file.close();
    return filePath;
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

QString createExecutableTool(const QString &directoryPath,
                             const QString &baseName,
                             const QString &versionLine)
{
#ifdef Q_OS_WIN
    const QString filePath = QDir(directoryPath).filePath(baseName + QStringLiteral(".bat"));
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return QString();
    }
    file.write("@echo off\r\n");
    file.write(QStringLiteral("echo %1\r\n").arg(versionLine).toUtf8());
    file.write("exit /b 0\r\n");
    file.close();
    return filePath;
#else
    const QString filePath = QDir(directoryPath).filePath(baseName);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return QString();
    }
    file.write("#!/bin/sh\n");
    file.write(QStringLiteral("echo \"%1\"\n").arg(versionLine).toUtf8());
    file.close();
    QFile::setPermissions(filePath,
                          QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
                              | QFileDevice::ReadGroup | QFileDevice::ExeGroup
                              | QFileDevice::ReadOther | QFileDevice::ExeOther);
    return filePath;
#endif
}
} // namespace

class AppSettingsManagerTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void defaultsNewPlaylistFolderAutoAddToEnabled();
    void persistsAndReloadsSettings();
    void sanitizesInvalidStoredValues();
    void persistsTrackInfoSettingsAndNormalizesFormats();
    void persistsYtDlpImportHistoryAndSanitizesSecrets();
    void signalsOnlyOnEffectiveChangesAndPersistsBurstUpdates();
    void resolvesImportToolRuntimeDeterministically();
    void usesDashVersionForRealFfmpegContract();
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

void AppSettingsManagerTest::defaultsNewPlaylistFolderAutoAddToEnabled()
{
    AppSettingsManager settings;
    QCOMPARE(settings.autoAddTracksFromPlaylistFolder(), true);
    QCOMPARE(settings.autoCheckUpdates(), true);
    QCOMPARE(settings.includePrereleaseUpdates(), false);
    QCOMPARE(settings.trackInfoEnabled(), true);
    QCOMPARE(settings.trackInfoWaveformOverlayHoverOnly(), true);
    QCOMPARE(settings.trackInfoWindowTitleFormat(), settings.defaultTrackInfoWindowTitleFormat());
    QCOMPARE(settings.trackInfoWaveformTooltipFormat(), QString());
    QCOMPARE(settings.trackInfoWaveformOverlayFormats(), settings.defaultTrackInfoWaveformOverlayFormats());
    QVERIFY(!settings.lastUpdateCheckAt().isValid());
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

    QVariantMap batchLastSettings;
    batchLastSettings.insert(QStringLiteral("outputDirectory"), QStringLiteral("/tmp/batch-out"));
    batchLastSettings.insert(QStringLiteral("namingPolicy"), QStringLiteral("artist-title"));
    batchLastSettings.insert(QStringLiteral("format"), QStringLiteral("webm"));
    batchLastSettings.insert(QStringLiteral("conflictPolicy"), QStringLiteral("skip-on-conflict"));
    batchLastSettings.insert(QStringLiteral("retryPolicy"), QStringLiteral("retry-failed-only"));
    batchLastSettings.insert(QStringLiteral("playlistAddMode"), QStringLiteral("deferred"));
    batchLastSettings.insert(QStringLiteral("bitrate"), 256);
    batchLastSettings.insert(QStringLiteral("sampleRate"), 48000);
    batchLastSettings.insert(QStringLiteral("channelMode"), QStringLiteral("mono"));
    batchLastSettings.insert(QStringLiteral("playbackRate"), 1.15);
    batchLastSettings.insert(QStringLiteral("pitchSemitones"), -2);
    batchLastSettings.insert(QStringLiteral("addResultsToPlaylist"), true);
    batchLastSettings.insert(QStringLiteral("applyEqualizer"), false);
    batchLastSettings.insert(QStringLiteral("equalizerBandGains"),
                             gains({0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}));

    QVariantMap batchPresetSettings = batchLastSettings;
    batchPresetSettings.remove(QStringLiteral("retryPolicy"));

    QVariantMap batchPreset;
    batchPreset.insert(QStringLiteral("id"), QStringLiteral("batch:test"));
    batchPreset.insert(QStringLiteral("name"), QStringLiteral("Batch Test"));
    batchPreset.insert(QStringLiteral("settings"), batchPresetSettings);
    batchPreset.insert(QStringLiteral("updatedAtMs"), static_cast<qint64>(654321));
    const QVariantList batchUserPresets = {batchPreset};

    QVariantMap batchDraft;
    batchDraft.insert(QStringLiteral("schema"), QStringLiteral("waveflux.batch-audio-converter.draft.v1"));
    batchDraft.insert(QStringLiteral("persistedAtMs"), static_cast<qint64>(777777));
    batchDraft.insert(QStringLiteral("settings"), batchLastSettings);
    batchDraft.insert(QStringLiteral("jobMetadata"),
                      QVariantMap{{QStringLiteral("jobId"), QStringLiteral("draft-job")},
                                  {QStringLiteral("createdAtMs"), static_cast<qint64>(111)},
                                  {QStringLiteral("startedAtMs"), static_cast<qint64>(222)}} );
    batchDraft.insert(QStringLiteral("items"),
                      QVariantList{QVariantMap{
                          {QStringLiteral("itemId"), QStringLiteral("draft-item")},
                          {QStringLiteral("sourceFile"), QStringLiteral("/tmp/source.wav")},
                          {QStringLiteral("state"), QStringLiteral("pending")}
                      }});

    QVariantMap batchFinishedReport;
    batchFinishedReport.insert(QStringLiteral("schema"), QStringLiteral("waveflux.batch-audio-converter.report.v1"));
    batchFinishedReport.insert(QStringLiteral("jobMetadata"),
                               QVariantMap{{QStringLiteral("jobId"), QStringLiteral("finished-job")}});
    batchFinishedReport.insert(QStringLiteral("finalSummary"),
                               QVariantMap{{QStringLiteral("succeededCount"), 1}});
    const QVariantList batchFinishedJobs = {batchFinishedReport};

    QVariantMap ytDlpLastSettings;
    ytDlpLastSettings.insert(QStringLiteral("outputDirectory"), QStringLiteral("/tmp/yt-out"));
    ytDlpLastSettings.insert(QStringLiteral("selectedFormat"), QStringLiteral("opus"));
    ytDlpLastSettings.insert(QStringLiteral("namingPolicy"), QStringLiteral("title-only"));
    ytDlpLastSettings.insert(QStringLiteral("conflictPolicy"), QStringLiteral("skip-on-conflict"));
    ytDlpLastSettings.insert(QStringLiteral("parallelDownloads"), 4);
    QVariantMap ytDlpDraft;
    ytDlpDraft.insert(QStringLiteral("schema"), QStringLiteral("waveflux.ytdlp-import.v2"));
    ytDlpDraft.insert(QStringLiteral("persistedAtMs"), QDateTime::currentMSecsSinceEpoch());
    ytDlpDraft.insert(QStringLiteral("settings"), ytDlpLastSettings);
    ytDlpDraft.insert(QStringLiteral("jobMetadata"),
                      QVariantMap{{QStringLiteral("jobId"), QStringLiteral("yt-draft-job")},
                                  {QStringLiteral("createdAtMs"), static_cast<qint64>(999)}} );
    ytDlpDraft.insert(QStringLiteral("sources"),
                      QVariantList{QVariantMap{
                          {QStringLiteral("sourceId"), QStringLiteral("yt-source-1")},
                          {QStringLiteral("sourceStatus"), QStringLiteral("ready")},
                          {QStringLiteral("immutableSourceInput"),
                           QVariantMap{{QStringLiteral("normalizedUrl"),
                                        QStringLiteral("https://example.com/watch?v=one")}}}
                      }});
    const QVariantList ytDlpRecentSources = {
        QStringLiteral("https://example.com/watch?v=one"),
        QStringLiteral("https://example.com/playlist?list=two")
    };
    const QVariantList ytDlpRecentCanonicalSources = {
        QStringLiteral("https://example.com/canonical/one")
    };
    const QVariantList ytDlpRecentOutputDirectories = {
        QStringLiteral("/tmp/yt-out"),
        QStringLiteral("/tmp/yt-archive")
    };
    const QDateTime lastUpdateCheckAt(QDate(2026, 5, 22), QTime(8, 15, 30), QTimeZone::UTC);
    const QDateTime lastAutomaticUpdateCheckAt(QDate(2026, 5, 22), QTime(9, 15, 30), QTimeZone::UTC);
    const QDateTime updateReminderDeferredUntil(QDate(2026, 5, 23), QTime(8, 15, 30), QTimeZone::UTC);

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
        settings.setAutoAddTracksFromPlaylistFolder(false);
        settings.setAutoCheckUpdates(false);
        settings.setIncludePrereleaseUpdates(true);
        QVariantMap trackInfoOverlay = settings.defaultTrackInfoWaveformOverlayFormats();
        trackInfoOverlay.insert(QStringLiteral("topCenter"), QStringLiteral("%g"));
        trackInfoOverlay.insert(QStringLiteral("bottomRight"), QStringLiteral("%T/%r"));
        settings.setTrackInfoEnabled(false);
        settings.setTrackInfoWaveformOverlayHoverOnly(false);
        settings.setTrackInfoWindowTitleFormat(QStringLiteral("%F - test"));
        settings.setTrackInfoWaveformTooltipFormat(QStringLiteral("%C %o"));
        settings.setTrackInfoWaveformOverlayFormats(trackInfoOverlay);
        settings.setUpdateCheckerEtag(QStringLiteral(" \"etag-test\" "));
        settings.setLastUpdateCheckAt(lastUpdateCheckAt);
        settings.setLastAutomaticUpdateCheckAt(lastAutomaticUpdateCheckAt);
        settings.setSkippedUpdateTag(QStringLiteral(" v9.9.9 "));
        settings.setUpdateReminderDeferredUntil(updateReminderDeferredUntil);
        settings.setDeterministicShuffleEnabled(true);
        settings.setShuffleSeed(123456789u);
        settings.setRepeatableShuffle(false);
        settings.setSqliteLibraryEnabled(false);
        settings.setYtDlpExecutablePath(QStringLiteral("/opt/tools/yt-dlp"));
        settings.setFfmpegExecutablePath(QStringLiteral("/opt/tools/ffmpeg"));
        settings.setEqualizerBandGains(firstBandGains);
        settings.setEqualizerLastManualGains(secondBandGains);
        settings.setEqualizerUserPresets(userPresets);
        settings.setEqualizerActivePresetId(QStringLiteral("user:test"));
        settings.setBatchAudioConverterLastSettings(batchLastSettings);
        settings.setBatchAudioConverterUserPresets(batchUserPresets);
        settings.setBatchAudioConverterDraft(batchDraft);
        settings.setBatchAudioConverterFinishedJobs(batchFinishedJobs);
        settings.setYtDlpImportLastSettings(ytDlpLastSettings);
        settings.setYtDlpImportDraft(ytDlpDraft);
        settings.setYtDlpImportRecentSources(ytDlpRecentSources);
        settings.setYtDlpImportRecentCanonicalSources(ytDlpRecentCanonicalSources);
        settings.setYtDlpImportRecentOutputDirectories(ytDlpRecentOutputDirectories);
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
        QStringLiteral("autoAddTracksFromPlaylistFolder"),
        QStringLiteral("trackInfo.enabled"),
        QStringLiteral("trackInfo.waveformOverlayHoverOnly"),
        QStringLiteral("trackInfo.windowTitleFormat"),
        QStringLiteral("trackInfo.waveformTooltipFormat"),
        QStringLiteral("trackInfo.waveformOverlay.topLeft"),
        QStringLiteral("trackInfo.waveformOverlay.topCenter"),
        QStringLiteral("trackInfo.waveformOverlay.topRight"),
        QStringLiteral("trackInfo.waveformOverlay.middleLeft"),
        QStringLiteral("trackInfo.waveformOverlay.middleCenter"),
        QStringLiteral("trackInfo.waveformOverlay.middleRight"),
        QStringLiteral("trackInfo.waveformOverlay.bottomLeft"),
        QStringLiteral("trackInfo.waveformOverlay.bottomCenter"),
        QStringLiteral("trackInfo.waveformOverlay.bottomRight"),
        QStringLiteral("updates.autoCheck"),
        QStringLiteral("updates.includePrerelease"),
        QStringLiteral("updates.etag"),
        QStringLiteral("updates.lastCheckAt"),
        QStringLiteral("updates.lastAutoCheckAt"),
        QStringLiteral("updates.skippedTag"),
        QStringLiteral("updates.deferredUntil"),
        QStringLiteral("deterministicShuffleEnabled"),
        QStringLiteral("shuffleSeed"),
        QStringLiteral("repeatableShuffle"),
        QStringLiteral("library.sqlite.enabled"),
        QStringLiteral("ytDlp.executablePath"),
        QStringLiteral("ffmpeg.executablePath"),
        QStringLiteral("ytDlp.lastValidatedPath"),
        QStringLiteral("ffmpeg.lastValidatedPath"),
        QStringLiteral("equalizerBandGains"),
        QStringLiteral("equalizer.lastManualGains"),
        QStringLiteral("equalizer.userPresets"),
        QStringLiteral("equalizer.activePresetId"),
        QStringLiteral("batchAudioConverter.lastSettings"),
        QStringLiteral("batchAudioConverter.userPresets"),
        QStringLiteral("batchAudioConverter.draft"),
        QStringLiteral("batchAudioConverter.finishedJobs"),
        QStringLiteral("ytDlpImport.lastSettings"),
        QStringLiteral("ytDlpImport.draft"),
        QStringLiteral("ytDlpImport.recentSources"),
        QStringLiteral("ytDlpImport.recentCanonicalSources"),
        QStringLiteral("ytDlpImport.recentOutputDirectories")
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
    QCOMPARE(reloaded.autoAddTracksFromPlaylistFolder(), false);
    QCOMPARE(reloaded.autoCheckUpdates(), false);
    QCOMPARE(reloaded.includePrereleaseUpdates(), true);
    QCOMPARE(reloaded.trackInfoEnabled(), false);
    QCOMPARE(reloaded.trackInfoWaveformOverlayHoverOnly(), false);
    QCOMPARE(reloaded.trackInfoWindowTitleFormat(), QStringLiteral("%F - test"));
    QCOMPARE(reloaded.trackInfoWaveformTooltipFormat(), QStringLiteral("%C %o"));
    QCOMPARE(reloaded.trackInfoWaveformOverlayFormats().value(QStringLiteral("topCenter")).toString(),
             QStringLiteral("%g"));
    QCOMPARE(reloaded.trackInfoWaveformOverlayFormats().value(QStringLiteral("bottomRight")).toString(),
             QStringLiteral("%T/%r"));
    QCOMPARE(reloaded.updateCheckerEtag(), QStringLiteral("\"etag-test\""));
    QCOMPARE(reloaded.lastUpdateCheckAt(), lastUpdateCheckAt);
    QCOMPARE(reloaded.lastAutomaticUpdateCheckAt(), lastAutomaticUpdateCheckAt);
    QCOMPARE(reloaded.skippedUpdateTag(), QStringLiteral("v9.9.9"));
    QCOMPARE(reloaded.updateReminderDeferredUntil(), updateReminderDeferredUntil);
    QCOMPARE(reloaded.deterministicShuffleEnabled(), true);
    QCOMPARE(reloaded.shuffleSeed(), 123456789u);
    QCOMPARE(reloaded.repeatableShuffle(), false);
    QCOMPARE(reloaded.sqliteLibraryEnabled(), false);
    QCOMPARE(reloaded.ytDlpExecutablePath(), QStringLiteral("/opt/tools/yt-dlp"));
    QCOMPARE(reloaded.ffmpegExecutablePath(), QStringLiteral("/opt/tools/ffmpeg"));
    QCOMPARE(reloaded.equalizerBandGains(), secondBandGains);
    QCOMPARE(reloaded.equalizerLastManualGains(), secondBandGains);
    QCOMPARE(reloaded.equalizerUserPresets().size(), 1);
    QCOMPARE(reloaded.equalizerUserPresets().constFirst().toMap().value(QStringLiteral("id")).toString(),
             QStringLiteral("user:test"));
    QCOMPARE(reloaded.equalizerActivePresetId(), QStringLiteral("user:test"));
    QCOMPARE(reloaded.batchAudioConverterLastSettings(), batchLastSettings);
    QCOMPARE(reloaded.batchAudioConverterUserPresets().size(), 1);
    QCOMPARE(reloaded.batchAudioConverterUserPresets().constFirst().toMap().value(QStringLiteral("id")).toString(),
             QStringLiteral("batch:test"));
    QVERIFY(!reloaded.batchAudioConverterUserPresets().constFirst().toMap()
                 .value(QStringLiteral("settings")).toMap()
                 .contains(QStringLiteral("retryPolicy")));
    QCOMPARE(reloaded.batchAudioConverterDraft().value(QStringLiteral("schema")).toString(),
             QStringLiteral("waveflux.batch-audio-converter.draft.v1"));
    QCOMPARE(reloaded.batchAudioConverterDraft().value(QStringLiteral("items")).toList().size(), 1);
    QCOMPARE(reloaded.batchAudioConverterFinishedJobs().size(), 1);
    QCOMPARE(reloaded.batchAudioConverterFinishedJobs().constFirst().toMap()
                 .value(QStringLiteral("jobMetadata")).toMap()
                 .value(QStringLiteral("jobId")).toString(),
             QStringLiteral("finished-job"));
    QCOMPARE(reloaded.ytDlpImportLastSettings(), ytDlpLastSettings);
    QCOMPARE(reloaded.ytDlpImportDraft().value(QStringLiteral("jobMetadata")).toMap()
                 .value(QStringLiteral("jobId")).toString(),
             QStringLiteral("yt-draft-job"));
    QCOMPARE(reloaded.ytDlpImportRecentSources(), ytDlpRecentSources);
    QCOMPARE(reloaded.ytDlpImportRecentCanonicalSources(), ytDlpRecentCanonicalSources);
    QCOMPARE(reloaded.ytDlpImportRecentOutputDirectories(), ytDlpRecentOutputDirectories);
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

    clearSettings();

    {
        QSettings settings(QStringLiteral("WaveFlux"), QStringLiteral("WaveFlux"));
        settings.beginGroup(QStringLiteral("App"));
        settings.setValue(QStringLiteral("trackInfo.windowTitleFormat"),
                          QStringLiteral("{%a - %t — |%F — }WaveFlux %v"));
        settings.setValue(QStringLiteral("trackInfo.waveformTooltipFormat"), QStringLiteral("%C"));
        settings.endGroup();
        settings.sync();
    }

    AppSettingsManager fromOldTrackInfoDefault;
    QCOMPARE(fromOldTrackInfoDefault.trackInfoWindowTitleFormat(),
             fromOldTrackInfoDefault.defaultTrackInfoWindowTitleFormat());
    QCOMPARE(fromOldTrackInfoDefault.trackInfoWaveformTooltipFormat(), QString());

    clearSettings();

    {
        QSettings settings(QStringLiteral("WaveFlux"), QStringLiteral("WaveFlux"));
        settings.beginGroup(QStringLiteral("App"));
        settings.setValue(QStringLiteral("batchAudioConverter.draft"),
                          QVariantMap{{QStringLiteral("schema"), QStringLiteral("broken")},
                                      {QStringLiteral("items"), QVariantList{QVariantMap{{QStringLiteral("id"), 1}}}}});
        settings.setValue(QStringLiteral("batchAudioConverter.finishedJobs"),
                          QVariantList{QVariantMap{{QStringLiteral("schema"), QStringLiteral("broken")}}});
        settings.setValue(QStringLiteral("ytDlpImport.draft"),
                          QVariantMap{{QStringLiteral("schema"), QStringLiteral("waveflux.ytdlp-import.v2")},
                                      {QStringLiteral("persistedAtMs"), 1},
                                      {QStringLiteral("jobMetadata"),
                                       QVariantMap{{QStringLiteral("jobId"), QStringLiteral("old-draft")}}},
                                      {QStringLiteral("settings"),
                                       QVariantMap{{QStringLiteral("outputDirectory"), QStringLiteral("/tmp/out")}}},
                                      {QStringLiteral("sources"),
                                       QVariantList{QVariantMap{{QStringLiteral("sourceId"),
                                                                 QStringLiteral("old-source")}}}}});
        settings.endGroup();
        settings.sync();
    }

    AppSettingsManager fromBrokenBatchPersistence;
    QCOMPARE(fromBrokenBatchPersistence.batchAudioConverterDraft(), QVariantMap());
    QCOMPARE(fromBrokenBatchPersistence.batchAudioConverterFinishedJobs(), QVariantList());
    QCOMPARE(fromBrokenBatchPersistence.ytDlpImportDraft(), QVariantMap());
}

void AppSettingsManagerTest::persistsTrackInfoSettingsAndNormalizesFormats()
{
    const QString longFormat(1100, QLatin1Char('x'));
    QVariantMap partialOverlay;
    partialOverlay.insert(QStringLiteral("middleCenter"), longFormat);
    partialOverlay.insert(QStringLiteral("unknown"), QStringLiteral("ignored"));

    {
        AppSettingsManager settings;
        settings.setTrackInfoWindowTitleFormat(longFormat);
        settings.setTrackInfoWaveformTooltipFormat(QStringLiteral("%C %o"));
        settings.setTrackInfoWaveformOverlayFormats(partialOverlay);

        QCOMPARE(settings.trackInfoWindowTitleFormat().size(), 1024);
        QCOMPARE(settings.trackInfoWaveformOverlayFormats().value(QStringLiteral("middleCenter")).toString().size(),
                 1024);
        QVERIFY(!settings.trackInfoWaveformOverlayFormats().contains(QStringLiteral("unknown")));
        QCOMPARE(settings.renderTrackInfoFormat(QStringLiteral("%a - %t"),
                                                QVariantMap{{QStringLiteral("artist"), QStringLiteral("Artist")},
                                                            {QStringLiteral("title"), QStringLiteral("Title")}},
                                                QStringLiteral("windowTitle")),
                 QStringLiteral("Artist - Title"));
    }

    AppSettingsManager reloaded;
    QCOMPARE(reloaded.trackInfoWindowTitleFormat().size(), 1024);
    QCOMPARE(reloaded.trackInfoWaveformTooltipFormat(), QStringLiteral("%C %o"));
    QCOMPARE(reloaded.trackInfoWaveformOverlayFormats().value(QStringLiteral("middleCenter")).toString().size(),
             1024);
    QCOMPARE(reloaded.trackInfoWaveformOverlayFormats().value(QStringLiteral("topLeft")).toString(),
             reloaded.defaultTrackInfoWaveformOverlayFormats().value(QStringLiteral("topLeft")).toString());
}

void AppSettingsManagerTest::persistsYtDlpImportHistoryAndSanitizesSecrets()
{
    QVariantMap settingsPreset;
    settingsPreset.insert(QStringLiteral("outputDirectory"), QStringLiteral(" /tmp/ytdlp-defaults "));
    settingsPreset.insert(QStringLiteral("selectedFormat"), QStringLiteral("broken"));
    settingsPreset.insert(QStringLiteral("namingPolicy"), QStringLiteral("broken"));
    settingsPreset.insert(QStringLiteral("conflictPolicy"), QStringLiteral("broken"));
    settingsPreset.insert(QStringLiteral("parallelDownloads"), 99);

    const QVariantList recentSources = {
        QStringLiteral("https://example.com/watch?v=public"),
        QStringLiteral("https://example.com/watch?v=public"),
        QStringLiteral("https://example.com/watch?v=token&access_token=secret"),
        QStringLiteral("ftp://example.com/not-allowed"),
        QStringLiteral("https://user:pass@example.com/private"),
        QStringLiteral("not-a-url")
    };
    const QVariantList recentCanonicalSources = {
        QStringLiteral("https://example.com/canonical/track#fragment"),
        QStringLiteral("https://example.com/canonical/track#fragment")
    };
    const QVariantList recentOutputDirectories = {
        QStringLiteral(" /tmp/ytdlp-defaults "),
        QStringLiteral("/tmp/ytdlp-archive"),
        QStringLiteral("/tmp/ytdlp-defaults")
    };

    {
        AppSettingsManager settings;
        settings.setYtDlpImportLastSettings(settingsPreset);
        settings.setYtDlpImportRecentSources(recentSources);
        settings.setYtDlpImportRecentCanonicalSources(recentCanonicalSources);
        settings.setYtDlpImportRecentOutputDirectories(recentOutputDirectories);
    }

    AppSettingsManager reloaded;
    QCOMPARE(reloaded.ytDlpImportLastSettings().value(QStringLiteral("outputDirectory")).toString(),
             QStringLiteral("/tmp/ytdlp-defaults"));
    QCOMPARE(reloaded.ytDlpImportLastSettings().value(QStringLiteral("selectedFormat")).toString(),
             QStringLiteral("mp3"));
    QCOMPARE(reloaded.ytDlpImportLastSettings().value(QStringLiteral("namingPolicy")).toString(),
             QStringLiteral("auto"));
    QCOMPARE(reloaded.ytDlpImportLastSettings().value(QStringLiteral("conflictPolicy")).toString(),
             QStringLiteral("auto-rename"));
    QCOMPARE(reloaded.ytDlpImportLastSettings().value(QStringLiteral("parallelDownloads")).toInt(),
             4);
    QCOMPARE(reloaded.ytDlpImportRecentSources(),
             QVariantList{QStringLiteral("https://example.com/watch?v=public")});
    QCOMPARE(reloaded.ytDlpImportRecentCanonicalSources(),
             QVariantList{QStringLiteral("https://example.com/canonical/track")});
    const QVariantList expectedOutputDirectories = {
        QStringLiteral("/tmp/ytdlp-defaults"),
        QStringLiteral("/tmp/ytdlp-archive")
    };
    QCOMPARE(reloaded.ytDlpImportRecentOutputDirectories(), expectedOutputDirectories);
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

void AppSettingsManagerTest::resolvesImportToolRuntimeDeterministically()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString ytDlpPath = createExecutableTool(tempDir.path(),
                                                   QStringLiteral("yt-dlp"),
                                                   QStringLiteral("2025.04.01"));
    const QString ffmpegPath = createExecutableTool(tempDir.path(),
                                                    QStringLiteral("ffmpeg"),
                                                    QStringLiteral("ffmpeg version 7.0"));
    QVERIFY(!ytDlpPath.isEmpty());
    QVERIFY(!ffmpegPath.isEmpty());

    const QByteArray originalPath = qgetenv("PATH");
    qputenv("PATH", tempDir.path().toUtf8());

    AppSettingsManager settings;

    const QVariantMap ytFromPath = settings.inspectYtDlpExecutable();
    QCOMPARE(ytFromPath.value(QStringLiteral("ok")).toBool(), true);
    QCOMPARE(ytFromPath.value(QStringLiteral("source")).toString(), QStringLiteral("path"));
    QCOMPARE(ytFromPath.value(QStringLiteral("resolvedPath")).toString(),
             QDir::cleanPath(QDir::fromNativeSeparators(ytDlpPath)));
    QCOMPARE(ytFromPath.value(QStringLiteral("version")).toString(), QStringLiteral("2025.04.01"));

    const QVariantMap runtimeReady = settings.validateYtDlpImportRuntime(QStringLiteral("mp3"));
    QCOMPARE(runtimeReady.value(QStringLiteral("ok")).toBool(), true);
    QCOMPARE(runtimeReady.value(QStringLiteral("requiresFfmpeg")).toBool(), true);

    settings.setYtDlpExecutablePath(QStringLiteral("/tmp/does-not-exist-yt-dlp"));
    const QVariantMap ytInvalidConfigured = settings.inspectYtDlpExecutable();
    QCOMPARE(ytInvalidConfigured.value(QStringLiteral("ok")).toBool(), false);
    QCOMPARE(ytInvalidConfigured.value(QStringLiteral("source")).toString(), QStringLiteral("configured"));
    QCOMPARE(ytInvalidConfigured.value(QStringLiteral("errorCode")).toString(),
             QStringLiteral("yt_dlp_configured_path_invalid"));

    settings.setYtDlpExecutablePath(ytDlpPath);
    const QVariantMap ytConfigured = settings.inspectYtDlpExecutable();
    QCOMPARE(ytConfigured.value(QStringLiteral("ok")).toBool(), true);
    QCOMPARE(ytConfigured.value(QStringLiteral("source")).toString(), QStringLiteral("configured"));

    QTest::qWait(220);

    QSettings persisted(QStringLiteral("WaveFlux"), QStringLiteral("WaveFlux"));
    persisted.beginGroup(QStringLiteral("App"));
    QCOMPARE(persisted.value(QStringLiteral("ytDlp.lastValidatedPath")).toString(),
             QDir::cleanPath(QDir::fromNativeSeparators(ytDlpPath)));
    QCOMPARE(persisted.value(QStringLiteral("ffmpeg.lastValidatedPath")).toString(),
             QDir::cleanPath(QDir::fromNativeSeparators(ffmpegPath)));
    persisted.endGroup();

    qputenv("PATH", originalPath);
}

void AppSettingsManagerTest::usesDashVersionForRealFfmpegContract()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

#ifdef Q_OS_WIN
    const QString ffmpegPath = QDir(tempDir.path()).filePath(QStringLiteral("ffmpeg.bat"));
    const QByteArray script =
        "@echo off\r\n"
        "if \"%1\"==\"-version\" (\r\n"
        "  echo ffmpeg version 7.1\r\n"
        "  exit /b 0\r\n"
        ")\r\n"
        "if \"%1\"==\"--version\" (\r\n"
        "  echo ffmpeg version 7.1 1>&2\r\n"
        "  exit /b 8\r\n"
        ")\r\n"
        "exit /b 9\r\n";
    QVERIFY(!writeTextFile(tempDir.path(), QStringLiteral("ffmpeg.bat"), script).isEmpty());
#else
    const QString ffmpegPath = QDir(tempDir.path()).filePath(QStringLiteral("ffmpeg"));
    const QByteArray script =
        "#!/bin/sh\n"
        "if [ \"$1\" = \"-version\" ]; then\n"
        "  echo \"ffmpeg version 7.1\"\n"
        "  exit 0\n"
        "fi\n"
        "if [ \"$1\" = \"--version\" ]; then\n"
        "  echo \"ffmpeg version 7.1\" >&2\n"
        "  exit 8\n"
        "fi\n"
        "exit 9\n";
    QVERIFY(!writeTextFile(tempDir.path(), QStringLiteral("ffmpeg"), script).isEmpty());
    QFile::setPermissions(ffmpegPath,
                          QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
                              | QFileDevice::ReadGroup | QFileDevice::ExeGroup
                              | QFileDevice::ReadOther | QFileDevice::ExeOther);
#endif

    AppSettingsManager settings;
    settings.setFfmpegExecutablePath(ffmpegPath);

    const QVariantMap inspection = settings.inspectFfmpegExecutable();
    QCOMPARE(inspection.value(QStringLiteral("ok")).toBool(), true);
    QCOMPARE(inspection.value(QStringLiteral("source")).toString(), QStringLiteral("configured"));
    QCOMPARE(inspection.value(QStringLiteral("version")).toString(), QStringLiteral("ffmpeg version 7.1"));
}

QTEST_MAIN(AppSettingsManagerTest)
#include "tst_AppSettingsManager.moc"
