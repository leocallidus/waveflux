#include "AppSettingsManager.h"

#include <KLocalizedString>

#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QProcess>
#include <QRegularExpression>
#include <QHash>
#include <QSet>
#include <QStandardPaths>
#include <QUrl>
#include <QUrlQuery>
#include <functional>
#include <limits>

namespace {
constexpr quint32 kDefaultShuffleSeed = 0xC4E5D2A1u;
constexpr auto kBatchDraftSchema = "waveflux.batch-audio-converter.draft.v1";
constexpr auto kBatchReportSchema = "waveflux.batch-audio-converter.report.v1";
constexpr auto kYtDlpDraftSchema = "waveflux.ytdlp-import.v2";
constexpr int kBatchFinishedJobsRetention = 20;
constexpr qint64 kYtDlpDraftLifetimeMs = 7LL * 24LL * 60LL * 60LL * 1000LL;
constexpr int kYtDlpRecentSourcesRetention = 12;
constexpr int kYtDlpRecentOutputDirectoriesRetention = 8;
constexpr int kYtDlpDefaultParallelDownloads = 1;
constexpr int kYtDlpMinParallelDownloads = 1;
constexpr int kYtDlpMaxParallelDownloads = 4;
constexpr int kExternalToolStartTimeoutMs = 3000;
constexpr int kExternalToolFinishTimeoutMs = 4000;

quint32 normalizeShuffleSeed(const QVariant &value, quint32 fallback = kDefaultShuffleSeed)
{
    bool ok = false;
    const qulonglong parsed = value.toULongLong(&ok);
    if (!ok) {
        return fallback;
    }
    const qulonglong clamped = qMin(parsed, static_cast<qulonglong>(std::numeric_limits<quint32>::max()));
    return static_cast<quint32>(clamped);
}

QString normalizeExecutablePathValue(const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }

    return QDir::cleanPath(QDir::fromNativeSeparators(trimmed));
}

QString normalizeLocalPathValue(const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }

    return QDir::cleanPath(QDir::fromNativeSeparators(trimmed));
}

QString firstNonEmptyLine(const QString &text)
{
    const QStringList lines = text.split(QRegularExpression(QStringLiteral("[\r\n]+")),
                                         Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (!trimmed.isEmpty()) {
            return trimmed;
        }
    }
    return QString();
}

QStringList executableCandidates(const QString &baseName)
{
#ifdef Q_OS_WIN
    return {baseName + QStringLiteral(".exe"), baseName};
#else
    return {baseName};
#endif
}

QString findExecutableFromPath(const QString &baseName)
{
    for (const QString &candidate : executableCandidates(baseName)) {
        const QString resolved = QStandardPaths::findExecutable(candidate);
        if (!resolved.isEmpty()) {
            return QDir::cleanPath(QDir::fromNativeSeparators(resolved));
        }
    }
    return QString();
}

struct ToolInspectionResult {
    bool ok = false;
    QString toolKey;
    QString status;
    QString source;
    QString configuredPath;
    QString resolvedPath;
    QString version;
    QString errorCode;
    QString message;
};

QVariantMap toolInspectionToVariantMap(const ToolInspectionResult &result)
{
    QVariantMap map;
    map.insert(QStringLiteral("ok"), result.ok);
    map.insert(QStringLiteral("toolKey"), result.toolKey);
    map.insert(QStringLiteral("status"), result.status);
    map.insert(QStringLiteral("source"), result.source);
    map.insert(QStringLiteral("configuredPath"), result.configuredPath);
    map.insert(QStringLiteral("resolvedPath"), result.resolvedPath);
    map.insert(QStringLiteral("version"), result.version);
    map.insert(QStringLiteral("errorCategory"), result.ok ? QString() : QStringLiteral("dependency"));
    map.insert(QStringLiteral("errorCode"), result.errorCode);
    map.insert(QStringLiteral("message"), result.message);
    return map;
}

bool probeExecutableVersion(const QString &program,
                            const QStringList &versionArguments,
                            QString *versionOut,
                            QString *failureDetailsOut)
{
    QProcess process;
    process.setProgram(program);
    process.setArguments(versionArguments);
    process.start();
    if (!process.waitForStarted(kExternalToolStartTimeoutMs)) {
        if (failureDetailsOut) {
            *failureDetailsOut = process.errorString().trimmed();
        }
        return false;
    }
    if (!process.waitForFinished(kExternalToolFinishTimeoutMs)) {
        process.kill();
        process.waitForFinished(1000);
        if (failureDetailsOut) {
            *failureDetailsOut = QStringLiteral("Timed out while waiting for version output.");
        }
        return false;
    }

    const QString stdoutText = QString::fromLocal8Bit(process.readAllStandardOutput());
    const QString stderrText = QString::fromLocal8Bit(process.readAllStandardError());
    const QString versionLine = firstNonEmptyLine(stdoutText.isEmpty() ? stderrText : stdoutText);

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0 || versionLine.isEmpty()) {
        if (failureDetailsOut) {
            const QString details = firstNonEmptyLine(stderrText);
            if (!details.isEmpty()) {
                *failureDetailsOut = details;
            } else if (!process.errorString().trimmed().isEmpty()) {
                *failureDetailsOut = process.errorString().trimmed();
            } else {
                *failureDetailsOut = QStringLiteral("The executable did not return a valid version response.");
            }
        }
        return false;
    }

    if (versionOut) {
        *versionOut = versionLine;
    }
    return true;
}

ToolInspectionResult inspectExternalTool(const AppSettingsManager &settings,
                                         const QString &baseName,
                                         const QString &toolKey,
                                         const QString &configuredPath)
{
    ToolInspectionResult result;
    result.toolKey = toolKey;
    result.configuredPath = configuredPath;

    const QString displayName = toolKey;
    QString resolvedPath;
    QString source = QStringLiteral("none");

    if (!configuredPath.isEmpty()) {
        const QFileInfo info(configuredPath);
        if (!info.exists() || !info.isFile() || !info.isExecutable()) {
            result.status = QStringLiteral("invalid");
            result.source = QStringLiteral("configured");
            result.errorCode = toolKey == QStringLiteral("yt-dlp")
                ? QStringLiteral("yt_dlp_configured_path_invalid")
                : QStringLiteral("ffmpeg_configured_path_invalid");
            result.message = settings.translate(QStringLiteral("settings.externalToolConfiguredInvalid"))
                                 .arg(displayName, configuredPath);
            return result;
        }

        resolvedPath = info.absoluteFilePath();
        source = QStringLiteral("configured");
    } else {
        resolvedPath = findExecutableFromPath(baseName);
        if (resolvedPath.isEmpty()) {
            result.status = QStringLiteral("missing");
            result.source = QStringLiteral("none");
            result.errorCode = toolKey == QStringLiteral("yt-dlp")
                ? QStringLiteral("yt_dlp_not_found")
                : QStringLiteral("ffmpeg_not_found");
            result.message = settings.translate(QStringLiteral("settings.externalToolMissingFromPath"))
                                 .arg(displayName);
            return result;
        }
        source = QStringLiteral("path");
    }

    QString version;
    QString failureDetails;
    const QStringList versionArguments = toolKey == QStringLiteral("ffmpeg")
        ? QStringList{QStringLiteral("-version")}
        : QStringList{QStringLiteral("--version")};
    const QString normalizedResolvedPath =
        QDir::cleanPath(QDir::fromNativeSeparators(resolvedPath));
    if (!probeExecutableVersion(normalizedResolvedPath, versionArguments, &version, &failureDetails)) {
        result.status = QStringLiteral("invalid");
        result.source = source;
        result.resolvedPath = normalizedResolvedPath;
        result.errorCode = toolKey == QStringLiteral("yt-dlp")
            ? (source == QStringLiteral("configured")
                   ? QStringLiteral("yt_dlp_configured_path_invalid")
                   : QStringLiteral("yt_dlp_path_entry_invalid"))
            : (source == QStringLiteral("configured")
                   ? QStringLiteral("ffmpeg_configured_path_invalid")
                   : QStringLiteral("ffmpeg_path_entry_invalid"));
        result.message = settings.translate(QStringLiteral("settings.externalToolExecutionFailed"))
                             .arg(displayName, failureDetails);
        return result;
    }

    result.ok = true;
    result.status = QStringLiteral("found");
    result.source = source;
    result.resolvedPath = normalizedResolvedPath;
    result.version = version;
    result.message = settings.translate(
        source == QStringLiteral("configured")
            ? QStringLiteral("settings.externalToolReadyConfigured")
            : QStringLiteral("settings.externalToolReadyPath"))
                         .arg(displayName, normalizedResolvedPath);
    return result;
}

QString normalizeBatchNamingPolicy(const QString &value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("artist-title")
        || normalized == QStringLiteral("album-track-title")) {
        return normalized;
    }
    return QStringLiteral("basename");
}

QString normalizeBatchFormat(const QString &value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("flac")
        || normalized == QStringLiteral("wav")
        || normalized == QStringLiteral("opus")
        || normalized == QStringLiteral("webm")) {
        return normalized;
    }
    return QStringLiteral("mp3");
}

QString normalizeBatchConflictPolicy(const QString &value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("overwrite-if-allowed")
        || normalized == QStringLiteral("skip-on-conflict")
        || normalized == QStringLiteral("fail-on-conflict")) {
        return normalized;
    }
    return QStringLiteral("auto-rename");
}

QString normalizeBatchPlaylistAddMode(const QString &value, bool addResultsToPlaylistFallback = true)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("deferred")) {
        return normalized;
    }
    if (normalized == QStringLiteral("disabled")
        || normalized == QStringLiteral("off")
        || normalized == QStringLiteral("never")) {
        return QStringLiteral("disabled");
    }
    if (normalized == QStringLiteral("immediate")) {
        return normalized;
    }
    return addResultsToPlaylistFallback ? QStringLiteral("immediate") : QStringLiteral("disabled");
}

QString normalizeBatchRetryPolicy(const QString &value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("retry-failed-only")
        || normalized == QStringLiteral("retry-failed-and-skipped")) {
        return normalized;
    }
    return QStringLiteral("manual");
}

QString normalizeYtDlpFormat(const QString &value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("m4a")
        || normalized == QStringLiteral("opus")) {
        return normalized;
    }
    return QStringLiteral("mp3");
}

QString normalizeYtDlpNamingPolicy(const QString &value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("title-only")
        || normalized == QStringLiteral("source-title-entry-title")) {
        return normalized;
    }
    return QStringLiteral("auto");
}

QString normalizeYtDlpConflictPolicy(const QString &value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("overwrite-if-allowed")
        || normalized == QStringLiteral("skip-on-conflict")
        || normalized == QStringLiteral("fail-on-conflict")) {
        return normalized;
    }
    return QStringLiteral("auto-rename");
}

int normalizeYtDlpParallelDownloads(const QVariant &value)
{
    return qBound(kYtDlpMinParallelDownloads,
                  value.toInt(),
                  kYtDlpMaxParallelDownloads);
}

QString normalizeHttpUrlForHistory(const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }

    const QUrl parsed(trimmed);
    if (!parsed.isValid()) {
        return QString();
    }

    const QString scheme = parsed.scheme().trimmed().toLower();
    if (scheme != QStringLiteral("http") && scheme != QStringLiteral("https")) {
        return QString();
    }
    if (!parsed.userName().trimmed().isEmpty() || !parsed.password().trimmed().isEmpty()) {
        return QString();
    }

    QUrl normalized = parsed;
    normalized.setScheme(scheme);
    normalized.setHost(parsed.host().trimmed().toLower());
    normalized.setFragment(QString());

    static const QSet<QString> sensitiveKeys = {
        QStringLiteral("access_token"),
        QStringLiteral("api_key"),
        QStringLiteral("auth"),
        QStringLiteral("authorization"),
        QStringLiteral("key"),
        QStringLiteral("pass"),
        QStringLiteral("password"),
        QStringLiteral("secret"),
        QStringLiteral("sig"),
        QStringLiteral("signature"),
        QStringLiteral("token")
    };
    const QUrlQuery query(normalized);
    for (const auto &item : query.queryItems(QUrl::FullyDecoded)) {
        if (sensitiveKeys.contains(item.first.trimmed().toLower())) {
            return QString();
        }
    }

    return normalized.toString(QUrl::FullyEncoded).trimmed();
}

QVariantList normalizeUniqueStringHistory(const QVariantList &values,
                                          int retention,
                                          const std::function<QString(const QString &)> &normalizer)
{
    QVariantList normalized;
    QSet<QString> seen;
    normalized.reserve(values.size());
    for (const QVariant &value : values) {
        const QString sanitized = normalizer(value.toString());
        if (sanitized.isEmpty() || seen.contains(sanitized)) {
            continue;
        }
        normalized.push_back(sanitized);
        seen.insert(sanitized);
        if (normalized.size() >= retention) {
            break;
        }
    }
    return normalized;
}

QString normalizeBatchChannelMode(const QString &value)
{
    return value.trimmed().toLower() == QStringLiteral("mono")
        ? QStringLiteral("mono")
        : QStringLiteral("stereo");
}

int normalizeBatchBitrate(int value)
{
    return qMax(0, value);
}

int normalizeBatchSampleRate(int value)
{
    return qMax(0, value);
}

double normalizeBatchPlaybackRate(double value)
{
    if (!qIsFinite(value)) {
        return 1.0;
    }
    return qBound(0.25, value, 4.0);
}

int normalizeBatchPitchSemitones(int value)
{
    return qBound(-24, value, 24);
}

const QHash<QString, QString> &englishTexts()
{
    static const QHash<QString, QString> texts = {
        {QStringLiteral("app.title"), QStringLiteral("WaveFlux")},
        {QStringLiteral("main.openFiles"), QStringLiteral("Open Files")},
        {QStringLiteral("main.addFolder"), QStringLiteral("Add Folder")},
        {QStringLiteral("ytDlpImport.toolbarButton"), QStringLiteral("URL")},
        {QStringLiteral("main.exportPlaylist"), QStringLiteral("Export Playlist")},
        {QStringLiteral("main.clearPlaylist"), QStringLiteral("Clear Playlist")},
        {QStringLiteral("main.nowPlaying"), QStringLiteral("Now Playing")},
        {QStringLiteral("main.settings"), QStringLiteral("Settings")},
        {QStringLiteral("main.enterFullscreen"), QStringLiteral("Enter Fullscreen")},
        {QStringLiteral("main.exitFullscreen"), QStringLiteral("Exit Fullscreen")},
        {QStringLiteral("main.hideOverlay"), QStringLiteral("Hide Overlay")},
        {QStringLiteral("main.showOverlay"), QStringLiteral("Show Overlay")},
        {QStringLiteral("main.fullscreenHint"), QStringLiteral("F11 cycles fullscreen and overlay")},
        {QStringLiteral("main.noTrack"), QStringLiteral("No track")},
        {QStringLiteral("main.unknownArtist"), QStringLiteral("Unknown Artist")},
        {QStringLiteral("main.playbackError"), QStringLiteral("Playback Error")},
        {QStringLiteral("main.waveformError"), QStringLiteral("Waveform Error")},
        {QStringLiteral("main.filePickerError"), QStringLiteral("File Picker Error")},
        {QStringLiteral("main.export"), QStringLiteral("Export")},
        {QStringLiteral("main.exportError"), QStringLiteral("Export Error")},
        {QStringLiteral("main.exportComplete"), QStringLiteral("Export Complete")},
        {QStringLiteral("main.lastError"), QStringLiteral("Last error: ")},
        {QStringLiteral("error.fileUnavailable"), QStringLiteral("File is unavailable: %1")},
        {QStringLiteral("error.midiUnsupported"), QStringLiteral("MIDI playback is not supported yet: %1")},
        {QStringLiteral("error.trackerUnsupported"),
         QStringLiteral("Tracker module format is not supported yet in WaveFlux OpenMPT matrix: %1")},
        {QStringLiteral("error.seekUnavailable"), QStringLiteral("Seeking is unavailable for the current track.")},
        {QStringLiteral("error.failedInitializeAudioEngine"), QStringLiteral("Failed to initialize audio engine")},
        {QStringLiteral("error.failedStartPlayback"), QStringLiteral("Failed to start playback")},
        {QStringLiteral("error.failedSetPlaybackRate"),
         QStringLiteral("Failed to set playback rate to %1x (effective %2x)")},
        {QStringLiteral("error.failedResolvePlaybackUri"), QStringLiteral("Failed to resolve playback URI for: %1")},
        {QStringLiteral("error.trackLoadingTimedOut"), QStringLiteral("Track loading timed out")},
        {QStringLiteral("error.playbackStoppedRepeatedFailures"),
         QStringLiteral("Playback stopped after repeated load failures.")},
        {QStringLiteral("error.playlistExportNotInitialized"), QStringLiteral("Playlist export service is not initialized.")},
        {QStringLiteral("error.noTracksSelected"), QStringLiteral("No tracks selected.")},
        {QStringLiteral("error.playlistEmpty"), QStringLiteral("Playlist is empty.")},
        {QStringLiteral("error.noOutputPathSelected"), QStringLiteral("No output path selected.")},
        {QStringLiteral("error.failedOpenOutputFileWriting"), QStringLiteral("Failed to open output file for writing.")},
        {QStringLiteral("status.playlistExportedTo"), QStringLiteral("Playlist exported to %1")},
        {QStringLiteral("audioConverter.errorTrackerRenderPath"), QStringLiteral("Failed to prepare temporary tracker render path.")},
        {QStringLiteral("audioConverter.errorTrackerRenderFailed"), QStringLiteral("Tracker conversion render failed.")},
        {QStringLiteral("audioConverter.errorSourcePathInvalid"), QStringLiteral("Source file path is invalid.")},
        {QStringLiteral("audioConverter.errorFormatUnsupported"), QStringLiteral("Selected output format is not supported.")},
        {QStringLiteral("audioConverter.errorMissingPlugin"),
         QStringLiteral("Required GStreamer plugin is unavailable: %1. Install the missing encoder or muxer plugin and try again.")},
        {QStringLiteral("audioConverter.errorTempOutputPath"), QStringLiteral("Failed to prepare temporary output file path.")},
        {QStringLiteral("audioConverter.errorCreatePipeline"), QStringLiteral("Failed to create the GStreamer conversion pipeline.")},
        {QStringLiteral("audioConverter.errorOutputAlreadyExists"), QStringLiteral("Output file already exists: %1")},
        {QStringLiteral("audioConverter.errorReplaceExistingOutput"), QStringLiteral("Failed to replace the existing output file: %1 (%2)")},
        {QStringLiteral("audioConverter.errorFinalizeOutput"), QStringLiteral("Failed to finalize the converted output file: %1 (%2)")},
        {QStringLiteral("batchConverter.started"), QStringLiteral("Batch conversion started.")},
        {QStringLiteral("batchConverter.restoredDraft"), QStringLiteral("Restored batch draft from the previous session.")},
        {QStringLiteral("batchConverter.alreadyRunning"), QStringLiteral("A batch conversion is already running.")},
        {QStringLiteral("batchConverter.itemStarting"), QStringLiteral("Starting conversion...")},
        {QStringLiteral("batchConverter.itemConverting"), QStringLiteral("Converting %1")},
        {QStringLiteral("batchConverter.workerUnavailable"), QStringLiteral("Audio converter worker is unavailable.")},
        {QStringLiteral("batchConverter.startFailed"), QStringLiteral("Conversion failed to start.")},
        {QStringLiteral("error.playlistNameEmpty"), QStringLiteral("Playlist name is empty")},
        {QStringLiteral("error.playlistStorageLimitReached"), QStringLiteral("Playlist storage limit reached")},
        {QStringLiteral("error.failedLocateSavedPlaylist"), QStringLiteral("Failed to locate saved playlist")},
        {QStringLiteral("error.playlistNotFound"), QStringLiteral("Playlist not found")},
        {QStringLiteral("error.playlistNameAlreadyExists"), QStringLiteral("Playlist with this name already exists")},
        {QStringLiteral("error.failedLocateDuplicatedPlaylist"), QStringLiteral("Failed to locate duplicated playlist")},
        {QStringLiteral("error.failedOpenPlaylistsStorage"), QStringLiteral("Failed to open playlists storage")},
        {QStringLiteral("error.failedParsePlaylistsStorage"), QStringLiteral("Failed to parse playlists storage")},
        {QStringLiteral("error.failedOpenPlaylistsStorageWriting"), QStringLiteral("Failed to open playlists storage for writing")},
        {QStringLiteral("error.failedPersistPlaylistsStorage"), QStringLiteral("Failed to persist playlists storage")},
        {QStringLiteral("error.presetNameEmpty"), QStringLiteral("Preset name is empty")},
        {QStringLiteral("error.userPresetNotFound"), QStringLiteral("User preset not found")},
        {QStringLiteral("error.invalidUserPresetAtIndex"), QStringLiteral("Invalid user preset at index %1")},
        {QStringLiteral("error.invalidUserPresetAtIndexWithReason"), QStringLiteral("Invalid user preset at index %1: %2")},
        {QStringLiteral("error.builtinPresetIdNotAllowed"), QStringLiteral("Built-in preset id cannot be used for user preset")},
        {QStringLiteral("error.smartCollectionNotFound"), QStringLiteral("Smart collection not found")},
        {QStringLiteral("error.smartCollectionsDisabled"), QStringLiteral("Smart collections engine is disabled")},
        {QStringLiteral("error.collectionNameEmpty"), QStringLiteral("Collection name is empty")},
        {QStringLiteral("error.invalidSmartCollectionId"), QStringLiteral("Invalid smart collection id")},
        {QStringLiteral("error.failedPruneContextPlaybackProgress"), QStringLiteral("Failed to prune context playback progress")},
        {QStringLiteral("error.databaseConnectionNotOpen"), QStringLiteral("Database connection is not open")},
        {QStringLiteral("error.databasePathEmpty"), QStringLiteral("Database path is empty")},
        {QStringLiteral("error.invalidOutputPointer"), QStringLiteral("Invalid output pointer")},
        {QStringLiteral("error.failedReadSmartCollectionsCount"), QStringLiteral("Failed to read smart_collections count")},
        {QStringLiteral("error.openFileManagerEmptyPath"), QStringLiteral("Cannot open file manager: empty file path.")},
        {QStringLiteral("error.openFileManagerLocalOnly"), QStringLiteral("Cannot open file manager: only local files are supported.")},
        {QStringLiteral("error.openFileManagerFileNotFound"), QStringLiteral("Cannot open file manager: file not found.")},
        {QStringLiteral("error.openFileManagerSelectedFile"), QStringLiteral("Cannot open file manager for the selected file.")},
        {QStringLiteral("error.moveToTrashLocalOnly"), QStringLiteral("Cannot move file to Trash: only local files are supported.")},
        {QStringLiteral("error.moveToTrashFileNotFound"), QStringLiteral("Cannot move file to Trash: file not found.")},
        {QStringLiteral("error.moveToTrashFailed"),
         QStringLiteral("Cannot move file to Trash. Check permissions and Trash support for this disk.")},
        {QStringLiteral("error.openUrlInvalid"), QStringLiteral("Cannot open URL: invalid URL.")},
        {QStringLiteral("error.openUrlDefaultBrowser"), QStringLiteral("Cannot open URL in the default browser.")},
        {QStringLiteral("error.filePickerRequestInProgress"), QStringLiteral("A file picker request is already in progress.")},
        {QStringLiteral("error.xdgPortalUnavailable"), QStringLiteral("XDG portal is unavailable: %1")},
        {QStringLiteral("error.failedOpenXdgPortalFilePicker"), QStringLiteral("Failed to open XDG portal file picker: %1")},
        {QStringLiteral("error.xdgPortalInvalidRequestHandle"), QStringLiteral("XDG portal returned an invalid request handle.")},
        {QStringLiteral("error.failedSubscribeXdgPortalResponse"), QStringLiteral("Failed to subscribe to XDG portal response.")},
        {QStringLiteral("error.xdgPortalFilePickerReturnedError"), QStringLiteral("XDG portal file picker returned an error.")},
        {QStringLiteral("profiler.title"), QStringLiteral("Profiler")},
        {QStringLiteral("profiler.modeFullscreen"), QStringLiteral("[fullscreen]")},
        {QStringLiteral("profiler.modeWindowed"), QStringLiteral("[windowed]")},
        {QStringLiteral("profiler.playlistTracks"), QStringLiteral("Playlist tracks")},
        {QStringLiteral("profiler.sceneFps"), QStringLiteral("Scene FPS")},
        {QStringLiteral("profiler.avg"), QStringLiteral("avg")},
        {QStringLiteral("profiler.worst"), QStringLiteral("worst")},
        {QStringLiteral("profiler.wavePaintsPerSec"), QStringLiteral("Wave paint/s")},
        {QStringLiteral("profiler.repaintsFullPerSec"), QStringLiteral("Repaint/s full")},
        {QStringLiteral("profiler.partial"), QStringLiteral("partial")},
        {QStringLiteral("profiler.dirty"), QStringLiteral("dirty")},
        {QStringLiteral("profiler.playlistDataPerSec"), QStringLiteral("Playlist data/s")},
        {QStringLiteral("profiler.searchQueriesPerSec"), QStringLiteral("Search q/s")},
        {QStringLiteral("profiler.p95"), QStringLiteral("p95")},
        {QStringLiteral("profiler.searchBackendPerSec"), QStringLiteral("Search backend/s sqlite")},
        {QStringLiteral("profiler.searchBackendFts"), QStringLiteral("fts")},
        {QStringLiteral("profiler.searchBackendLike"), QStringLiteral("like")},
        {QStringLiteral("profiler.searchBackendFail"), QStringLiteral("fail")},
        {QStringLiteral("profiler.memoryWorkingSet"), QStringLiteral("Memory WS")},
        {QStringLiteral("profiler.private"), QStringLiteral("private")},
        {QStringLiteral("profiler.memoryCommit"), QStringLiteral("Memory commit")},
        {QStringLiteral("profiler.peakWorkingSet"), QStringLiteral("peak WS")},
        {QStringLiteral("profiler.lastCheckpoint"), QStringLiteral("Last checkpoint")},
        {QStringLiteral("profiler.lastExport"), QStringLiteral("Last export")},
        {QStringLiteral("profiler.notAvailable"), QStringLiteral("n/a")},
        {QStringLiteral("profiler.exportError"), QStringLiteral("Export error")},
        {QStringLiteral("profiler.hotkeys"), QStringLiteral("Hotkeys: Ctrl+Shift+P overlay, Ctrl+Shift+E enable, Ctrl+Shift+R reset")},
        {QStringLiteral("profiler.exportHotkeys"), QStringLiteral("Export: Ctrl+Shift+J json, Ctrl+Shift+C csv, Ctrl+Shift+B bundle")},
        {QStringLiteral("waveform.zoomBadgeZoom"), QStringLiteral("Zoom x%1")},
        {QStringLiteral("waveform.zoomBadgeQuick"), QStringLiteral("Quick x%1")},
        {QStringLiteral("waveform.zoomBadgeQuickScrub"), QStringLiteral("Quick scrub x%1")},
        {QStringLiteral("waveform.zoomBadgeFineSeekHint"), QStringLiteral("Shift-drag: fine seek")},
        {QStringLiteral("waveform.zoomBadgePanHint"), QStringLiteral("RMB-drag: pan (inertia)")},
        {QStringLiteral("waveform.loadingPlaceholder"), QStringLiteral("Waveform %1%")},
        {QStringLiteral("waveform.emptyPlaceholder"), QStringLiteral("Drop audio file here")},
        {QStringLiteral("waveform.unsupportedPlaceholder"), QStringLiteral("Waveform preview is unavailable for this source")},
        {QStringLiteral("waveform.failedPlaceholder"), QStringLiteral("Waveform could not be generated")},
        {QStringLiteral("waveform.silentPlaceholder"), QStringLiteral("Waveform is empty or silent")},
        {QStringLiteral("main.hires"), QStringLiteral("HI-RES")},
        {QStringLiteral("main.lossless"), QStringLiteral("LOSSLESS")},
        {QStringLiteral("dialogs.openAudioFiles"), QStringLiteral("Open Audio and XSPF Playlist Files")},
        {QStringLiteral("dialogs.addFolder"), QStringLiteral("Add Folder")},
        {QStringLiteral("dialogs.exportPlaylist"), QStringLiteral("Export Playlist")},
        {QStringLiteral("dialogs.audioFiles"),
         QStringLiteral("Audio and XSPF playlist files (*.mp3 *.flac *.ogg *.wav *.aac *.m4a *.xspf)")},
        {QStringLiteral("dialogs.audioFilterLabel"), QStringLiteral("Audio files")},
        {QStringLiteral("dialogs.xspfFilterLabel"), QStringLiteral("XSPF playlists")},
        {QStringLiteral("dialogs.allFilesFilterLabel"), QStringLiteral("All files")},
        {QStringLiteral("dialogs.allFiles"), QStringLiteral("All files (*)")},
        {QStringLiteral("dialogs.m3uPlaylist"), QStringLiteral("M3U Playlist (*.m3u *.m3u8)")},
        {QStringLiteral("dialogs.xspfPlaylist"), QStringLiteral("XSPF Playlist (*.xspf)")},
        {QStringLiteral("dialogs.jsonPlaylist"), QStringLiteral("JSON Playlist (*.json)")},
        {QStringLiteral("dialogs.chooseWaveformColor"), QStringLiteral("Choose Waveform Color")},
        {QStringLiteral("dialogs.chooseProgressColor"), QStringLiteral("Choose Progress Color")},
        {QStringLiteral("dialogs.chooseAccentColor"), QStringLiteral("Choose Accent Color")},
        {QStringLiteral("settings.title"), QStringLiteral("Settings")},
        {QStringLiteral("settings.appearance"), QStringLiteral("Appearance")},
        {QStringLiteral("settings.darkMode"), QStringLiteral("Dark Mode")},
        {QStringLiteral("settings.sidebarVisible"), QStringLiteral("Show right sidebar")},
        {QStringLiteral("settings.sidebarDescription"),
         QStringLiteral("Sidebar is automatically hidden on narrow windows (<900px)")},
        {QStringLiteral("settings.collectionsSidebarVisible"), QStringLiteral("Show collections panel")},
        {QStringLiteral("settings.collectionsSidebarDescription"),
         QStringLiteral("Left smart-collections panel in normal skin")},
        {QStringLiteral("settings.theme"), QStringLiteral("Theme:")},
        {QStringLiteral("settings.waveformColor"), QStringLiteral("Waveform Color:")},
        {QStringLiteral("settings.progressColor"), QStringLiteral("Progress Color:")},
        {QStringLiteral("settings.accentColor"), QStringLiteral("Accent Color:")},
        {QStringLiteral("settings.language"), QStringLiteral("Language:")},
        {QStringLiteral("settings.languageAuto"), QStringLiteral("Auto (System)")},
        {QStringLiteral("settings.languageEnglish"), QStringLiteral("English")},
        {QStringLiteral("settings.languageRussian"), QStringLiteral("Russian")},
        {QStringLiteral("settings.tray"), QStringLiteral("System Tray:")},
        {QStringLiteral("settings.system"), QStringLiteral("System")},
        {QStringLiteral("settings.trayEnabled"), QStringLiteral("Enable tray integration")},
        {QStringLiteral("settings.trayDescription"),
         QStringLiteral("Close button hides app to tray instead of exiting")},
        {QStringLiteral("settings.confirmTrashDeletion"), QStringLiteral("Confirm before moving tracks to Trash")},
        {QStringLiteral("settings.confirmTrashDeletionDescription"),
         QStringLiteral("Show a warning dialog before moving a file to Trash from the playlist")},
        {QStringLiteral("settings.automaticPlaylistSearch"), QStringLiteral("Automatic playlist search")},
        {QStringLiteral("settings.automaticPlaylistSearchDescription"),
         QStringLiteral("When disabled, playlist search starts only after pressing Enter or clicking the magnifier. When enabled, search updates while typing.")},
        {QStringLiteral("settings.autoAddTracksFromPlaylistFolder"),
         QStringLiteral("Auto-add new tracks from playlist folder")},
        {QStringLiteral("settings.autoAddTracksFromPlaylistFolderDescription"),
         QStringLiteral("When most playlist tracks come from one folder, WaveFlux watches that folder and appends newly added supported files automatically.")},
        {QStringLiteral("settings.ytDlpExecutablePath"), QStringLiteral("yt-dlp executable path")},
        {QStringLiteral("settings.ytDlpExecutablePathDescription"),
         QStringLiteral("Leave empty to resolve yt-dlp from PATH.")},
        {QStringLiteral("settings.ffmpegExecutablePath"), QStringLiteral("ffmpeg executable path")},
        {QStringLiteral("settings.ffmpegExecutablePathDescription"),
         QStringLiteral("Leave empty to resolve ffmpeg from PATH.")},
        {QStringLiteral("settings.browse"), QStringLiteral("Browse")},
        {QStringLiteral("settings.pickExecutableTitle"), QStringLiteral("Choose %1 executable")},
        {QStringLiteral("settings.externalToolConfiguredInvalid"),
         QStringLiteral("Configured path for %1 is invalid or not executable: %2")},
        {QStringLiteral("settings.externalToolMissingFromPath"),
         QStringLiteral("%1 was not found in PATH. Set an explicit executable path or install %1.")},
        {QStringLiteral("settings.externalToolExecutionFailed"),
         QStringLiteral("Failed to execute %1 --version: %2")},
        {QStringLiteral("settings.externalToolReadyConfigured"),
         QStringLiteral("Using configured %1 executable: %2")},
        {QStringLiteral("settings.externalToolReadyPath"),
         QStringLiteral("Using %1 from PATH: %2")},
        {QStringLiteral("settings.externalToolResolvedPath"), QStringLiteral("Resolved path")},
        {QStringLiteral("settings.externalToolVersion"), QStringLiteral("Version")},
        {QStringLiteral("settings.externalToolLastValidatedPath"), QStringLiteral("Last validated path")},
        {QStringLiteral("settings.importRuntimeVersionPolicy"), QStringLiteral("Version policy")},
        {QStringLiteral("settings.importRuntimeVersionPolicyDescription"),
         QStringLiteral("WaveFlux records executable versions for diagnostics.")},
        {QStringLiteral("ytDlpImport.probeStarted"), QStringLiteral("Reading source metadata...")},
        {QStringLiteral("ytDlpImport.probeAlreadyRunning"), QStringLiteral("A metadata probe is already running.")},
        {QStringLiteral("ytDlpImport.probeInvalidUrl"),
         QStringLiteral("Enter a valid http(s) URL before starting metadata probe.")},
        {QStringLiteral("ytDlpImport.probeRuntimeUnavailable"),
         QStringLiteral("yt-dlp runtime settings are unavailable.")},
        {QStringLiteral("ytDlpImport.probeCanceled"), QStringLiteral("Metadata probe was canceled.")},
        {QStringLiteral("ytDlpImport.probeFailedStart"),
         QStringLiteral("Failed to start yt-dlp metadata probe: %1")},
        {QStringLiteral("ytDlpImport.probeFailedProcess"),
         QStringLiteral("yt-dlp metadata probe failed: %1")},
        {QStringLiteral("ytDlpImport.probeFailedInvalidJson"),
         QStringLiteral("yt-dlp returned invalid metadata JSON: %1")},
        {QStringLiteral("ytDlpImport.errorDialogTitle"), QStringLiteral("yt-dlp error")},
        {QStringLiteral("ytDlpImport.probeReadySingle"),
         QStringLiteral("Metadata preview is ready for a single item.")},
        {QStringLiteral("ytDlpImport.probeReadyPlaylist"),
         QStringLiteral("Metadata preview is ready for %1 playlist entries.")},
        {QStringLiteral("ytDlpImport.importAlreadyRunning"),
         QStringLiteral("An import queue is already running.")},
        {QStringLiteral("ytDlpImport.importBlockedWhileProbing"),
         QStringLiteral("Finish metadata probe before starting the import.")},
        {QStringLiteral("ytDlpImport.importRequiresProbe"),
         QStringLiteral("Check the URL metadata before starting the import.")},
        {QStringLiteral("ytDlpImport.importStarted"),
         QStringLiteral("Starting audio import queue...")},
        {QStringLiteral("ytDlpImport.importRunningActiveCount"),
         QStringLiteral("Running: %1 active downloads.")},
        {QStringLiteral("ytDlpImport.importFinished"),
         QStringLiteral("Audio import queue finished.")},
        {QStringLiteral("ytDlpImport.importCanceled"),
         QStringLiteral("Audio import queue was canceled.")},
        {QStringLiteral("ytDlpImport.importInvalidOutputDirectory"),
         QStringLiteral("The selected output directory is invalid: %1")},
        {QStringLiteral("ytDlpImport.importNoPlayableItems"),
         QStringLiteral("There are no playable items to import.")},
        {QStringLiteral("ytDlpImport.itemStarting"),
         QStringLiteral("Starting download...")},
        {QStringLiteral("ytDlpImport.itemFinished"),
         QStringLiteral("Download finished.")},
        {QStringLiteral("ytDlpImport.itemCanceled"),
         QStringLiteral("Download canceled.")},
        {QStringLiteral("ytDlpImport.itemSkippedUnavailable"),
         QStringLiteral("Skipped: source item is unavailable.")},
        {QStringLiteral("ytDlpImport.itemMissingOutput"),
         QStringLiteral("Download finished without creating the expected file: %1")},
        {QStringLiteral("ytDlpImport.importFailedStart"),
         QStringLiteral("Failed to start yt-dlp import process: %1")},
        {QStringLiteral("ytDlpImport.importProcessFailed"),
         QStringLiteral("yt-dlp import failed: %1")},
        {QStringLiteral("ytDlpImport.errorCategoryDependency"), QStringLiteral("Dependency problem")},
        {QStringLiteral("ytDlpImport.errorCategoryContent"), QStringLiteral("Source content problem")},
        {QStringLiteral("ytDlpImport.errorCategoryNetwork"), QStringLiteral("Network problem")},
        {QStringLiteral("ytDlpImport.errorCategoryPermission"),
         QStringLiteral("Permission problem")},
        {QStringLiteral("ytDlpImport.errorCategoryDisk"), QStringLiteral("Disk space problem")},
        {QStringLiteral("ytDlpImport.errorCategoryOutput"), QStringLiteral("Output file problem")},
        {QStringLiteral("ytDlpImport.errorCategoryPostprocess"),
         QStringLiteral("Post-processing problem")},
        {QStringLiteral("ytDlpImport.errorCategoryCanceled"), QStringLiteral("Canceled")},
        {QStringLiteral("ytDlpImport.errorCategoryMixed"), QStringLiteral("Mixed problems")},
        {QStringLiteral("ytDlpImport.errorCategoryGeneric"), QStringLiteral("Import problem")},
        {QStringLiteral("ytDlpImport.summaryHeadlineSucceeded"),
         QStringLiteral("Import finished successfully.")},
        {QStringLiteral("ytDlpImport.summaryHeadlineSucceededWithSkips"),
         QStringLiteral("Import finished with skipped items.")},
        {QStringLiteral("ytDlpImport.summaryHeadlinePartialFailed"),
         QStringLiteral("Import finished with partial success.")},
        {QStringLiteral("ytDlpImport.summaryHeadlineFailed"),
         QStringLiteral("Import failed.")},
        {QStringLiteral("ytDlpImport.summaryHeadlineCanceled"),
         QStringLiteral("Import was canceled before any file was finalized.")},
        {QStringLiteral("ytDlpImport.summaryHeadlinePartialCanceled"),
         QStringLiteral("Import was canceled after partial success.")},
        {QStringLiteral("ytDlpImport.summaryDetailPattern"),
         QStringLiteral("Succeeded: %1, failed: %2, canceled: %3, skipped: %4.")},
        {QStringLiteral("ytDlpImport.problemGeneral"), QStringLiteral("General import failure")},
        {QStringLiteral("ytDlpImport.dialogTitle"), QStringLiteral("Import audio from URL")},
        {QStringLiteral("ytDlpImport.dialogSubtitle"),
         QStringLiteral("Probe the source first, confirm the output plan, then run a controlled sequential import.")},
        {QStringLiteral("ytDlpImport.urlSection"), QStringLiteral("Source URL")},
        {QStringLiteral("ytDlpImport.urlHint"),
         QStringLiteral("WaveFlux reads metadata first so you can verify the source before starting downloads.")},
        {QStringLiteral("ytDlpImport.urlPlaceholder"),
         QStringLiteral("https://example.com/watch?v=...")},
        {QStringLiteral("ytDlpImport.checkUrl"), QStringLiteral("Check URL")},
        {QStringLiteral("ytDlpImport.checkingUrl"), QStringLiteral("Checking...")},
        {QStringLiteral("ytDlpImport.pasteUrl"), QStringLiteral("Paste")},
        {QStringLiteral("ytDlpImport.previewSection"), QStringLiteral("Source preview")},
        {QStringLiteral("ytDlpImport.previewHint"),
         QStringLiteral("This preview comes from yt-dlp metadata probe and does not download files yet.")},
        {QStringLiteral("ytDlpImport.sourceTypeLabel"), QStringLiteral("Source type")},
        {QStringLiteral("ytDlpImport.sourceSingle"), QStringLiteral("Single track")},
        {QStringLiteral("ytDlpImport.sourcePlaylist"), QStringLiteral("Playlist")},
        {QStringLiteral("ytDlpImport.sourceTitleLabel"), QStringLiteral("Title")},
        {QStringLiteral("ytDlpImport.currentEntryTitleLabel"), QStringLiteral("Current entry")},
        {QStringLiteral("ytDlpImport.entryCountLabel"), QStringLiteral("Total entries")},
        {QStringLiteral("ytDlpImport.playableCountLabel"), QStringLiteral("Playable entries")},
        {QStringLiteral("ytDlpImport.unavailableCountLabel"), QStringLiteral("Unavailable entries")},
        {QStringLiteral("ytDlpImport.extractorLabel"), QStringLiteral("Extractor")},
        {QStringLiteral("ytDlpImport.redirectedLabel"), QStringLiteral("Resolved URL")},
        {QStringLiteral("ytDlpImport.outputSection"), QStringLiteral("Output configuration")},
        {QStringLiteral("ytDlpImport.outputHint"),
         QStringLiteral("Choose the saved format and the target folder before starting the queue.")},
        {QStringLiteral("ytDlpImport.namingPolicyLabel"), QStringLiteral("Naming rule")},
        {QStringLiteral("ytDlpImport.namingAuto"),
         QStringLiteral("Playlist index + title")},
        {QStringLiteral("ytDlpImport.namingTitleOnly"),
         QStringLiteral("Title only")},
        {QStringLiteral("ytDlpImport.namingSourceAndEntryTitle"),
         QStringLiteral("Source title + entry title")},
        {QStringLiteral("ytDlpImport.namingSummaryAuto"),
         QStringLiteral("Playlist imports keep a positional prefix so playlist order remains visible on disk.")},
        {QStringLiteral("ytDlpImport.namingSummaryTitleOnly"),
         QStringLiteral("Files are named from the source title only; conflicts are auto-renamed during planning.")},
        {QStringLiteral("ytDlpImport.namingSummarySourceAndEntryTitle"),
         QStringLiteral("File names combine source title and entry title when both are distinct.")},
        {QStringLiteral("ytDlpImport.parallelDownloadsLabel"), QStringLiteral("Download mode")},
        {QStringLiteral("ytDlpImport.parallelDownloadsSequentialOption"),
         QStringLiteral("1 download (Safe sequential)")},
        {QStringLiteral("ytDlpImport.parallelDownloadsParallelOption"),
         QStringLiteral("2 downloads (Controlled parallel)")},
        {QStringLiteral("ytDlpImport.parallelDownloadsHighParallelOption"),
         QStringLiteral("4 downloads (Higher throughput, higher risk)")},
        {QStringLiteral("ytDlpImport.parallelDownloadsSequentialHint"),
         QStringLiteral("Keeps the current safe mode: one yt-dlp worker at a time.")},
        {QStringLiteral("ytDlpImport.parallelDownloadsParallelHint"),
         QStringLiteral("Faster for larger playlists, but uses more disk and network pressure and can trigger rate limits sooner.")},
        {QStringLiteral("ytDlpImport.summarySection"), QStringLiteral("Import summary")},
        {QStringLiteral("ytDlpImport.summaryHint"),
         QStringLiteral("Review what will be downloaded, where files will be saved, and how the playlist order is preserved.")},
        {QStringLiteral("ytDlpImport.summaryTargetDirectory"), QStringLiteral("Save to")},
        {QStringLiteral("ytDlpImport.summaryFormat"), QStringLiteral("Output format")},
        {QStringLiteral("ytDlpImport.summaryNamingRule"), QStringLiteral("Naming")},
        {QStringLiteral("ytDlpImport.summaryItems"), QStringLiteral("Entries")},
        {QStringLiteral("ytDlpImport.summaryPlayable"), QStringLiteral("Will import")},
        {QStringLiteral("ytDlpImport.summaryUnavailable"), QStringLiteral("Will skip")},
        {QStringLiteral("ytDlpImport.summaryQueueMode"), QStringLiteral("Queue mode")},
        {QStringLiteral("ytDlpImport.summaryQueueModeSequential"),
         QStringLiteral("Sequential: one yt-dlp process at a time.")},
        {QStringLiteral("ytDlpImport.summaryQueueModeParallel"),
         QStringLiteral("Parallel: up to %1 yt-dlp processes at once.")},
        {QStringLiteral("ytDlpImport.summaryPlaylistOrder"), QStringLiteral("Playlist UI order")},
        {QStringLiteral("ytDlpImport.summaryPlaylistOrderValue"),
         QStringLiteral("Successful files are appended in source order. Runtime completion order is ignored.")},
        {QStringLiteral("ytDlpImport.queueSection"), QStringLiteral("Queue preview")},
        {QStringLiteral("ytDlpImport.queueHint"),
         QStringLiteral("Before start this list shows probe entries. During import it switches to runtime queue state.")},
        {QStringLiteral("ytDlpImport.queueStateReady"), QStringLiteral("Ready")},
        {QStringLiteral("ytDlpImport.queueStatePending"), QStringLiteral("Pending")},
        {QStringLiteral("ytDlpImport.queueStateRunning"), QStringLiteral("Running")},
        {QStringLiteral("ytDlpImport.queueStateSucceeded"), QStringLiteral("Succeeded")},
        {QStringLiteral("ytDlpImport.queueStateFailed"), QStringLiteral("Failed")},
        {QStringLiteral("ytDlpImport.queueStateCanceled"), QStringLiteral("Canceled")},
        {QStringLiteral("ytDlpImport.queueStateSkipped"), QStringLiteral("Skipped")},
        {QStringLiteral("ytDlpImport.queueStateUnavailable"), QStringLiteral("Unavailable")},
        {QStringLiteral("ytDlpImport.untitledEntry"), QStringLiteral("Untitled source entry")},
        {QStringLiteral("ytDlpImport.plannedOutputLabel"), QStringLiteral("Planned file: %1")},
        {QStringLiteral("ytDlpImport.progressSection"), QStringLiteral("Progress")},
        {QStringLiteral("ytDlpImport.progressHint"),
         QStringLiteral("Critical failures are surfaced here directly; no log reading is required.")},
        {QStringLiteral("ytDlpImport.activeDownloadsLabel"), QStringLiteral("Active downloads")},
        {QStringLiteral("ytDlpImport.finalSummarySection"), QStringLiteral("Final summary")},
        {QStringLiteral("ytDlpImport.finalSummaryHint"),
         QStringLiteral("Only successfully finalized local files are passed to the playlist import step.")},
        {QStringLiteral("ytDlpImport.finalSucceededCount"), QStringLiteral("Succeeded")},
        {QStringLiteral("ytDlpImport.finalFailedCount"), QStringLiteral("Failed")},
        {QStringLiteral("ytDlpImport.finalCanceledCount"), QStringLiteral("Canceled")},
        {QStringLiteral("ytDlpImport.finalSkippedCount"), QStringLiteral("Skipped")},
        {QStringLiteral("ytDlpImport.finalImportedCount"), QStringLiteral("Imported into playlist")},
        {QStringLiteral("ytDlpImport.finalNotProbedCount"), QStringLiteral("Not probed")},
        {QStringLiteral("ytDlpImport.finalConflictBlockedCount"), QStringLiteral("Conflict blocked")},
        {QStringLiteral("ytDlpImport.recentUrls"), QStringLiteral("Recent URLs")},
        {QStringLiteral("ytDlpImport.recentFolders"), QStringLiteral("Recent folders")},
        {QStringLiteral("ytDlpImport.sourcesSection"), QStringLiteral("Sources")},
        {QStringLiteral("ytDlpImport.sourcesHint"),
         QStringLiteral("Manage the source queue before and between runs.")},
        {QStringLiteral("ytDlpImport.clearFailedProbes"), QStringLiteral("Clear failed probes")},
        {QStringLiteral("ytDlpImport.retryFailedProbes"), QStringLiteral("Retry failed probes")},
        {QStringLiteral("ytDlpImport.retryFailedImports"), QStringLiteral("Retry failed imports")},
        {QStringLiteral("ytDlpImport.reopenLatestReport"), QStringLiteral("Reopen latest report")},
        {QStringLiteral("ytDlpImport.sourceStatusPending"), QStringLiteral("Pending")},
        {QStringLiteral("ytDlpImport.sourceStatusPendingProbe"), QStringLiteral("Pending probe")},
        {QStringLiteral("ytDlpImport.sourceStatusProbing"), QStringLiteral("Probing")},
        {QStringLiteral("ytDlpImport.sourceStatusReady"), QStringLiteral("Ready")},
        {QStringLiteral("ytDlpImport.sourceStatusReadyWithIssues"), QStringLiteral("Ready with issues")},
        {QStringLiteral("ytDlpImport.sourceStatusProbeFailed"), QStringLiteral("Probe failed")},
        {QStringLiteral("ytDlpImport.sourceStatusImporting"), QStringLiteral("Importing")},
        {QStringLiteral("ytDlpImport.sourceStatusCompleted"), QStringLiteral("Completed")},
        {QStringLiteral("ytDlpImport.sourceStatusCompletedWithFailures"),
         QStringLiteral("Completed with failures")},
        {QStringLiteral("ytDlpImport.sourceStatusCanceled"), QStringLiteral("Canceled")},
        {QStringLiteral("ytDlpImport.sourceEntryCount"), QStringLiteral("%1 entries")},
        {QStringLiteral("ytDlpImport.sourcePreviewStale"), QStringLiteral("Stale preview")},
        {QStringLiteral("ytDlpImport.diagnosticsNamingAuto"),
         QStringLiteral("Naming: playlist index + title")},
        {QStringLiteral("ytDlpImport.diagnosticsNamingTitleOnly"),
         QStringLiteral("Naming: title only")},
        {QStringLiteral("ytDlpImport.diagnosticsNamingSourceAndEntryTitle"),
         QStringLiteral("Naming: source title + entry title")},
        {QStringLiteral("ytDlpImport.diagnosticsFallbackMissingSourceTitle"),
         QStringLiteral("Fallback: source title missing")},
        {QStringLiteral("ytDlpImport.diagnosticsFallbackRedundantSourceTitle"),
         QStringLiteral("Fallback: source title matched entry title")},
        {QStringLiteral("ytDlpImport.diagnosticsFallbackNonPlaylistSource"),
         QStringLiteral("Fallback: no playlist index for this source")},
        {QStringLiteral("ytDlpImport.diagnosticsConflictScopeQueue"),
         QStringLiteral("Conflict scope: another item in this job")},
        {QStringLiteral("ytDlpImport.diagnosticsConflictScopeExistingTarget"),
         QStringLiteral("Conflict scope: file already exists on disk")},
        {QStringLiteral("ytDlpImport.diagnosticsResolutionAutoRenamed"),
         QStringLiteral("Resolution: auto-renamed")},
        {QStringLiteral("ytDlpImport.diagnosticsResolutionSkipOnConflict"),
         QStringLiteral("Resolution: will be skipped")},
        {QStringLiteral("ytDlpImport.diagnosticsResolutionFailOnConflict"),
         QStringLiteral("Resolution: will fail before download")},
        {QStringLiteral("ytDlpImport.clearButton"), QStringLiteral("Clear")},
        {QStringLiteral("ytDlpImport.cancelButton"), QStringLiteral("Cancel")},
        {QStringLiteral("ytDlpImport.hideSession"), QStringLiteral("Hide session")},
        {QStringLiteral("ytDlpImport.showSession"), QStringLiteral("Show session")},
        {QStringLiteral("ytDlpImport.sessionReadyHidden"),
         QStringLiteral("The current URL import session is still available in the background.")},
        {QStringLiteral("ytDlpImport.startImport"), QStringLiteral("Start import")},
        {QStringLiteral("ytDlpImport.stateIdle"), QStringLiteral("Idle")},
        {QStringLiteral("ytDlpImport.stateProbing"), QStringLiteral("Probing")},
        {QStringLiteral("ytDlpImport.stateReady"), QStringLiteral("Ready")},
        {QStringLiteral("ytDlpImport.stateRunning"), QStringLiteral("Running")},
        {QStringLiteral("ytDlpImport.stateCanceled"), QStringLiteral("Canceled")},
        {QStringLiteral("ytDlpImport.stateFailed"), QStringLiteral("Failed")},
        {QStringLiteral("ytDlpImport.stateSucceeded"), QStringLiteral("Succeeded")},
        {QStringLiteral("ytDlpImport.selectOutputFolderTitle"),
         QStringLiteral("Select output folder for URL import")},
        {QStringLiteral("ytDlpImport.errorInvalidOutputDirectory"),
         QStringLiteral("The selected output directory is invalid.")},
        {QStringLiteral("ytDlpImport.clipboardEmpty"),
         QStringLiteral("Clipboard does not contain text to paste into the URL field.")},
        {QStringLiteral("settings.audio"), QStringLiteral("Audio")},
        {QStringLiteral("settings.pitch"), QStringLiteral("Pitch (semitones):")},
        {QStringLiteral("settings.resetPitch"), QStringLiteral("Reset pitch")},
        {QStringLiteral("settings.colors"), QStringLiteral("Colors")},
        {QStringLiteral("settings.presetThemes"), QStringLiteral("Preset Themes")},
        {QStringLiteral("settings.dark"), QStringLiteral("Dark")},
        {QStringLiteral("settings.light"), QStringLiteral("Light")},
        {QStringLiteral("settings.reset"), QStringLiteral("Reset")},
        {QStringLiteral("settings.close"), QStringLiteral("Close")},
        {QStringLiteral("settings.aboutVersion"), QStringLiteral("WaveFlux v1.2.0")},
        {QStringLiteral("settings.aboutTagline"),
         QStringLiteral("A minimalist audio player with waveform visualization")},
        {QStringLiteral("player.previous"), QStringLiteral("Previous")},
        {QStringLiteral("player.pause"), QStringLiteral("Pause")},
        {QStringLiteral("player.play"), QStringLiteral("Play")},
        {QStringLiteral("player.stop"), QStringLiteral("Stop")},
        {QStringLiteral("player.next"), QStringLiteral("Next")},
        {QStringLiteral("player.shuffleEnable"), QStringLiteral("Enable shuffle")},
        {QStringLiteral("player.shuffleDisable"), QStringLiteral("Disable shuffle")},
        {QStringLiteral("player.repeatOff"), QStringLiteral("Repeat: Off")},
        {QStringLiteral("player.repeatAll"), QStringLiteral("Repeat: All tracks")},
        {QStringLiteral("player.repeatOne"), QStringLiteral("Repeat: Current track")},
        {QStringLiteral("player.resetSpeed"), QStringLiteral("Reset speed")},
        {QStringLiteral("player.spaceHoldSpeed2x"), QStringLiteral("hold for temporary 2x speed")},
        {QStringLiteral("player.resetPitch"), QStringLiteral("Reset pitch")},
        {QStringLiteral("player.semitones"), QStringLiteral("semitones")},
        {QStringLiteral("playlist.searchPlaceholder"),
         QStringLiteral("Search... title: artist: album: path: is:lossless is:hires")},
        {QStringLiteral("playlist.sort"), QStringLiteral("Sort")},
        {QStringLiteral("playlist.sortPlaylist"), QStringLiteral("Sort playlist")},
        {QStringLiteral("playlist.random"), QStringLiteral("Random")},
        {QStringLiteral("playlist.randomize"), QStringLiteral("Randomize playlist order")},
        {QStringLiteral("playlist.locate"), QStringLiteral("Locate")},
        {QStringLiteral("playlist.locateCurrent"), QStringLiteral("Scroll to current track")},
        {QStringLiteral("playlist.clear"), QStringLiteral("Clear")},
        {QStringLiteral("playlist.clearPlaylist"), QStringLiteral("Clear playlist")},
        {QStringLiteral("playlist.tracks"), QStringLiteral("tracks")},
        {QStringLiteral("playlist.matches"), QStringLiteral("matches")},
        {QStringLiteral("playlist.dropHint"), QStringLiteral("Drop audio files or .xspf playlists here\nor use File > Open")},
        {QStringLiteral("playlist.noMatches"), QStringLiteral("No tracks match your search")},
        {QStringLiteral("playlist.byNameAsc"), QStringLiteral("By Name (A-Z)")},
        {QStringLiteral("playlist.byNameDesc"), QStringLiteral("By Name (Z-A)")},
        {QStringLiteral("playlist.byDateOldest"), QStringLiteral("By Date Added (Oldest)")},
        {QStringLiteral("playlist.byDateNewest"), QStringLiteral("By Date Added (Newest)")},
        {QStringLiteral("tagEditor.title"), QStringLiteral("Edit Tags")},
        {QStringLiteral("tagEditor.titleLabel"), QStringLiteral("Title:")},
        {QStringLiteral("tagEditor.artist"), QStringLiteral("Artist:")},
        {QStringLiteral("tagEditor.album"), QStringLiteral("Album:")},
        {QStringLiteral("tagEditor.genre"), QStringLiteral("Genre:")},
        {QStringLiteral("tagEditor.year"), QStringLiteral("Year:")},
        {QStringLiteral("tagEditor.trackNumber"), QStringLiteral("Track #:")},
        {QStringLiteral("tagEditor.cover"), QStringLiteral("Cover:")},
        {QStringLiteral("tagEditor.coverSelect"), QStringLiteral("Choose...")},
        {QStringLiteral("tagEditor.coverClear"), QStringLiteral("Remove")},
        {QStringLiteral("tagEditor.coverKeep"), QStringLiteral("Keep existing embedded cover")},
        {QStringLiteral("tagEditor.coverSelected"), QStringLiteral("Selected: ")},
        {QStringLiteral("tagEditor.coverRemovePending"), QStringLiteral("Cover will be removed on save")},
        {QStringLiteral("tagEditor.coverPickerTitle"), QStringLiteral("Choose Cover Image")},
        {QStringLiteral("tagEditor.file"), QStringLiteral("File: ")},
        {QStringLiteral("tagEditor.error"), QStringLiteral("Error: ")},
        {QStringLiteral("tagEditor.bulkTitle"), QStringLiteral("Edit Tags for Selection")},
        {QStringLiteral("tagEditor.bulkHint"), QStringLiteral("Enable fields to overwrite for all selected tracks.")},
        {QStringLiteral("tagEditor.bulkApply"), QStringLiteral("Apply to Selected")},
        {QStringLiteral("audioConverter.title"), QStringLiteral("Audio Converter")},
        {QStringLiteral("audioConverter.sourceSection"), QStringLiteral("Source track")},
        {QStringLiteral("audioConverter.sourcePath"), QStringLiteral("Path: ")},
        {QStringLiteral("audioConverter.duration"), QStringLiteral("Duration: ")},
        {QStringLiteral("audioConverter.originalFormat"), QStringLiteral("Original format: ")},
        {QStringLiteral("audioConverter.sourceSpec"), QStringLiteral("Original spec: ")},
        {QStringLiteral("audioConverter.outputSection"), QStringLiteral("Output file")},
        {QStringLiteral("audioConverter.outputSectionHint"), QStringLiteral("Choose where the converted file will be saved. Existing files are never replaced without confirmation.")},
        {QStringLiteral("audioConverter.outputPlaceholder"), QStringLiteral("Choose a target audio file")},
        {QStringLiteral("audioConverter.browse"), QStringLiteral("Browse...")},
        {QStringLiteral("audioConverter.useSuggested"), QStringLiteral("Use suggested")},
        {QStringLiteral("audioConverter.outputHint"), QStringLiteral("Suggested path: ")},
        {QStringLiteral("audioConverter.formatSection"), QStringLiteral("Format and quality")},
        {QStringLiteral("audioConverter.formatSectionHint"), QStringLiteral("Pick the target format and the audio quality that will be written to the new file.")},
        {QStringLiteral("audioConverter.format"), QStringLiteral("Format")},
        {QStringLiteral("audioConverter.formatUnavailableLabel"), QStringLiteral("%1 (Unavailable)")},
        {QStringLiteral("audioConverter.formatUnavailableHint"), QStringLiteral("%1 is unavailable on this installation. Install the required conversion components and try again: %2")},
        {QStringLiteral("audioConverter.formatUnavailableGenericHint"), QStringLiteral("%1 is unavailable on this installation. Install the required conversion components and try again.")},
        {QStringLiteral("audioConverter.codec"), QStringLiteral("Codec")},
        {QStringLiteral("audioConverter.container"), QStringLiteral("Container")},
        {QStringLiteral("audioConverter.bitrate"), QStringLiteral("Bitrate")},
        {QStringLiteral("audioConverter.sampleRate"), QStringLiteral("Sample rate")},
        {QStringLiteral("audioConverter.channels"), QStringLiteral("Channels")},
        {QStringLiteral("audioConverter.channelMono"), QStringLiteral("Mono")},
        {QStringLiteral("audioConverter.channelStereo"), QStringLiteral("Stereo")},
        {QStringLiteral("audioConverter.transformSection"), QStringLiteral("Transform")},
        {QStringLiteral("audioConverter.transformSectionHint"), QStringLiteral("Adjust playback speed and pitch only if you want the converted file to sound different from the source.")},
        {QStringLiteral("audioConverter.speed"), QStringLiteral("Speed: ")},
        {QStringLiteral("audioConverter.pitch"), QStringLiteral("Pitch: ")},
        {QStringLiteral("audioConverter.reset"), QStringLiteral("Reset")},
        {QStringLiteral("audioConverter.semitones"), QStringLiteral("semitones")},
        {QStringLiteral("audioConverter.statusSection"), QStringLiteral("Status")},
        {QStringLiteral("audioConverter.badgeReady"), QStringLiteral("Ready")},
        {QStringLiteral("audioConverter.badgeAttention"), QStringLiteral("Attention")},
        {QStringLiteral("audioConverter.badgeConflict"), QStringLiteral("Replace file")},
        {QStringLiteral("audioConverter.badgeRunning"), QStringLiteral("Running")},
        {QStringLiteral("audioConverter.badgeSucceeded"), QStringLiteral("Done")},
        {QStringLiteral("audioConverter.badgeCanceled"), QStringLiteral("Canceled")},
        {QStringLiteral("audioConverter.badgeFailed"), QStringLiteral("Failed")},
        {QStringLiteral("audioConverter.statusTitleReady"), QStringLiteral("Ready to convert")},
        {QStringLiteral("audioConverter.statusTitleNeedsAttention"), QStringLiteral("Review the output setup")},
        {QStringLiteral("audioConverter.statusTitleConflict"), QStringLiteral("Replacement confirmation required")},
        {QStringLiteral("audioConverter.statusTitleRunning"), QStringLiteral("Conversion in progress")},
        {QStringLiteral("audioConverter.statusTitleSucceeded"), QStringLiteral("Converted file is ready")},
        {QStringLiteral("audioConverter.statusTitleCanceled"), QStringLiteral("Conversion was canceled")},
        {QStringLiteral("audioConverter.statusTitleFailed"), QStringLiteral("Conversion could not finish")},
        {QStringLiteral("audioConverter.progressAccessibleName"), QStringLiteral("Conversion progress")},
        {QStringLiteral("audioConverter.runtimeReady"), QStringLiteral("Choose an output path and conversion settings.")},
        {QStringLiteral("audioConverter.runtimeStarted"), QStringLiteral("Converting audio. Closing the dialog will cancel the current operation.")},
        {QStringLiteral("audioConverter.runtimeCanceled"), QStringLiteral("Conversion was canceled. You can adjust the settings and try again.")},
        {QStringLiteral("audioConverter.runtimeSucceeded"), QStringLiteral("Conversion finished successfully.")},
        {QStringLiteral("audioConverter.runtimeSucceededMetadataSkipped"), QStringLiteral("Conversion finished successfully. Basic metadata could not be copied: %1")},
        {QStringLiteral("audioConverter.runtimeFailedAlreadyRunning"), QStringLiteral("Another conversion is already running. Wait for it to finish or cancel it before starting a new one.")},
        {QStringLiteral("audioConverter.runtimeFailedStartPreparation"), QStringLiteral("Conversion could not start. Check the source file, output path, and required conversion components, then try again.")},
        {QStringLiteral("audioConverter.runtimeFailedStartPlayback"), QStringLiteral("Conversion could not start processing the new output. Try again or choose different conversion settings.")},
        {QStringLiteral("audioConverter.runtimeFailedPipeline"), QStringLiteral("Conversion stopped because audio processing failed. Check that the source file and required conversion components are available, then try again.")},
        {QStringLiteral("audioConverter.runtimeFailedExistingOutput"), QStringLiteral("A file with the same name already exists. Confirm replacement before starting the conversion.")},
        {QStringLiteral("audioConverter.runtimeFailedReplaceExisting"), QStringLiteral("The existing output file could not be replaced: %1")},
        {QStringLiteral("audioConverter.runtimeFailedFinalizeOutput"), QStringLiteral("The converted file could not be finalized at the selected destination: %1")},
        {QStringLiteral("audioConverter.runtimeFailedGeneric"), QStringLiteral("Conversion failed. Check the source file and destination, then try again.")},
        {QStringLiteral("audioConverter.readyHint"), QStringLiteral("Choose an output path and conversion settings.")},
        {QStringLiteral("audioConverter.stateRunning"), QStringLiteral("Converting audio. Closing the dialog will cancel the current operation.")},
        {QStringLiteral("audioConverter.stateSucceeded"), QStringLiteral("Conversion finished successfully.")},
        {QStringLiteral("audioConverter.stateCanceled"), QStringLiteral("Conversion was canceled. You can adjust the settings and try again.")},
        {QStringLiteral("audioConverter.stateFailed"), QStringLiteral("Conversion failed. Fix the problem and try again.")},
        {QStringLiteral("audioConverter.resultPath"), QStringLiteral("Saved file: ")},
        {QStringLiteral("audioConverter.closeAccessibleDescription"), QStringLiteral("Close the converter dialog without changing the current conversion settings.")},
        {QStringLiteral("audioConverter.escapeRunningConfirmTitle"), QStringLiteral("Cancel Conversion and Close")},
        {QStringLiteral("audioConverter.escapeRunningConfirmMessage"), QStringLiteral("Audio conversion is still running. Cancel the current operation and close the dialog?")},
        {QStringLiteral("audioConverter.showInPlaylist"), QStringLiteral("Show in Playlist")},
        {QStringLiteral("audioConverter.summaryFormat"), QStringLiteral("Format")},
        {QStringLiteral("audioConverter.summaryTransform"), QStringLiteral("Transform")},
        {QStringLiteral("audioConverter.summaryOutput"), QStringLiteral("Output file")},
        {QStringLiteral("audioConverter.convert"), QStringLiteral("Convert")},
        {QStringLiteral("audioConverter.replace"), QStringLiteral("Replace")},
        {QStringLiteral("audioConverter.cancel"), QStringLiteral("Cancel")},
        {QStringLiteral("audioConverter.close"), QStringLiteral("Close")},
        {QStringLiteral("audioConverter.saveDialogTitle"), QStringLiteral("Choose Output Audio File")},
        {QStringLiteral("audioConverter.confirmReplaceTitle"), QStringLiteral("Replace Existing File")},
        {QStringLiteral("audioConverter.confirmReplaceMessage"),
         QStringLiteral("A file with this name already exists.\n\nIt will be permanently replaced by the converted file. Continue?")},
        {QStringLiteral("audioConverter.notAvailable"), QStringLiteral("-")},
        {QStringLiteral("audioConverter.errorLocalOnly"), QStringLiteral("Only local files can be converted in this version.")},
        {QStringLiteral("audioConverter.errorInvalidOutputPath"), QStringLiteral("The selected output path is invalid.")},
        {QStringLiteral("audioConverter.errorTrackRequired"), QStringLiteral("Choose one local track for conversion.")},
        {QStringLiteral("audioConverter.errorCueUnsupported"), QStringLiteral("CUE segment conversion will be added later.")},
        {QStringLiteral("audioConverter.preflightSourceRequired"), QStringLiteral("Choose one local track for conversion.")},
        {QStringLiteral("audioConverter.preflightSourceUnreadable"), QStringLiteral("Source file is missing or unreadable: %1")},
        {QStringLiteral("audioConverter.preflightFormatUnsupported"), QStringLiteral("Selected output format is not supported: %1")},
        {QStringLiteral("audioConverter.preflightOutputRequired"), QStringLiteral("Choose an output file path.")},
        {QStringLiteral("audioConverter.preflightOutputInvalidPath"), QStringLiteral("Enter an absolute local output path or choose one with the file picker.")},
        {QStringLiteral("audioConverter.preflightOutputMatchesSource"), QStringLiteral("Output file must be different from the source file.")},
        {QStringLiteral("audioConverter.preflightOutputDirectoryMissing"), QStringLiteral("Output directory does not exist: %1")},
        {QStringLiteral("audioConverter.preflightOutputDirectoryNotWritable"), QStringLiteral("Output directory is not writable: %1")},
        {QStringLiteral("audioConverter.preflightOutputDirectoryWriteProbeFailed"), QStringLiteral("Cannot write to the output directory: %1")},
        {QStringLiteral("audioConverter.preflightExistingOutputConfirm"), QStringLiteral("A file already exists at this path. Confirm replacement to continue: %1")},
        {QStringLiteral("audioConverter.preflightExistingOutputNotWritable"), QStringLiteral("Existing output file is not writable: %1")},
        {QStringLiteral("audioConverter.preflightMissingPlugins"), QStringLiteral("Required conversion components for %1 are unavailable: %2")},
        {QStringLiteral("batchAudioConverter.title"), QStringLiteral("Batch Audio Conversion")},
        {QStringLiteral("batchAudioConverter.summaryLine"), QStringLiteral("%1 selected, %2 will be processed, %3 skipped.")},
        {QStringLiteral("batchAudioConverter.summarySection"), QStringLiteral("Batch summary")},
        {QStringLiteral("batchAudioConverter.summaryHint"), QStringLiteral("Review the scope before starting the queue.")},
        {QStringLiteral("batchAudioConverter.selectedCount"), QStringLiteral("Selected tracks")},
        {QStringLiteral("batchAudioConverter.willProcess"), QStringLiteral("Will process")},
        {QStringLiteral("batchAudioConverter.skippedCount"), QStringLiteral("Skipped")},
        {QStringLiteral("batchAudioConverter.outputSection"), QStringLiteral("Output layout")},
        {QStringLiteral("batchAudioConverter.outputHint"), QStringLiteral("Preview output paths update before the batch starts.")},
        {QStringLiteral("batchAudioConverter.presetsSection"), QStringLiteral("Reusable presets")},
        {QStringLiteral("batchAudioConverter.presetsHint"), QStringLiteral("Save and reapply batch settings without affecting runtime state.")},
        {QStringLiteral("batchAudioConverter.preset"), QStringLiteral("Preset")},
        {QStringLiteral("batchAudioConverter.noPresets"), QStringLiteral("No saved presets yet")},
        {QStringLiteral("batchAudioConverter.saveAsPreset"), QStringLiteral("Save as preset")},
        {QStringLiteral("batchAudioConverter.applyPreset"), QStringLiteral("Apply preset")},
        {QStringLiteral("batchAudioConverter.renamePreset"), QStringLiteral("Rename preset")},
        {QStringLiteral("batchAudioConverter.deletePreset"), QStringLiteral("Delete preset")},
        {QStringLiteral("batchAudioConverter.presetNamePlaceholder"), QStringLiteral("Preset name")},
        {QStringLiteral("batchAudioConverter.presetNameRequired"), QStringLiteral("Preset name is required.")},
        {QStringLiteral("batchAudioConverter.deletePresetTitle"), QStringLiteral("Delete batch preset")},
        {QStringLiteral("batchAudioConverter.deletePresetMessage"), QStringLiteral("Delete preset \"%1\"?")},
        {QStringLiteral("batchAudioConverter.outputDirectory"), QStringLiteral("Output directory")},
        {QStringLiteral("batchAudioConverter.outputDirectoryPlaceholder"), QStringLiteral("Leave empty to use each source folder")},
        {QStringLiteral("batchAudioConverter.browseFolder"), QStringLiteral("Browse...")},
        {QStringLiteral("batchAudioConverter.addFiles"), QStringLiteral("Add Files...")},
        {QStringLiteral("batchAudioConverter.addFolder"), QStringLiteral("Add Folder...")},
        {QStringLiteral("batchAudioConverter.useSourceFolders"), QStringLiteral("Use source folders")},
        {QStringLiteral("batchAudioConverter.namingPolicy"), QStringLiteral("Naming policy")},
        {QStringLiteral("batchAudioConverter.namingBasename"), QStringLiteral("Basename")},
        {QStringLiteral("batchAudioConverter.namingArtistTitle"), QStringLiteral("Artist - Title")},
        {QStringLiteral("batchAudioConverter.namingAlbumTrackTitle"), QStringLiteral("Album - Track - Title")},
        {QStringLiteral("batchAudioConverter.conflictPolicy"), QStringLiteral("Conflict policy")},
        {QStringLiteral("batchAudioConverter.conflictAutoRename"), QStringLiteral("Auto rename")},
        {QStringLiteral("batchAudioConverter.conflictOverwrite"), QStringLiteral("Overwrite if allowed")},
        {QStringLiteral("batchAudioConverter.conflictSkip"), QStringLiteral("Skip on conflict")},
        {QStringLiteral("batchAudioConverter.conflictFail"), QStringLiteral("Fail on conflict")},
        {QStringLiteral("batchAudioConverter.playlistMode"), QStringLiteral("Playlist results")},
        {QStringLiteral("batchAudioConverter.playlistModeImmediate"), QStringLiteral("Add immediately")},
        {QStringLiteral("batchAudioConverter.playlistModeDeferred"), QStringLiteral("Add after the batch finishes")},
        {QStringLiteral("batchAudioConverter.playlistModeDisabled"), QStringLiteral("Do not add to playlist")},
        {QStringLiteral("batchAudioConverter.playlistModeHint"), QStringLiteral("Choose whether successful results are inserted as they finish, only after review, or not at all.")},
        {QStringLiteral("batchAudioConverter.previewNamingPattern"), QStringLiteral("Name policy: %1.")},
        {QStringLiteral("batchAudioConverter.previewFallbackPattern"), QStringLiteral("Fallback to %1 because metadata is missing: %2.")},
        {QStringLiteral("batchAudioConverter.previewDirectorySourceFolder"), QStringLiteral("Output folder: source folder.")},
        {QStringLiteral("batchAudioConverter.previewDirectoryBatchOutput"), QStringLiteral("Output folder: shared batch output directory.")},
        {QStringLiteral("batchAudioConverter.previewCollisionPattern"), QStringLiteral("Collision handling: %1.")},
        {QStringLiteral("batchAudioConverter.previewFinalizationPattern"), QStringLiteral("Write mode: %1.")},
        {QStringLiteral("batchAudioConverter.previewCollisionPlanned"), QStringLiteral("no conflict")},
        {QStringLiteral("batchAudioConverter.previewCollisionAutoRenamed"), QStringLiteral("auto-renamed to avoid a conflict")},
        {QStringLiteral("batchAudioConverter.previewCollisionOverwriteExisting"), QStringLiteral("existing target will be replaced")},
        {QStringLiteral("batchAudioConverter.previewCollisionSkipConflict"), QStringLiteral("item will be skipped on conflict")},
        {QStringLiteral("batchAudioConverter.previewCollisionFailConflict"), QStringLiteral("item will fail on conflict")},
        {QStringLiteral("batchAudioConverter.previewCollisionQueueConflict"), QStringLiteral("another queued item already targets this path")},
        {QStringLiteral("batchAudioConverter.previewCollisionOverwriteBlocked"), QStringLiteral("overwrite is blocked because another queued item already targets this path")},
        {QStringLiteral("batchAudioConverter.previewFinalizationTempCommit"), QStringLiteral("write to a temp file, then commit the new output")},
        {QStringLiteral("batchAudioConverter.previewFinalizationTempReplace"), QStringLiteral("write to a temp file, then replace the existing output")},
        {QStringLiteral("batchAudioConverter.previewFinalizationNotStarted"), QStringLiteral("no output file will be written")},
        {QStringLiteral("batchAudioConverter.metadataArtist"), QStringLiteral("artist")},
        {QStringLiteral("batchAudioConverter.metadataTitle"), QStringLiteral("title")},
        {QStringLiteral("batchAudioConverter.metadataAlbum"), QStringLiteral("album")},
        {QStringLiteral("batchAudioConverter.metadataTrackNumber"), QStringLiteral("track number")},
        {QStringLiteral("batchAudioConverter.addResultsToPlaylist"), QStringLiteral("Add successful results to the current playlist")},
        {QStringLiteral("batchAudioConverter.formatHint"), QStringLiteral("These settings apply to every queued item.")},
        {QStringLiteral("batchAudioConverter.transformHint"), QStringLiteral("Speed and pitch are applied to the whole batch.")},
        {QStringLiteral("batchAudioConverter.queueSection"), QStringLiteral("Queue preview")},
        {QStringLiteral("batchAudioConverter.queueHint"), QStringLiteral("Each item shows the planned output path and runtime state.")},
        {QStringLiteral("batchAudioConverter.removeSelected"), QStringLiteral("Remove Selected")},
        {QStringLiteral("batchAudioConverter.clearFailed"), QStringLiteral("Clear Failed")},
        {QStringLiteral("batchAudioConverter.clearCompleted"), QStringLiteral("Clear Completed")},
        {QStringLiteral("batchAudioConverter.retrySelected"), QStringLiteral("Retry Selected")},
        {QStringLiteral("batchAudioConverter.retryFailed"), QStringLiteral("Retry Failed")},
        {QStringLiteral("batchAudioConverter.retrySkipped"), QStringLiteral("Retry Skipped")},
        {QStringLiteral("batchAudioConverter.clearSelection"), QStringLiteral("Clear Selection")},
        {QStringLiteral("batchAudioConverter.filterAll"), QStringLiteral("All")},
        {QStringLiteral("batchAudioConverter.filterPending"), QStringLiteral("Pending")},
        {QStringLiteral("batchAudioConverter.filterFailed"), QStringLiteral("Failed")},
        {QStringLiteral("batchAudioConverter.filterSucceeded"), QStringLiteral("Succeeded")},
        {QStringLiteral("batchAudioConverter.viewCompact"), QStringLiteral("Compact")},
        {QStringLiteral("batchAudioConverter.viewExpanded"), QStringLiteral("Expanded")},
        {QStringLiteral("batchAudioConverter.moveUp"), QStringLiteral("Up")},
        {QStringLiteral("batchAudioConverter.moveDown"), QStringLiteral("Down")},
        {QStringLiteral("batchAudioConverter.retry"), QStringLiteral("Retry")},
        {QStringLiteral("batchAudioConverter.remove"), QStringLiteral("Remove")},
        {QStringLiteral("batchAudioConverter.queueCompactPattern"), QStringLiteral("%1. %2")},
        {QStringLiteral("batchAudioConverter.runtimeSection"), QStringLiteral("Runtime monitor")},
        {QStringLiteral("batchAudioConverter.runtimeHint"), QStringLiteral("Progress stays visible after errors and after completion.")},
        {QStringLiteral("batchAudioConverter.viewExpandedReport"), QStringLiteral("Expanded report")},
        {QStringLiteral("batchAudioConverter.copyReport"), QStringLiteral("Copy report")},
        {QStringLiteral("batchAudioConverter.openOutputFolder"), QStringLiteral("Open output folder")},
        {QStringLiteral("batchAudioConverter.addSucceededOutputsToPlaylist"), QStringLiteral("Add succeeded outputs to playlist")},
        {QStringLiteral("batchAudioConverter.exportText"), QStringLiteral("Export Text")},
        {QStringLiteral("batchAudioConverter.exportJson"), QStringLiteral("Export JSON")},
        {QStringLiteral("batchAudioConverter.exportCsv"), QStringLiteral("Export CSV")},
        {QStringLiteral("batchAudioConverter.currentTrack"), QStringLiteral("Current track: ")},
        {QStringLiteral("batchAudioConverter.noCurrentTrack"), QStringLiteral("No active item")},
        {QStringLiteral("batchAudioConverter.currentTrackProgress"), QStringLiteral("Current item progress")},
        {QStringLiteral("batchAudioConverter.batchProgress"), QStringLiteral("Batch progress")},
        {QStringLiteral("batchAudioConverter.summaryDone"), QStringLiteral("Completed %1 of %2. Succeeded: %3. Failed: %4. Canceled: %5. Skipped: %6.")},
        {QStringLiteral("batchAudioConverter.stickyFinalSummary"), QStringLiteral("Final summary")},
        {QStringLiteral("batchAudioConverter.footerSummary"), QStringLiteral("Pending: %1. Running: %2. Succeeded: %3. Failed: %4.")},
        {QStringLiteral("batchAudioConverter.runtimeNoOutputFolder"), QStringLiteral("There is no output folder to open yet.")},
        {QStringLiteral("batchAudioConverter.runtimeOpenedOutputFolder"), QStringLiteral("Opened the output folder.")},
        {QStringLiteral("batchAudioConverter.runtimeFailedToOpenOutputFolder"), QStringLiteral("Failed to open the output folder.")},
        {QStringLiteral("batchAudioConverter.runtimeNoReportToCopy"), QStringLiteral("There is no finished report to copy yet.")},
        {QStringLiteral("batchAudioConverter.runtimeCopiedReport"), QStringLiteral("Copied the batch report to the clipboard.")},
        {QStringLiteral("batchAudioConverter.runtimeFailedToCopyReport"), QStringLiteral("Failed to copy the batch report to the clipboard.")},
        {QStringLiteral("batchAudioConverter.runtimeAddedSucceededOutputs"), QStringLiteral("Added %1 succeeded outputs to the playlist.")},
        {QStringLiteral("batchAudioConverter.runtimeNoDeferredOutputs"), QStringLiteral("There are no deferred outputs left to add to the playlist.")},
        {QStringLiteral("batchAudioConverter.convertSelected"), QStringLiteral("Convert selected")},
        {QStringLiteral("batchAudioConverter.selectInputFilesTitle"), QStringLiteral("Choose Files for Batch Conversion")},
        {QStringLiteral("batchAudioConverter.selectInputFolderTitle"), QStringLiteral("Choose Folder for Batch Conversion")},
        {QStringLiteral("batchAudioConverter.selectOutputFolderTitle"), QStringLiteral("Choose Batch Output Folder")},
        {QStringLiteral("batchAudioConverter.errorSelectionRequired"), QStringLiteral("Choose at least one local file for batch conversion.")},
        {QStringLiteral("batchAudioConverter.errorInvalidSourceFolder"), QStringLiteral("The selected source folder is invalid or contains no supported files.")},
        {QStringLiteral("batchAudioConverter.errorInvalidOutputDirectory"), QStringLiteral("The selected output directory is invalid.")},
        {QStringLiteral("batchAudioConverter.statePending"), QStringLiteral("Pending")},
        {QStringLiteral("batchAudioConverter.stateRunning"), QStringLiteral("Running")},
        {QStringLiteral("batchAudioConverter.stateSucceeded"), QStringLiteral("Succeeded")},
        {QStringLiteral("batchAudioConverter.stateFailed"), QStringLiteral("Failed")},
        {QStringLiteral("batchAudioConverter.stateCanceled"), QStringLiteral("Canceled")},
        {QStringLiteral("batchAudioConverter.stateSkipped"), QStringLiteral("Skipped")},
        {QStringLiteral("playlist.play"), QStringLiteral("Play")},
        {QStringLiteral("playlist.playNext"), QStringLiteral("Play Next")},
        {QStringLiteral("playlist.addToQueue"), QStringLiteral("Add to Queue")},
        {QStringLiteral("playlist.clearQueue"), QStringLiteral("Clear Queue")},
        {QStringLiteral("playlist.removeSelected"), QStringLiteral("Remove Selected")},
        {QStringLiteral("playlist.editTagsSelected"), QStringLiteral("Edit Tags for Selection...")},
        {QStringLiteral("playlist.audioConverterSelected"), QStringLiteral("Convert Selected...")},
        {QStringLiteral("menu.importUrl"), QStringLiteral("Import from URL...")},
        {QStringLiteral("playlist.exportSelected"), QStringLiteral("Export Selected...")},
        {QStringLiteral("playlist.moveToTrash"), QStringLiteral("Move to Trash")},
        {QStringLiteral("playlist.confirmTrashTitle"), QStringLiteral("Move Track to Trash")},
        {QStringLiteral("playlist.confirmTrashMessage"),
         QStringLiteral("The track file will be moved to Trash and removed from the playlist. Continue?")},
        {QStringLiteral("playlist.openInFileManager"), QStringLiteral("Show in File Manager")},
        {QStringLiteral("playlist.editTags"), QStringLiteral("Edit Tags...")},
        {QStringLiteral("playlist.audioConverter"), QStringLiteral("Audio Converter...")},
        {QStringLiteral("playlist.remove"), QStringLiteral("Remove")},
        {QStringLiteral("tray.showHide"), QStringLiteral("Show/Hide")},
        {QStringLiteral("tray.play"), QStringLiteral("Play")},
        {QStringLiteral("tray.pause"), QStringLiteral("Pause")},
        {QStringLiteral("tray.stop"), QStringLiteral("Stop")},
        {QStringLiteral("tray.previous"), QStringLiteral("Previous")},
        {QStringLiteral("tray.next"), QStringLiteral("Next")},
        {QStringLiteral("tray.settings"), QStringLiteral("Settings...")},
        {QStringLiteral("tray.quit"), QStringLiteral("Quit")},
        {QStringLiteral("settings.skin"), QStringLiteral("Skin:")},
        {QStringLiteral("settings.skinNormal"), QStringLiteral("Normal")},
        {QStringLiteral("settings.skinCompact"), QStringLiteral("Compact")},
        {QStringLiteral("settings.skinDescription"), QStringLiteral("Compact mode provides minimal interface for small screens")},
        {QStringLiteral("settings.waveformSection"), QStringLiteral("Waveform")},
        {QStringLiteral("settings.themeSection"), QStringLiteral("Theme")},
        {QStringLiteral("settings.sectionAppearanceDescription"),
         QStringLiteral("Language, skin mode, and interface layout options.")},
        {QStringLiteral("settings.sectionSystemDescription"),
         QStringLiteral("Tray behavior and safety confirmations.")},
        {QStringLiteral("settings.sectionAudioDescription"),
         QStringLiteral("Playback controls, speed/pitch, and shuffle behavior.")},
        {QStringLiteral("settings.sectionWaveformDescription"),
         QStringLiteral("Waveform geometry, hints, and CUE overlays.")},
        {QStringLiteral("settings.sectionColorsDescription"),
         QStringLiteral("Waveform, progress, and accent colors.")},
        {QStringLiteral("settings.sectionThemeDescription"),
         QStringLiteral("Theme presets and global reset options.")},
        {QStringLiteral("settings.searchPlaceholder"), QStringLiteral("Search settings...")},
        {QStringLiteral("settings.quickActions"), QStringLiteral("Quick actions")},
        {QStringLiteral("settings.quickResetAudio"), QStringLiteral("Reset Audio Only")},
        {QStringLiteral("settings.quickResetWaveform"), QStringLiteral("Reset Waveform Only")},
        {QStringLiteral("settings.quickResetAll"), QStringLiteral("Reset All to Defaults")},
        {QStringLiteral("settings.resetConfirmTitleAudio"), QStringLiteral("Confirm Audio Reset")},
        {QStringLiteral("settings.resetConfirmTitleWaveform"), QStringLiteral("Confirm Waveform Reset")},
        {QStringLiteral("settings.resetConfirmTitleAll"), QStringLiteral("Confirm Full Reset")},
        {QStringLiteral("settings.resetConfirmTitleTheme"), QStringLiteral("Confirm Theme Reset")},
        {QStringLiteral("settings.resetConfirmMessage"),
         QStringLiteral("Review the following changes before applying reset:")},
        {QStringLiteral("settings.resetConfirmNoChanges"), QStringLiteral("No settings need to be changed.")},
        {QStringLiteral("settings.resetConfirmApply"), QStringLiteral("Apply Reset")},
        {QStringLiteral("settings.resetConfirmCancel"), QStringLiteral("Cancel")},
        {QStringLiteral("settings.valueEnabled"), QStringLiteral("Enabled")},
        {QStringLiteral("settings.valueDisabled"), QStringLiteral("Disabled")},
        {QStringLiteral("settings.valueSystemDefault"), QStringLiteral("System default")},
        {QStringLiteral("settings.waveformHeight"), QStringLiteral("Waveform Height:")},
        {QStringLiteral("settings.compactWaveformHeight"), QStringLiteral("Compact Waveform Height:")},
        {QStringLiteral("settings.waveformZoomHintsVisible"), QStringLiteral("Show waveform zoom hints")},
        {QStringLiteral("settings.waveformZoomHintsDescription"),
         QStringLiteral("Display the zoom/help badge while waveform is zoomed")},
        {QStringLiteral("settings.waveformCueOverlayEnabled"), QStringLiteral("Show CUE segments on waveform")},
        {QStringLiteral("settings.waveformCueOverlayEnabledDescription"),
         QStringLiteral("Render CUE track regions over waveform for quick navigation context")},
        {QStringLiteral("settings.waveformCueLabelsVisible"), QStringLiteral("Show CUE segment labels")},
        {QStringLiteral("settings.waveformCueLabelsVisibleDescription"),
         QStringLiteral("Show CUE track title and duration inside waveform segments")},
        {QStringLiteral("settings.waveformCueAutoHideOnZoom"), QStringLiteral("Hide CUE segments while zoomed")},
        {QStringLiteral("settings.waveformCueAutoHideOnZoomDescription"),
         QStringLiteral("Automatically hide CUE overlays during waveform zoom and quick scrub")},
        {QStringLiteral("settings.speed"), QStringLiteral("Speed:")},
        {QStringLiteral("settings.resetSpeed"), QStringLiteral("Reset speed")},
        {QStringLiteral("settings.showSpeedPitch"), QStringLiteral("Show Speed/Pitch controls")},
        {QStringLiteral("settings.showSpeedPitchDescription"), QStringLiteral("Display speed and pitch sliders in the control bar")},
        {QStringLiteral("settings.reversePlayback"), QStringLiteral("Reverse track playback")},
        {QStringLiteral("settings.reversePlaybackDescription"),
         QStringLiteral("Play each track from end to start while preserving current speed")},
        {QStringLiteral("settings.reversePlaybackUnavailableDescription"),
         QStringLiteral("Reverse playback is unavailable for this backend or runtime profile.")},
        {QStringLiteral("settings.pitchUnavailableDescription"),
         QStringLiteral("Pitch shifting is unavailable for tracker modules in the current OpenMPT path.")},
        {QStringLiteral("settings.speedUnavailableDescription"),
         QStringLiteral("Playback speed changes are unavailable for tracker modules in the current OpenMPT path.")},
        {QStringLiteral("settings.audioQualityProfile"), QStringLiteral("Audio quality profile")},
        {QStringLiteral("settings.audioQualityProfileDescription"),
         QStringLiteral("Choose processing character: Standard is balanced, Hi-Fi is cleaner, Studio is the most transparent.")},
        {QStringLiteral("settings.audioQualityProfileUnavailableDescription"),
         QStringLiteral("Audio quality profiles stay on Standard for tracker modules because the OpenMPT path does not use the GStreamer mastering chain.")},
        {QStringLiteral("settings.audioQualityStandard"), QStringLiteral("Standard")},
        {QStringLiteral("settings.audioQualityHiFi"), QStringLiteral("Hi-Fi")},
        {QStringLiteral("settings.audioQualityStudio"), QStringLiteral("Studio")},
        {QStringLiteral("settings.dynamicSpectrum"), QStringLiteral("Dynamic Spectrum Analyzer")},
        {QStringLiteral("settings.dynamicSpectrumDescription"), QStringLiteral("Real-time audio visualization (may affect performance)")},
        {QStringLiteral("settings.dynamicSpectrumUnavailableDescription"),
         QStringLiteral("Spectrum analyzer is unavailable for the current backend or missing runtime support.")},
        {QStringLiteral("settings.deterministicShuffle"), QStringLiteral("Deterministic shuffle order")},
        {QStringLiteral("settings.deterministicShuffleDescription"),
         QStringLiteral("Use a fixed seed so shuffle order can be reproduced")},
        {QStringLiteral("settings.repeatableShuffle"), QStringLiteral("Repeatable across cycles")},
        {QStringLiteral("settings.repeatableShuffleDescription"),
         QStringLiteral("When disabled, each new shuffle cycle advances generation for a new order")},
        {QStringLiteral("settings.shuffleSeedDependencyHint"),
         QStringLiteral("Available only when deterministic shuffle order is enabled.")},
        {QStringLiteral("settings.repeatableShuffleDependencyHint"),
         QStringLiteral("Enable deterministic shuffle order to manage repeatability.")},
        {QStringLiteral("settings.shuffleSeed"), QStringLiteral("Shuffle seed:")},
        {QStringLiteral("settings.regenerateSeed"), QStringLiteral("Regenerate")},
        {QStringLiteral("settings.waveformCueLabelsDependencyHint"),
         QStringLiteral("Enable CUE segment overlay to configure labels.")},
        {QStringLiteral("settings.waveformCueAutoHideDependencyHint"),
         QStringLiteral("Enable CUE segment overlay to configure auto-hide.")},
        {QStringLiteral("player.speed"), QStringLiteral("Speed")},
        {QStringLiteral("player.pitch"), QStringLiteral("Pitch")},
        // InfoSidebar
        {QStringLiteral("sidebar.spectrumAnalyzer"), QStringLiteral("SPECTRUM ANALYZER")},
        {QStringLiteral("sidebar.technicalSpecs"), QStringLiteral("TECHNICAL SPECS")},
        {QStringLiteral("sidebar.engine"), QStringLiteral("Engine:")},
        {QStringLiteral("sidebar.engineValue"), QStringLiteral("FluxAudio")},
        {QStringLiteral("sidebar.codec"), QStringLiteral("Codec:")},
        {QStringLiteral("sidebar.sampleRate"), QStringLiteral("Sample Rate:")},
        {QStringLiteral("sidebar.bitrate"), QStringLiteral("Bitrate:")},
        {QStringLiteral("sidebar.bitDepth"), QStringLiteral("Bit Depth:")},
        {QStringLiteral("sidebar.bpm"), QStringLiteral("Beats Per Minute:")},
        {QStringLiteral("sidebar.trackerModule"), QStringLiteral("TRACKER MODULE")},
        {QStringLiteral("sidebar.trackerType"), QStringLiteral("Tracker:")},
        {QStringLiteral("sidebar.trackerChannels"), QStringLiteral("Channels:")},
        {QStringLiteral("sidebar.trackerPatterns"), QStringLiteral("Patterns:")},
        {QStringLiteral("sidebar.trackerInstruments"), QStringLiteral("Instruments:")},
        {QStringLiteral("sidebar.buffer"), QStringLiteral("Buffer:")},
        {QStringLiteral("sidebar.bufferValue"), QStringLiteral("512 MB Pre-loaded")},
        {QStringLiteral("sidebar.albumArt"), QStringLiteral("ALBUM ART")},
        {QStringLiteral("sidebar.unknown"), QStringLiteral("Unknown")},
        {QStringLiteral("sidebar.lossless"), QStringLiteral("Lossless")},
        {QStringLiteral("sidebar.bitPcm"), QStringLiteral("-bit PCM")},
        // ControlBar
        {QStringLiteral("player.mute"), QStringLiteral("Mute")},
        {QStringLiteral("player.maxVolume"), QStringLiteral("Max volume")},
        {QStringLiteral("player.equalizer"), QStringLiteral("Equalizer")},
        {QStringLiteral("player.equalizerUnavailable"), QStringLiteral("Equalizer unavailable")},
        {QStringLiteral("queue.open"), QStringLiteral("Open Up Next Panel")},
        {QStringLiteral("queue.upNext"), QStringLiteral("Up Next")},
        {QStringLiteral("queue.clear"), QStringLiteral("Clear Queue")},
        {QStringLiteral("queue.empty"), QStringLiteral("Queue is empty")},
        {QStringLiteral("equalizer.title"), QStringLiteral("Equalizer")},
        {QStringLiteral("equalizer.subtitle"), QStringLiteral("Parametric EQ (equalizer-nbands)")},
        {QStringLiteral("equalizer.reset"), QStringLiteral("Reset")},
        {QStringLiteral("equalizer.unavailable"), QStringLiteral("Equalizer plugin is unavailable")},
        {QStringLiteral("equalizer.unavailableDescription"), QStringLiteral("Install GStreamer 'equalizer' plugin (equalizer-nbands) to enable EQ.")},
        {QStringLiteral("equalizer.preset"), QStringLiteral("Preset")},
        {QStringLiteral("equalizer.applyPreset"), QStringLiteral("Apply")},
        {QStringLiteral("equalizer.presetFlat"), QStringLiteral("Flat")},
        {QStringLiteral("equalizer.presetBassBoost"), QStringLiteral("Bass Boost")},
        {QStringLiteral("equalizer.presetVocal"), QStringLiteral("Vocal")},
        {QStringLiteral("equalizer.presetHighBoost"), QStringLiteral("High Boost")},
        {QStringLiteral("equalizer.presetRock"), QStringLiteral("Rock")},
        {QStringLiteral("equalizer.presetPop"), QStringLiteral("Pop")},
        {QStringLiteral("equalizer.presetJazz"), QStringLiteral("Jazz")},
        {QStringLiteral("equalizer.presetElectronic"), QStringLiteral("Electronic")},
        {QStringLiteral("equalizer.presetClassical"), QStringLiteral("Classical")},
        {QStringLiteral("equalizer.builtIn"), QStringLiteral("Built-in")},
        {QStringLiteral("equalizer.user"), QStringLiteral("User")},
        {QStringLiteral("equalizer.userEmpty"), QStringLiteral("No user presets yet.")},
        {QStringLiteral("equalizer.saveAs"), QStringLiteral("Save as preset")},
        {QStringLiteral("equalizer.rename"), QStringLiteral("Rename")},
        {QStringLiteral("equalizer.delete"), QStringLiteral("Delete")},
        {QStringLiteral("equalizer.import"), QStringLiteral("Import")},
        {QStringLiteral("equalizer.export"), QStringLiteral("Export")},
        {QStringLiteral("equalizer.portalTitleImport"), QStringLiteral("Import EQ Presets (JSON)")},
        {QStringLiteral("equalizer.portalTitleExport"), QStringLiteral("Export EQ Presets (JSON)")},
        {QStringLiteral("equalizer.exportUser"), QStringLiteral("Export user presets")},
        {QStringLiteral("equalizer.exportBundle"), QStringLiteral("Export full bundle")},
        {QStringLiteral("equalizer.namePlaceholder"), QStringLiteral("Preset name")},
        {QStringLiteral("equalizer.nameRequired"), QStringLiteral("Preset name is required.")},
        {QStringLiteral("equalizer.errorPresetIdRequired"), QStringLiteral("Preset id is required for export.")},
        {QStringLiteral("equalizer.errorInvalidImportPath"), QStringLiteral("Invalid preset import file path.")},
        {QStringLiteral("equalizer.errorInvalidExportPath"), QStringLiteral("Invalid preset export file path.")},
        {QStringLiteral("equalizer.errorInvalidExportMode"), QStringLiteral("Invalid preset export mode.")},
        {QStringLiteral("equalizer.errorExportFailed"), QStringLiteral("Failed to export EQ presets.")},
        {QStringLiteral("equalizer.mergeKeepBoth"), QStringLiteral("Merge: keep both")},
        {QStringLiteral("equalizer.mergeReplace"), QStringLiteral("Merge: replace existing")},
        {QStringLiteral("equalizer.deleteConfirmTitle"), QStringLiteral("Delete preset")},
        {QStringLiteral("equalizer.deleteConfirmMessage"),
         QStringLiteral("Delete preset \"%1\"? This action cannot be undone.")},
        {QStringLiteral("equalizer.exportDone"), QStringLiteral("Preset export complete")},
        {QStringLiteral("equalizer.exportFailed"), QStringLiteral("Preset export failed")},
        {QStringLiteral("equalizer.exportPathLabel"), QStringLiteral("Path")},
        {QStringLiteral("equalizer.exportCountLabel"), QStringLiteral("Presets")},
        {QStringLiteral("equalizer.hotkeysLegend"),
         QStringLiteral("Shortcuts: Open %1, Import %2, Export %3")},
        {QStringLiteral("equalizer.shortcutImportTooltip"),
         QStringLiteral("Import presets (%1)")},
        {QStringLiteral("equalizer.shortcutExportTooltip"),
         QStringLiteral("Export selected preset (%1)")},
        {QStringLiteral("equalizer.importDone"), QStringLiteral("Preset import complete")},
        {QStringLiteral("equalizer.importPartial"), QStringLiteral("Preset import completed with issues")},
        {QStringLiteral("equalizer.importFailed"), QStringLiteral("Preset import failed")},
        {QStringLiteral("equalizer.importSummary"), QStringLiteral("Import summary")},
        {QStringLiteral("equalizer.importMergePolicy"), QStringLiteral("Merge policy")},
        {QStringLiteral("equalizer.importImported"), QStringLiteral("Imported")},
        {QStringLiteral("equalizer.importReplaced"), QStringLiteral("Replaced")},
        {QStringLiteral("equalizer.importSkipped"), QStringLiteral("Skipped")},
        {QStringLiteral("equalizer.importIssues"), QStringLiteral("Issues")},
        {QStringLiteral("xspf.importDone"), QStringLiteral("XSPF import complete")},
        {QStringLiteral("xspf.importPartial"), QStringLiteral("XSPF import completed with issues")},
        {QStringLiteral("xspf.importFailed"), QStringLiteral("XSPF import failed")},
        {QStringLiteral("xspf.importSummary"), QStringLiteral("Import summary")},
        {QStringLiteral("xspf.importSource"), QStringLiteral("Playlist: %1")},
        {QStringLiteral("xspf.importAdded"), QStringLiteral("Added: %1")},
        {QStringLiteral("xspf.importSkipped"), QStringLiteral("Skipped: %1")},
        {QStringLiteral("xspf.importUnknownSource"), QStringLiteral("unknown source")},
        {QStringLiteral("equalizer.statusSuccess"), QStringLiteral("Success")},
        {QStringLiteral("equalizer.statusError"), QStringLiteral("Error")},
        {QStringLiteral("equalizer.statusInfo"), QStringLiteral("Info")},
        {QStringLiteral("equalizer.statusDetails"), QStringLiteral("Details")},
        // CompactSkin
        {QStringLiteral("compact.hidePlaylist"), QStringLiteral("Hide Playlist")},
        {QStringLiteral("compact.showPlaylist"), QStringLiteral("Show Playlist")},
        // HeaderBar menu items
        {QStringLiteral("menu.file"), QStringLiteral("File")},
        {QStringLiteral("menu.edit"), QStringLiteral("Edit")},
        {QStringLiteral("menu.view"), QStringLiteral("View")},
        {QStringLiteral("menu.playback"), QStringLiteral("Playback")},
        {QStringLiteral("menu.library"), QStringLiteral("Library")},
        {QStringLiteral("menu.help"), QStringLiteral("Help")},
        {QStringLiteral("menu.openFiles"), QStringLiteral("Open Files...")},
        {QStringLiteral("menu.addFolder"), QStringLiteral("Add Folder...")},
        {QStringLiteral("menu.audioConverter"), QStringLiteral("Audio Converter...")},
        {QStringLiteral("menu.exportPlaylist"), QStringLiteral("Export Playlist...")},
        {QStringLiteral("menu.clearPlaylist"), QStringLiteral("Clear Playlist")},
        {QStringLiteral("menu.quit"), QStringLiteral("Quit")},
        {QStringLiteral("menu.find"), QStringLiteral("Find")},
        {QStringLiteral("menu.selectAll"), QStringLiteral("Select All")},
        {QStringLiteral("menu.clearSelection"), QStringLiteral("Clear Selection")},
        {QStringLiteral("menu.viewCollectionsPanel"), QStringLiteral("Collections Panel")},
        {QStringLiteral("menu.viewInfoSidebar"), QStringLiteral("Info Sidebar")},
        {QStringLiteral("menu.viewSpeedPitch"), QStringLiteral("Speed/Pitch Controls")},
        {QStringLiteral("menu.profilerOverlay"), QStringLiteral("Profiler Overlay")},
        {QStringLiteral("menu.profilerEnable"), QStringLiteral("Enable Profiler")},
        {QStringLiteral("menu.profilerReset"), QStringLiteral("Reset Profiler")},
        {QStringLiteral("menu.profilerExportJson"), QStringLiteral("Export Profiler JSON")},
        {QStringLiteral("menu.profilerExportCsv"), QStringLiteral("Export Profiler CSV")},
        {QStringLiteral("menu.profilerExportBundle"), QStringLiteral("Export Profiler Bundle")},
        {QStringLiteral("menu.seekBack5"), QStringLiteral("Seek -5s")},
        {QStringLiteral("menu.seekForward5"), QStringLiteral("Seek +5s")},
        {QStringLiteral("menu.repeatMode"), QStringLiteral("Repeat")},
        {QStringLiteral("menu.newEmptyPlaylist"), QStringLiteral("New Empty Playlist")},
        // Help
        {QStringLiteral("help.about"), QStringLiteral("About")},
        {QStringLiteral("help.shortcuts"), QStringLiteral("Keyboard Shortcuts")},
        {QStringLiteral("help.aboutDialogTitle"), QStringLiteral("About WaveFlux")},
        {QStringLiteral("help.aboutAppName"), QStringLiteral("WaveFlux")},
        {QStringLiteral("help.aboutVersionLabel"), QStringLiteral("Version:")},
        {QStringLiteral("help.aboutVersionValue"), QStringLiteral("1.2")},
        {QStringLiteral("help.aboutDescription"),
         QStringLiteral("WaveFlux is a focused desktop audio player for local libraries and internet streams, with waveform visualization, queue control, and precise playback tools.")},
        {QStringLiteral("help.aboutAuthorLabel"), QStringLiteral("Author:")},
        {QStringLiteral("help.aboutAuthorName"), QStringLiteral("leocallidus")},
        {QStringLiteral("help.aboutAuthorUrl"), QStringLiteral("https://github.com/leocallidus")},
        {QStringLiteral("help.aboutYearLabel"), QStringLiteral("Created:")},
        {QStringLiteral("help.aboutYearValue"), QStringLiteral("2026")},
        {QStringLiteral("help.shortcutsDialogTitle"), QStringLiteral("Keyboard Shortcuts")},
        {QStringLiteral("help.shortcutsDialogSubtitle"), QStringLiteral("Reference for keyboard navigation and quick actions.")},
        {QStringLiteral("help.shortcutsColumnAction"), QStringLiteral("Action")},
        {QStringLiteral("help.shortcutsColumnKeys"), QStringLiteral("Shortcut")},
        {QStringLiteral("help.shortcutsColumnContext"), QStringLiteral("Context")},
        {QStringLiteral("help.shortcutsGroupPlayback"), QStringLiteral("Playback")},
        {QStringLiteral("help.shortcutsGroupNavigation"), QStringLiteral("Navigation & Interface")},
        {QStringLiteral("help.shortcutsGroupPlaylist"), QStringLiteral("Playlist & Library")},
        {QStringLiteral("help.shortcutsGroupProfiler"), QStringLiteral("Profiler & Service")},
        {QStringLiteral("help.shortcutsContextGlobal"), QStringLiteral("Global")},
        {QStringLiteral("help.shortcutsContextMainWindow"), QStringLiteral("Main Window")},
        {QStringLiteral("help.shortcutsContextPlaylist"), QStringLiteral("Playlist")},
        {QStringLiteral("help.shortcutsContextDialog"), QStringLiteral("Dialog")},
        {QStringLiteral("header.searchPlaceholder"), QStringLiteral("Search... title: artist: album: path:")},
        {QStringLiteral("header.searchManualPlaceholder"),
         QStringLiteral("Enter a search query and press Enter or click the magnifier")},
        {QStringLiteral("header.quickFilters"), QStringLiteral("Quick Filters")},
        {QStringLiteral("header.filterAllFields"), QStringLiteral("All Fields")},
        {QStringLiteral("header.filterTitle"), QStringLiteral("Title")},
        {QStringLiteral("header.filterArtist"), QStringLiteral("Artist")},
        {QStringLiteral("header.filterAlbum"), QStringLiteral("Album")},
        {QStringLiteral("header.filterPath"), QStringLiteral("Path")},
        {QStringLiteral("header.filterLossless"), QStringLiteral("Lossless")},
        {QStringLiteral("header.filterHiRes"), QStringLiteral("Hi-Res")},
        {QStringLiteral("header.filterReset"), QStringLiteral("Reset Filters")},
        {QStringLiteral("header.menu"), QStringLiteral("Menu")},
        // PlaylistTable columns
        {QStringLiteral("table.title"), QStringLiteral("TITLE")},
        {QStringLiteral("table.artist"), QStringLiteral("ARTIST")},
        {QStringLiteral("table.album"), QStringLiteral("ALBUM")},
        {QStringLiteral("table.duration"), QStringLiteral("DURATION")},
        {QStringLiteral("table.bitrate"), QStringLiteral("BITRATE")},
        // Smart collections
        {QStringLiteral("collections.sectionTitle"), QStringLiteral("COLLECTIONS")},
        {QStringLiteral("collections.openPanel"), QStringLiteral("Open Collections")},
        {QStringLiteral("collections.currentPlaylist"), QStringLiteral("Current Playlist")},
        {QStringLiteral("playlists.sectionTitle"), QStringLiteral("PLAYLISTS")},
        {QStringLiteral("playlists.add"), QStringLiteral("Add playlist")},
        {QStringLiteral("playlists.saveCurrent"), QStringLiteral("Save current playlist")},
        {QStringLiteral("playlists.name"), QStringLiteral("Playlist name")},
        {QStringLiteral("playlists.namePlaceholder"), QStringLiteral("My playlist")},
        {QStringLiteral("playlists.save"), QStringLiteral("Save")},
        {QStringLiteral("playlists.nameRequired"), QStringLiteral("Playlist name is required.")},
        {QStringLiteral("playlists.saveChanges"), QStringLiteral("Save changes")},
        {QStringLiteral("playlists.empty"), QStringLiteral("No saved playlists yet.")},
        {QStringLiteral("playlists.emptyTracks"), QStringLiteral("No tracks in this playlist.")},
        {QStringLiteral("playlists.tracks"), QStringLiteral("Tracks")},
        {QStringLiteral("playlists.trackCount"), QStringLiteral("%1 tracks")},
        {QStringLiteral("playlists.edit"), QStringLiteral("Edit playlist")},
        {QStringLiteral("playlists.editTitle"), QStringLiteral("Edit Playlist")},
        {QStringLiteral("playlists.duplicate"), QStringLiteral("Duplicate playlist")},
        {QStringLiteral("playlists.moveUp"), QStringLiteral("Move up")},
        {QStringLiteral("playlists.moveDown"), QStringLiteral("Move down")},
        {QStringLiteral("playlists.removeTrack"), QStringLiteral("Remove track")},
        {QStringLiteral("playlists.copySuffix"), QStringLiteral(" (copy)")},
        {QStringLiteral("playlists.rename"), QStringLiteral("Rename playlist")},
        {QStringLiteral("playlists.renameTitle"), QStringLiteral("Rename Playlist")},
        {QStringLiteral("playlists.renameApply"), QStringLiteral("Rename")},
        {QStringLiteral("playlists.delete"), QStringLiteral("Delete playlist")},
        {QStringLiteral("playlists.deleteConfirmTitle"), QStringLiteral("Delete Playlist")},
        {QStringLiteral("playlists.deleteConfirmMessage"),
         QStringLiteral("Delete playlist \"%1\"? This cannot be undone.")},
        {QStringLiteral("playlists.errorTitle"), QStringLiteral("Playlist Error")},
        {QStringLiteral("collections.create"), QStringLiteral("Create")},
        {QStringLiteral("collections.delete"), QStringLiteral("Delete")},
        {QStringLiteral("collections.deleteConfirmTitle"), QStringLiteral("Delete Collection")},
        {QStringLiteral("collections.deleteConfirmMessage"),
         QStringLiteral("Delete smart collection \"%1\"? This cannot be undone.")},
        {QStringLiteral("collections.disabled"), QStringLiteral("Collections are unavailable (SQLite library disabled).")},
        {QStringLiteral("collections.empty"), QStringLiteral("No smart collections yet.")},
        {QStringLiteral("collections.emptyTracks"), QStringLiteral("No tracks in this collection.")},
        {QStringLiteral("collections.applyErrorTitle"), QStringLiteral("Collections Error")},
        {QStringLiteral("collections.createDialogTitle"), QStringLiteral("Create Smart Collection")},
        {QStringLiteral("collections.template"), QStringLiteral("Template")},
        {QStringLiteral("collections.name"), QStringLiteral("Name")},
        {QStringLiteral("collections.namePlaceholder"), QStringLiteral("Collection name")},
        {QStringLiteral("collections.logic"), QStringLiteral("Logic")},
        {QStringLiteral("collections.logicAll"), QStringLiteral("Match all rules")},
        {QStringLiteral("collections.logicAny"), QStringLiteral("Match any rule")},
        {QStringLiteral("collections.rules"), QStringLiteral("Rules")},
        {QStringLiteral("collections.addRule"), QStringLiteral("Create Rule")},
        {QStringLiteral("collections.value"), QStringLiteral("Value")},
        {QStringLiteral("collections.sort"), QStringLiteral("Sort")},
        {QStringLiteral("collections.sortAsc"), QStringLiteral("Ascending")},
        {QStringLiteral("collections.sortDesc"), QStringLiteral("Descending")},
        {QStringLiteral("collections.limit"), QStringLiteral("Limit")},
        {QStringLiteral("collections.limitHint"), QStringLiteral("0 = unlimited")},
        {QStringLiteral("collections.cancel"), QStringLiteral("Cancel")},
        {QStringLiteral("collections.nameRequired"), QStringLiteral("Collection name is required.")},
        {QStringLiteral("collections.rulesRequired"), QStringLiteral("Add at least one valid rule.")},
        {QStringLiteral("collections.createFailed"), QStringLiteral("Failed to create smart collection.")},
        {QStringLiteral("collections.enabled"), QStringLiteral("Enabled")},
        {QStringLiteral("collections.pinned"), QStringLiteral("Pinned")},
        {QStringLiteral("collections.boolTrue"), QStringLiteral("Yes")},
        {QStringLiteral("collections.boolFalse"), QStringLiteral("No")},
        {QStringLiteral("collections.templateNone"), QStringLiteral("Template: Empty")},
        {QStringLiteral("collections.templateRecentlyAdded"), QStringLiteral("Template: Recently Added")},
        {QStringLiteral("collections.templateFrequentlyPlayed"), QStringLiteral("Template: Frequently Played")},
        {QStringLiteral("collections.templateNeverPlayed"), QStringLiteral("Template: Never Played")},
        {QStringLiteral("collections.templateHiRes"), QStringLiteral("Template: Hi-Res")},
        {QStringLiteral("collections.fieldAllText"), QStringLiteral("Any text field")},
        {QStringLiteral("collections.fieldTitle"), QStringLiteral("Title")},
        {QStringLiteral("collections.fieldArtist"), QStringLiteral("Artist")},
        {QStringLiteral("collections.fieldAlbum"), QStringLiteral("Album")},
        {QStringLiteral("collections.fieldPath"), QStringLiteral("Path")},
        {QStringLiteral("collections.fieldFormat"), QStringLiteral("Format")},
        {QStringLiteral("collections.fieldAddedDays"), QStringLiteral("Added days ago")},
        {QStringLiteral("collections.fieldPlayCount"), QStringLiteral("Play count")},
        {QStringLiteral("collections.fieldSkipCount"), QStringLiteral("Skip count")},
        {QStringLiteral("collections.fieldRating"), QStringLiteral("Rating")},
        {QStringLiteral("collections.fieldSampleRate"), QStringLiteral("Sample rate")},
        {QStringLiteral("collections.fieldBitDepth"), QStringLiteral("Bit depth")},
        {QStringLiteral("collections.fieldLastPlayedDays"), QStringLiteral("Last played days ago")},
        {QStringLiteral("collections.fieldFavorite"), QStringLiteral("Favorite")},
        {QStringLiteral("collections.fieldAddedAt"), QStringLiteral("Added at")},
        {QStringLiteral("collections.fieldLastPlayedAt"), QStringLiteral("Last played at")},
        {QStringLiteral("collections.opMatch"), QStringLiteral("match")},
        {QStringLiteral("collections.opContains"), QStringLiteral("contains")},
        {QStringLiteral("collections.opStartsWith"), QStringLiteral("starts with")},
        {QStringLiteral("collections.opEq"), QStringLiteral("=")},
        {QStringLiteral("collections.opNe"), QStringLiteral("!=")},
        {QStringLiteral("collections.opGe"), QStringLiteral(">=")},
        {QStringLiteral("collections.opLe"), QStringLiteral("<=")},
        {QStringLiteral("collections.opGt"), QStringLiteral(">")},
        {QStringLiteral("collections.opLt"), QStringLiteral("<")}
    };
    return texts;
}

const QHash<QString, QString> &russianTexts()
{
    static const QHash<QString, QString> texts = {
        {QStringLiteral("app.title"), QStringLiteral("WaveFlux")},
        {QStringLiteral("main.openFiles"), QStringLiteral("Открыть файлы")},
        {QStringLiteral("main.addFolder"), QStringLiteral("Добавить папку")},
        {QStringLiteral("ytDlpImport.toolbarButton"), QStringLiteral("URL")},
        {QStringLiteral("main.exportPlaylist"), QStringLiteral("Экспорт плейлиста")},
        {QStringLiteral("main.clearPlaylist"), QStringLiteral("Очистить плейлист")},
        {QStringLiteral("main.nowPlaying"), QStringLiteral("Сейчас играет")},
        {QStringLiteral("main.settings"), QStringLiteral("Настройки")},
        {QStringLiteral("main.enterFullscreen"), QStringLiteral("Полный экран")},
        {QStringLiteral("main.exitFullscreen"), QStringLiteral("Выйти из полноэкранного режима")},
        {QStringLiteral("main.hideOverlay"), QStringLiteral("Скрыть панель")},
        {QStringLiteral("main.showOverlay"), QStringLiteral("Показать панель")},
        {QStringLiteral("main.fullscreenHint"), QStringLiteral("F11 переключает полноэкранный режим и панель")},
        {QStringLiteral("main.noTrack"), QStringLiteral("Нет трека")},
        {QStringLiteral("main.unknownArtist"), QStringLiteral("Неизвестный исполнитель")},
        {QStringLiteral("main.playbackError"), QStringLiteral("Ошибка воспроизведения")},
        {QStringLiteral("main.waveformError"), QStringLiteral("Ошибка волны")},
        {QStringLiteral("main.filePickerError"), QStringLiteral("Ошибка выбора файла")},
        {QStringLiteral("main.export"), QStringLiteral("Экспорт")},
        {QStringLiteral("main.exportError"), QStringLiteral("Ошибка экспорта")},
        {QStringLiteral("main.exportComplete"), QStringLiteral("Экспорт завершен")},
        {QStringLiteral("main.lastError"), QStringLiteral("Последняя ошибка: ")},
        {QStringLiteral("error.fileUnavailable"), QStringLiteral("Файл недоступен: %1")},
        {QStringLiteral("error.midiUnsupported"), QStringLiteral("Воспроизведение MIDI пока не поддерживается: %1")},
        {QStringLiteral("error.trackerUnsupported"),
         QStringLiteral("Формат tracker-модуля пока не поддерживается в матрице OpenMPT WaveFlux: %1")},
        {QStringLiteral("error.seekUnavailable"), QStringLiteral("Перемотка недоступна для текущего трека.")},
        {QStringLiteral("error.failedInitializeAudioEngine"), QStringLiteral("Не удалось инициализировать аудиодвижок")},
        {QStringLiteral("error.failedStartPlayback"), QStringLiteral("Не удалось начать воспроизведение")},
        {QStringLiteral("error.failedSetPlaybackRate"),
         QStringLiteral("Не удалось установить скорость воспроизведения %1x (эффективно %2x)")},
        {QStringLiteral("error.failedResolvePlaybackUri"), QStringLiteral("Не удалось определить URI воспроизведения для: %1")},
        {QStringLiteral("error.trackLoadingTimedOut"), QStringLiteral("Превышено время ожидания загрузки трека")},
        {QStringLiteral("error.playbackStoppedRepeatedFailures"),
         QStringLiteral("Воспроизведение остановлено после повторяющихся ошибок загрузки.")},
        {QStringLiteral("error.playlistExportNotInitialized"), QStringLiteral("Сервис экспорта плейлиста не инициализирован.")},
        {QStringLiteral("error.noTracksSelected"), QStringLiteral("Не выбраны треки.")},
        {QStringLiteral("error.playlistEmpty"), QStringLiteral("Плейлист пуст.")},
        {QStringLiteral("error.noOutputPathSelected"), QStringLiteral("Не выбран путь для сохранения.")},
        {QStringLiteral("error.failedOpenOutputFileWriting"), QStringLiteral("Не удалось открыть выходной файл для записи.")},
        {QStringLiteral("status.playlistExportedTo"), QStringLiteral("Плейлист экспортирован в %1")},
        {QStringLiteral("audioConverter.errorTrackerRenderPath"), QStringLiteral("Не удалось подготовить временный путь для рендера tracker-модуля.")},
        {QStringLiteral("audioConverter.errorTrackerRenderFailed"), QStringLiteral("Не удалось выполнить рендер tracker-модуля для конвертации.")},
        {QStringLiteral("audioConverter.errorSourcePathInvalid"), QStringLiteral("Некорректный путь к исходному файлу.")},
        {QStringLiteral("audioConverter.errorFormatUnsupported"), QStringLiteral("Выбранный выходной формат не поддерживается.")},
        {QStringLiteral("audioConverter.errorMissingPlugin"),
         QStringLiteral("Недоступен необходимый GStreamer-плагин: %1. Установите недостающий кодировщик или muxer и попробуйте снова.")},
        {QStringLiteral("audioConverter.errorTempOutputPath"), QStringLiteral("Не удалось подготовить временный путь для выходного файла.")},
        {QStringLiteral("audioConverter.errorCreatePipeline"), QStringLiteral("Не удалось создать конвейер GStreamer для конвертации.")},
        {QStringLiteral("audioConverter.errorOutputAlreadyExists"), QStringLiteral("Выходной файл уже существует: %1")},
        {QStringLiteral("audioConverter.errorReplaceExistingOutput"), QStringLiteral("Не удалось заменить существующий выходной файл: %1 (%2)")},
        {QStringLiteral("audioConverter.errorFinalizeOutput"), QStringLiteral("Не удалось завершить запись сконвертированного файла: %1 (%2)")},
        {QStringLiteral("batchConverter.started"), QStringLiteral("Пакетная конвертация запущена.")},
        {QStringLiteral("batchConverter.restoredDraft"), QStringLiteral("Черновик пакетной конвертации восстановлен из предыдущего сеанса.")},
        {QStringLiteral("batchConverter.alreadyRunning"), QStringLiteral("Пакетная конвертация уже выполняется.")},
        {QStringLiteral("batchConverter.itemStarting"), QStringLiteral("Подготовка конвертации...")},
        {QStringLiteral("batchConverter.itemConverting"), QStringLiteral("Конвертация %1")},
        {QStringLiteral("batchConverter.workerUnavailable"), QStringLiteral("Рабочий конвертер аудио недоступен.")},
        {QStringLiteral("batchConverter.startFailed"), QStringLiteral("Не удалось запустить конвертацию.")},
        {QStringLiteral("error.playlistNameEmpty"), QStringLiteral("Имя плейлиста пустое")},
        {QStringLiteral("error.playlistStorageLimitReached"), QStringLiteral("Достигнут лимит сохранённых плейлистов")},
        {QStringLiteral("error.failedLocateSavedPlaylist"), QStringLiteral("Не удалось найти сохранённый плейлист")},
        {QStringLiteral("error.playlistNotFound"), QStringLiteral("Плейлист не найден")},
        {QStringLiteral("error.playlistNameAlreadyExists"), QStringLiteral("Плейлист с таким именем уже существует")},
        {QStringLiteral("error.failedLocateDuplicatedPlaylist"), QStringLiteral("Не удалось найти дубликат плейлиста")},
        {QStringLiteral("error.failedOpenPlaylistsStorage"), QStringLiteral("Не удалось открыть хранилище плейлистов")},
        {QStringLiteral("error.failedParsePlaylistsStorage"), QStringLiteral("Не удалось разобрать хранилище плейлистов")},
        {QStringLiteral("error.failedOpenPlaylistsStorageWriting"), QStringLiteral("Не удалось открыть хранилище плейлистов для записи")},
        {QStringLiteral("error.failedPersistPlaylistsStorage"), QStringLiteral("Не удалось сохранить хранилище плейлистов")},
        {QStringLiteral("error.presetNameEmpty"), QStringLiteral("Имя пресета пустое")},
        {QStringLiteral("error.userPresetNotFound"), QStringLiteral("Пользовательский пресет не найден")},
        {QStringLiteral("error.invalidUserPresetAtIndex"), QStringLiteral("Некорректный пользовательский пресет с индексом %1")},
        {QStringLiteral("error.invalidUserPresetAtIndexWithReason"), QStringLiteral("Некорректный пользовательский пресет с индексом %1: %2")},
        {QStringLiteral("error.builtinPresetIdNotAllowed"), QStringLiteral("Встроенный идентификатор пресета нельзя использовать для пользовательского пресета")},
        {QStringLiteral("error.smartCollectionNotFound"), QStringLiteral("Умная коллекция не найдена")},
        {QStringLiteral("error.smartCollectionsDisabled"), QStringLiteral("Движок умных коллекций отключён")},
        {QStringLiteral("error.collectionNameEmpty"), QStringLiteral("Имя коллекции пустое")},
        {QStringLiteral("error.invalidSmartCollectionId"), QStringLiteral("Некорректный идентификатор умной коллекции")},
        {QStringLiteral("error.failedPruneContextPlaybackProgress"), QStringLiteral("Не удалось очистить прогресс воспроизведения контекстов")},
        {QStringLiteral("error.databaseConnectionNotOpen"), QStringLiteral("Соединение с базой данных не открыто")},
        {QStringLiteral("error.databasePathEmpty"), QStringLiteral("Путь к базе данных пустой")},
        {QStringLiteral("error.invalidOutputPointer"), QStringLiteral("Некорректный выходной указатель")},
        {QStringLiteral("error.failedReadSmartCollectionsCount"), QStringLiteral("Не удалось прочитать количество smart_collections")},
        {QStringLiteral("error.openFileManagerEmptyPath"), QStringLiteral("Невозможно открыть файловый менеджер: путь к файлу пуст.")},
        {QStringLiteral("error.openFileManagerLocalOnly"), QStringLiteral("Невозможно открыть файловый менеджер: поддерживаются только локальные файлы.")},
        {QStringLiteral("error.openFileManagerFileNotFound"), QStringLiteral("Невозможно открыть файловый менеджер: файл не найден.")},
        {QStringLiteral("error.openFileManagerSelectedFile"), QStringLiteral("Невозможно открыть файловый менеджер для выбранного файла.")},
        {QStringLiteral("error.moveToTrashLocalOnly"), QStringLiteral("Невозможно переместить файл в корзину: поддерживаются только локальные файлы.")},
        {QStringLiteral("error.moveToTrashFileNotFound"), QStringLiteral("Невозможно переместить файл в корзину: файл не найден.")},
        {QStringLiteral("error.moveToTrashFailed"),
         QStringLiteral("Невозможно переместить файл в корзину. Проверьте права доступа и поддержку корзины на этом диске.")},
        {QStringLiteral("error.openUrlInvalid"), QStringLiteral("Невозможно открыть URL: некорректный адрес.")},
        {QStringLiteral("error.openUrlDefaultBrowser"), QStringLiteral("Невозможно открыть URL в браузере по умолчанию.")},
        {QStringLiteral("error.filePickerRequestInProgress"), QStringLiteral("Запрос выбора файла уже выполняется.")},
        {QStringLiteral("error.xdgPortalUnavailable"), QStringLiteral("XDG portal недоступен: %1")},
        {QStringLiteral("error.failedOpenXdgPortalFilePicker"), QStringLiteral("Не удалось открыть диалог выбора файла через XDG portal: %1")},
        {QStringLiteral("error.xdgPortalInvalidRequestHandle"), QStringLiteral("XDG portal вернул некорректный дескриптор запроса.")},
        {QStringLiteral("error.failedSubscribeXdgPortalResponse"), QStringLiteral("Не удалось подписаться на ответ XDG portal.")},
        {QStringLiteral("error.xdgPortalFilePickerReturnedError"), QStringLiteral("XDG portal вернул ошибку выбора файла.")},
        {QStringLiteral("profiler.title"), QStringLiteral("Профайлер")},
        {QStringLiteral("profiler.modeFullscreen"), QStringLiteral("[полный экран]")},
        {QStringLiteral("profiler.modeWindowed"), QStringLiteral("[окно]")},
        {QStringLiteral("profiler.playlistTracks"), QStringLiteral("Треков в плейлисте")},
        {QStringLiteral("profiler.sceneFps"), QStringLiteral("FPS сцены")},
        {QStringLiteral("profiler.avg"), QStringLiteral("сред.")},
        {QStringLiteral("profiler.worst"), QStringLiteral("макс.")},
        {QStringLiteral("profiler.wavePaintsPerSec"), QStringLiteral("Отрисовок волны/с")},
        {QStringLiteral("profiler.repaintsFullPerSec"), QStringLiteral("Перерисовок/с полных")},
        {QStringLiteral("profiler.partial"), QStringLiteral("частичных")},
        {QStringLiteral("profiler.dirty"), QStringLiteral("грязная область")},
        {QStringLiteral("profiler.playlistDataPerSec"), QStringLiteral("Запросов данных плейлиста/с")},
        {QStringLiteral("profiler.searchQueriesPerSec"), QStringLiteral("Поисковых запросов/с")},
        {QStringLiteral("profiler.p95"), QStringLiteral("p95")},
        {QStringLiteral("profiler.searchBackendPerSec"), QStringLiteral("Поисковый backend/с sqlite")},
        {QStringLiteral("profiler.searchBackendFts"), QStringLiteral("fts")},
        {QStringLiteral("profiler.searchBackendLike"), QStringLiteral("like")},
        {QStringLiteral("profiler.searchBackendFail"), QStringLiteral("ошибки")},
        {QStringLiteral("profiler.memoryWorkingSet"), QStringLiteral("Память WS")},
        {QStringLiteral("profiler.private"), QStringLiteral("private")},
        {QStringLiteral("profiler.memoryCommit"), QStringLiteral("Память commit")},
        {QStringLiteral("profiler.peakWorkingSet"), QStringLiteral("пик WS")},
        {QStringLiteral("profiler.lastCheckpoint"), QStringLiteral("Последний checkpoint")},
        {QStringLiteral("profiler.lastExport"), QStringLiteral("Последний экспорт")},
        {QStringLiteral("profiler.notAvailable"), QStringLiteral("н/д")},
        {QStringLiteral("profiler.exportError"), QStringLiteral("Ошибка экспорта")},
        {QStringLiteral("profiler.hotkeys"), QStringLiteral("Горячие клавиши: Ctrl+Shift+P overlay, Ctrl+Shift+E enable, Ctrl+Shift+R reset")},
        {QStringLiteral("profiler.exportHotkeys"), QStringLiteral("Экспорт: Ctrl+Shift+J json, Ctrl+Shift+C csv, Ctrl+Shift+B bundle")},
        {QStringLiteral("waveform.zoomBadgeZoom"), QStringLiteral("Зум x%1")},
        {QStringLiteral("waveform.zoomBadgeQuick"), QStringLiteral("Быстро x%1")},
        {QStringLiteral("waveform.zoomBadgeQuickScrub"), QStringLiteral("Быстрый скраб x%1")},
        {QStringLiteral("waveform.zoomBadgeFineSeekHint"), QStringLiteral("Shift-перетаскивание: точный поиск")},
        {QStringLiteral("waveform.zoomBadgePanHint"), QStringLiteral("ПКМ-перетаскивание: панорама (инерция)")},
        {QStringLiteral("waveform.loadingPlaceholder"), QStringLiteral("Волна %1%")},
        {QStringLiteral("waveform.emptyPlaceholder"), QStringLiteral("Перетащите сюда аудиофайл")},
        {QStringLiteral("waveform.unsupportedPlaceholder"), QStringLiteral("Предпросмотр волны недоступен для этого источника")},
        {QStringLiteral("waveform.failedPlaceholder"), QStringLiteral("Не удалось построить волну")},
        {QStringLiteral("waveform.silentPlaceholder"), QStringLiteral("Волна пустая или беззвучная")},
        {QStringLiteral("main.hires"), QStringLiteral("HI-RES")},
        {QStringLiteral("main.lossless"), QStringLiteral("LOSSLESS")},
        {QStringLiteral("dialogs.openAudioFiles"), QStringLiteral("Открыть аудиофайлы и XSPF-плейлисты")},
        {QStringLiteral("dialogs.addFolder"), QStringLiteral("Добавить папку")},
        {QStringLiteral("dialogs.exportPlaylist"), QStringLiteral("Экспорт плейлиста")},
        {QStringLiteral("dialogs.audioFiles"),
         QStringLiteral("Аудиофайлы и XSPF-плейлисты (*.mp3 *.flac *.ogg *.wav *.aac *.m4a *.xspf)")},
        {QStringLiteral("dialogs.audioFilterLabel"), QStringLiteral("Аудиофайлы")},
        {QStringLiteral("dialogs.xspfFilterLabel"), QStringLiteral("XSPF-плейлисты")},
        {QStringLiteral("dialogs.allFilesFilterLabel"), QStringLiteral("Все файлы")},
        {QStringLiteral("dialogs.allFiles"), QStringLiteral("Все файлы (*)")},
        {QStringLiteral("dialogs.m3uPlaylist"), QStringLiteral("Плейлист M3U (*.m3u *.m3u8)")},
        {QStringLiteral("dialogs.xspfPlaylist"), QStringLiteral("Плейлист XSPF (*.xspf)")},
        {QStringLiteral("dialogs.jsonPlaylist"), QStringLiteral("Плейлист JSON (*.json)")},
        {QStringLiteral("dialogs.chooseWaveformColor"), QStringLiteral("Выберите цвет волны")},
        {QStringLiteral("dialogs.chooseProgressColor"), QStringLiteral("Выберите цвет прогресса")},
        {QStringLiteral("dialogs.chooseAccentColor"), QStringLiteral("Выберите акцентный цвет")},
        {QStringLiteral("settings.title"), QStringLiteral("Настройки")},
        {QStringLiteral("settings.appearance"), QStringLiteral("Внешний вид")},
        {QStringLiteral("settings.darkMode"), QStringLiteral("Темная тема")},
        {QStringLiteral("settings.sidebarVisible"), QStringLiteral("Показывать правую панель")},
        {QStringLiteral("settings.sidebarDescription"),
         QStringLiteral("Панель автоматически скрывается на узких окнах (<900px)")},
        {QStringLiteral("settings.collectionsSidebarVisible"), QStringLiteral("Показывать панель коллекций")},
        {QStringLiteral("settings.collectionsSidebarDescription"),
         QStringLiteral("Левая панель умных коллекций в обычном скине")},
        {QStringLiteral("settings.theme"), QStringLiteral("Тема:")},
        {QStringLiteral("settings.waveformColor"), QStringLiteral("Цвет волны:")},
        {QStringLiteral("settings.progressColor"), QStringLiteral("Цвет прогресса:")},
        {QStringLiteral("settings.accentColor"), QStringLiteral("Акцентный цвет:")},
        {QStringLiteral("settings.language"), QStringLiteral("Язык:")},
        {QStringLiteral("settings.languageAuto"), QStringLiteral("Авто (системный)")},
        {QStringLiteral("settings.languageEnglish"), QStringLiteral("Английский")},
        {QStringLiteral("settings.languageRussian"), QStringLiteral("Русский")},
        {QStringLiteral("settings.tray"), QStringLiteral("Системный трей:")},
        {QStringLiteral("settings.system"), QStringLiteral("Система")},
        {QStringLiteral("settings.trayEnabled"), QStringLiteral("Включить интеграцию с треем")},
        {QStringLiteral("settings.trayDescription"),
         QStringLiteral("Кнопка закрытия прячет приложение в трей вместо выхода")},
        {QStringLiteral("settings.confirmTrashDeletion"), QStringLiteral("Подтверждать удаление треков в корзину")},
        {QStringLiteral("settings.confirmTrashDeletionDescription"),
         QStringLiteral("Показывать предупреждение перед перемещением файла в корзину из плейлиста")},
        {QStringLiteral("settings.automaticPlaylistSearch"), QStringLiteral("Автоматический поиск")},
        {QStringLiteral("settings.automaticPlaylistSearchDescription"),
         QStringLiteral("Если выключено, поиск по плейлисту запускается только по Enter или по кнопке лупы. Если включено, поиск обновляется во время ввода.")},
        {QStringLiteral("settings.autoAddTracksFromPlaylistFolder"),
         QStringLiteral("Автодобавление новых треков из папки плейлиста")},
        {QStringLiteral("settings.autoAddTracksFromPlaylistFolderDescription"),
         QStringLiteral("Если большинство треков плейлиста находится в одной папке, WaveFlux следит за ней и автоматически добавляет новые поддерживаемые файлы.")},
        {QStringLiteral("settings.ytDlpExecutablePath"), QStringLiteral("Путь к yt-dlp")},
        {QStringLiteral("settings.ytDlpExecutablePathDescription"),
         QStringLiteral("Оставьте поле пустым, чтобы искать yt-dlp через PATH.")},
        {QStringLiteral("settings.ffmpegExecutablePath"), QStringLiteral("Путь к ffmpeg")},
        {QStringLiteral("settings.ffmpegExecutablePathDescription"),
         QStringLiteral("Оставьте поле пустым, чтобы искать ffmpeg через PATH.")},
        {QStringLiteral("settings.browse"), QStringLiteral("Выбрать")},
        {QStringLiteral("settings.pickExecutableTitle"), QStringLiteral("Выберите исполняемый файл %1")},
        {QStringLiteral("settings.externalToolConfiguredInvalid"),
         QStringLiteral("Указанный путь для %1 некорректен или файл не является исполняемым: %2")},
        {QStringLiteral("settings.externalToolMissingFromPath"),
         QStringLiteral("%1 не найден в PATH. Укажите явный путь к исполняемому файлу или установите %1.")},
        {QStringLiteral("settings.externalToolExecutionFailed"),
         QStringLiteral("Не удалось выполнить `%1 --version`: %2")},
        {QStringLiteral("settings.externalToolReadyConfigured"),
         QStringLiteral("Используется явно заданный %1: %2")},
        {QStringLiteral("settings.externalToolReadyPath"),
         QStringLiteral("Используется %1 из PATH: %2")},
        {QStringLiteral("settings.externalToolResolvedPath"), QStringLiteral("Разрешённый путь")},
        {QStringLiteral("settings.externalToolVersion"), QStringLiteral("Версия")},
        {QStringLiteral("settings.externalToolLastValidatedPath"), QStringLiteral("Последний валидный путь")},
        {QStringLiteral("settings.importRuntimeVersionPolicy"), QStringLiteral("Политика версий")},
        {QStringLiteral("settings.importRuntimeVersionPolicyDescription"),
         QStringLiteral("WaveFlux сохраняет версии инструментов для диагностики.")},
        {QStringLiteral("ytDlpImport.probeStarted"), QStringLiteral("Чтение метаданных источника...")},
        {QStringLiteral("ytDlpImport.probeAlreadyRunning"), QStringLiteral("Проверка метаданных уже выполняется.")},
        {QStringLiteral("ytDlpImport.probeInvalidUrl"),
         QStringLiteral("Перед запуском проверки метаданных укажите корректный URL http(s).")},
        {QStringLiteral("ytDlpImport.probeRuntimeUnavailable"),
         QStringLiteral("Настройки runtime для yt-dlp недоступны.")},
        {QStringLiteral("ytDlpImport.probeCanceled"), QStringLiteral("Проверка метаданных отменена.")},
        {QStringLiteral("ytDlpImport.probeFailedStart"),
         QStringLiteral("Не удалось запустить проверку метаданных yt-dlp: %1")},
        {QStringLiteral("ytDlpImport.probeFailedProcess"),
         QStringLiteral("Проверка метаданных yt-dlp завершилась ошибкой: %1")},
        {QStringLiteral("ytDlpImport.probeFailedInvalidJson"),
         QStringLiteral("yt-dlp вернул некорректный JSON метаданных: %1")},
        {QStringLiteral("ytDlpImport.errorDialogTitle"), QStringLiteral("Ошибка yt-dlp")},
        {QStringLiteral("ytDlpImport.probeReadySingle"),
         QStringLiteral("Предпросмотр метаданных готов для одного элемента.")},
        {QStringLiteral("ytDlpImport.probeReadyPlaylist"),
         QStringLiteral("Предпросмотр метаданных готов для %1 элементов плейлиста.")},
        {QStringLiteral("ytDlpImport.importAlreadyRunning"),
         QStringLiteral("Очередь импорта уже выполняется.")},
        {QStringLiteral("ytDlpImport.importBlockedWhileProbing"),
         QStringLiteral("Сначала завершите проверку метаданных, затем запускайте импорт.")},
        {QStringLiteral("ytDlpImport.importRequiresProbe"),
         QStringLiteral("Перед запуском импорта сначала проверьте метаданные URL.")},
        {QStringLiteral("ytDlpImport.importStarted"),
         QStringLiteral("Запуск очереди импорта аудио...")},
        {QStringLiteral("ytDlpImport.importRunningActiveCount"),
         QStringLiteral("Выполняется: активных загрузок %1.")},
        {QStringLiteral("ytDlpImport.importFinished"),
         QStringLiteral("Очередь импорта аудио завершена.")},
        {QStringLiteral("ytDlpImport.importCanceled"),
         QStringLiteral("Очередь импорта аудио отменена.")},
        {QStringLiteral("ytDlpImport.importInvalidOutputDirectory"),
         QStringLiteral("Указана некорректная выходная папка: %1")},
        {QStringLiteral("ytDlpImport.importNoPlayableItems"),
         QStringLiteral("Нет доступных элементов для импорта.")},
        {QStringLiteral("ytDlpImport.itemStarting"),
         QStringLiteral("Запуск загрузки...")},
        {QStringLiteral("ytDlpImport.itemFinished"),
         QStringLiteral("Загрузка завершена.")},
        {QStringLiteral("ytDlpImport.itemCanceled"),
         QStringLiteral("Загрузка отменена.")},
        {QStringLiteral("ytDlpImport.itemSkippedUnavailable"),
         QStringLiteral("Пропущено: элемент источника недоступен.")},
        {QStringLiteral("ytDlpImport.itemMissingOutput"),
         QStringLiteral("Загрузка завершилась без создания ожидаемого файла: %1")},
        {QStringLiteral("ytDlpImport.importFailedStart"),
         QStringLiteral("Не удалось запустить процесс импорта yt-dlp: %1")},
        {QStringLiteral("ytDlpImport.importProcessFailed"),
         QStringLiteral("Импорт через yt-dlp завершился ошибкой: %1")},
        {QStringLiteral("ytDlpImport.errorCategoryDependency"),
         QStringLiteral("Проблема с зависимостями")},
        {QStringLiteral("ytDlpImport.errorCategoryContent"),
         QStringLiteral("Проблема с содержимым источника")},
        {QStringLiteral("ytDlpImport.errorCategoryNetwork"), QStringLiteral("Сетевая проблема")},
        {QStringLiteral("ytDlpImport.errorCategoryPermission"),
         QStringLiteral("Проблема с правами доступа")},
        {QStringLiteral("ytDlpImport.errorCategoryDisk"), QStringLiteral("Недостаточно места на диске")},
        {QStringLiteral("ytDlpImport.errorCategoryOutput"),
         QStringLiteral("Проблема с выходным файлом")},
        {QStringLiteral("ytDlpImport.errorCategoryPostprocess"),
         QStringLiteral("Проблема постобработки")},
        {QStringLiteral("ytDlpImport.errorCategoryCanceled"), QStringLiteral("Отменено")},
        {QStringLiteral("ytDlpImport.errorCategoryMixed"), QStringLiteral("Смешанные проблемы")},
        {QStringLiteral("ytDlpImport.errorCategoryGeneric"), QStringLiteral("Ошибка импорта")},
        {QStringLiteral("ytDlpImport.summaryHeadlineSucceeded"),
         QStringLiteral("Импорт завершён успешно.")},
        {QStringLiteral("ytDlpImport.summaryHeadlineSucceededWithSkips"),
         QStringLiteral("Импорт завершён с пропущенными элементами.")},
        {QStringLiteral("ytDlpImport.summaryHeadlinePartialFailed"),
         QStringLiteral("Импорт завершён с частичным успехом.")},
        {QStringLiteral("ytDlpImport.summaryHeadlineFailed"),
         QStringLiteral("Импорт завершился ошибкой.")},
        {QStringLiteral("ytDlpImport.summaryHeadlineCanceled"),
         QStringLiteral("Импорт отменён до финализации файлов.")},
        {QStringLiteral("ytDlpImport.summaryHeadlinePartialCanceled"),
         QStringLiteral("Импорт отменён после частичного успеха.")},
        {QStringLiteral("ytDlpImport.summaryDetailPattern"),
         QStringLiteral("Успешно: %1, с ошибкой: %2, отменено: %3, пропущено: %4.")},
        {QStringLiteral("ytDlpImport.problemGeneral"), QStringLiteral("Общая ошибка импорта")},
        {QStringLiteral("ytDlpImport.dialogTitle"), QStringLiteral("Импорт аудио по ссылке")},
        {QStringLiteral("ytDlpImport.dialogSubtitle"),
         QStringLiteral("Сначала проверьте источник, затем подтвердите план сохранения и запустите последовательный импорт.")},
        {QStringLiteral("ytDlpImport.urlSection"), QStringLiteral("Ссылка на источник")},
        {QStringLiteral("ytDlpImport.urlHint"),
         QStringLiteral("WaveFlux сначала читает метаданные, чтобы вы подтвердили источник до начала загрузки.")},
        {QStringLiteral("ytDlpImport.urlPlaceholder"),
         QStringLiteral("https://example.com/watch?v=...")},
        {QStringLiteral("ytDlpImport.checkUrl"), QStringLiteral("Проверить ссылку")},
        {QStringLiteral("ytDlpImport.checkingUrl"), QStringLiteral("Проверка...")},
        {QStringLiteral("ytDlpImport.pasteUrl"), QStringLiteral("Вставить")},
        {QStringLiteral("ytDlpImport.previewSection"), QStringLiteral("Предпросмотр источника")},
        {QStringLiteral("ytDlpImport.previewHint"),
         QStringLiteral("Этот блок строится по результату проверки метаданных yt-dlp и пока ничего не скачивает.")},
        {QStringLiteral("ytDlpImport.sourceTypeLabel"), QStringLiteral("Тип источника")},
        {QStringLiteral("ytDlpImport.sourceSingle"), QStringLiteral("Одиночный трек")},
        {QStringLiteral("ytDlpImport.sourcePlaylist"), QStringLiteral("Плейлист")},
        {QStringLiteral("ytDlpImport.sourceTitleLabel"), QStringLiteral("Название")},
        {QStringLiteral("ytDlpImport.currentEntryTitleLabel"), QStringLiteral("Текущий элемент")},
        {QStringLiteral("ytDlpImport.entryCountLabel"), QStringLiteral("Всего элементов")},
        {QStringLiteral("ytDlpImport.playableCountLabel"), QStringLiteral("Доступно для импорта")},
        {QStringLiteral("ytDlpImport.unavailableCountLabel"), QStringLiteral("Недоступно")},
        {QStringLiteral("ytDlpImport.extractorLabel"), QStringLiteral("Источник обработки")},
        {QStringLiteral("ytDlpImport.redirectedLabel"), QStringLiteral("Разрешённый URL")},
        {QStringLiteral("ytDlpImport.outputSection"), QStringLiteral("Параметры сохранения")},
        {QStringLiteral("ytDlpImport.outputHint"),
         QStringLiteral("До запуска очереди выберите итоговый формат и папку назначения.")},
        {QStringLiteral("ytDlpImport.namingPolicyLabel"), QStringLiteral("Правило именования")},
        {QStringLiteral("ytDlpImport.namingAuto"),
         QStringLiteral("Позиция в плейлисте + название")},
        {QStringLiteral("ytDlpImport.namingTitleOnly"),
         QStringLiteral("Только название")},
        {QStringLiteral("ytDlpImport.namingSourceAndEntryTitle"),
         QStringLiteral("Название источника + название элемента")},
        {QStringLiteral("ytDlpImport.namingSummaryAuto"),
         QStringLiteral("Импорт из плейлиста сохраняет префикс позиции, чтобы порядок источника был виден и на диске.")},
        {QStringLiteral("ytDlpImport.namingSummaryTitleOnly"),
         QStringLiteral("Файлы называются только по заголовку источника, а конфликты имён авто-переименовываются на этапе планирования.")},
        {QStringLiteral("ytDlpImport.namingSummarySourceAndEntryTitle"),
         QStringLiteral("Имя файла составляется из названия источника и названия элемента, когда они различаются.")},
        {QStringLiteral("ytDlpImport.parallelDownloadsLabel"), QStringLiteral("Режим загрузки")},
        {QStringLiteral("ytDlpImport.parallelDownloadsSequentialOption"),
         QStringLiteral("1 загрузка (безопасный последовательный режим)")},
        {QStringLiteral("ytDlpImport.parallelDownloadsParallelOption"),
         QStringLiteral("2 загрузки (контролируемый параллельный режим)")},
        {QStringLiteral("ytDlpImport.parallelDownloadsHighParallelOption"),
         QStringLiteral("4 загрузки (выше скорость, выше риск)")},
        {QStringLiteral("ytDlpImport.parallelDownloadsSequentialHint"),
         QStringLiteral("Сохраняет текущий безопасный режим: один worker yt-dlp за раз.")},
        {QStringLiteral("ytDlpImport.parallelDownloadsParallelHint"),
         QStringLiteral("Быстрее для больших плейлистов, но повышает нагрузку на диск и сеть и может раньше вызвать rate limit.")},
        {QStringLiteral("ytDlpImport.summarySection"), QStringLiteral("Сводка операции")},
        {QStringLiteral("ytDlpImport.summaryHint"),
         QStringLiteral("Проверьте, что именно будет скачано, куда оно сохранится и как сохранится порядок в плейлисте.")},
        {QStringLiteral("ytDlpImport.summaryTargetDirectory"), QStringLiteral("Папка сохранения")},
        {QStringLiteral("ytDlpImport.summaryFormat"), QStringLiteral("Формат на выходе")},
        {QStringLiteral("ytDlpImport.summaryNamingRule"), QStringLiteral("Именование")},
        {QStringLiteral("ytDlpImport.summaryItems"), QStringLiteral("Элементов")},
        {QStringLiteral("ytDlpImport.summaryPlayable"), QStringLiteral("Будет импортировано")},
        {QStringLiteral("ytDlpImport.summaryUnavailable"), QStringLiteral("Будет пропущено")},
        {QStringLiteral("ytDlpImport.summaryQueueMode"), QStringLiteral("Режим очереди")},
        {QStringLiteral("ytDlpImport.summaryQueueModeSequential"),
         QStringLiteral("Последовательно: один процесс yt-dlp за раз.")},
        {QStringLiteral("ytDlpImport.summaryQueueModeParallel"),
         QStringLiteral("Параллельно: до %1 процессов yt-dlp одновременно.")},
        {QStringLiteral("ytDlpImport.summaryPlaylistOrder"), QStringLiteral("Порядок в плейлисте")},
        {QStringLiteral("ytDlpImport.summaryPlaylistOrderValue"),
         QStringLiteral("Успешные файлы добавляются в порядке источника. Порядок завершения загрузок игнорируется.")},
        {QStringLiteral("ytDlpImport.queueSection"), QStringLiteral("Предпросмотр очереди")},
        {QStringLiteral("ytDlpImport.queueHint"),
         QStringLiteral("До запуска здесь показаны элементы предварительной проверки. Во время импорта список переключается на текущее состояние очереди.")},
        {QStringLiteral("ytDlpImport.queueStateReady"), QStringLiteral("Готово к импорту")},
        {QStringLiteral("ytDlpImport.queueStatePending"), QStringLiteral("Ожидает")},
        {QStringLiteral("ytDlpImport.queueStateRunning"), QStringLiteral("Выполняется")},
        {QStringLiteral("ytDlpImport.queueStateSucceeded"), QStringLiteral("Успешно")},
        {QStringLiteral("ytDlpImport.queueStateFailed"), QStringLiteral("Ошибка")},
        {QStringLiteral("ytDlpImport.queueStateCanceled"), QStringLiteral("Отменено")},
        {QStringLiteral("ytDlpImport.queueStateSkipped"), QStringLiteral("Пропущено")},
        {QStringLiteral("ytDlpImport.queueStateUnavailable"), QStringLiteral("Недоступно")},
        {QStringLiteral("ytDlpImport.untitledEntry"), QStringLiteral("Элемент без названия")},
        {QStringLiteral("ytDlpImport.plannedOutputLabel"), QStringLiteral("Планируемый файл: %1")},
        {QStringLiteral("ytDlpImport.progressSection"), QStringLiteral("Прогресс")},
        {QStringLiteral("ytDlpImport.progressHint"),
         QStringLiteral("Критичные ошибки показываются здесь напрямую, без чтения логов.")},
        {QStringLiteral("ytDlpImport.activeDownloadsLabel"), QStringLiteral("Активные загрузки")},
        {QStringLiteral("ytDlpImport.finalSummarySection"), QStringLiteral("Итоговая сводка")},
        {QStringLiteral("ytDlpImport.finalSummaryHint"),
         QStringLiteral("В шаг импорта плейлиста попадают только успешно финализированные локальные файлы.")},
        {QStringLiteral("ytDlpImport.finalSucceededCount"), QStringLiteral("Успешно")},
        {QStringLiteral("ytDlpImport.finalFailedCount"), QStringLiteral("С ошибкой")},
        {QStringLiteral("ytDlpImport.finalCanceledCount"), QStringLiteral("Отменено")},
        {QStringLiteral("ytDlpImport.finalSkippedCount"), QStringLiteral("Пропущено")},
        {QStringLiteral("ytDlpImport.finalImportedCount"), QStringLiteral("Импортировано в плейлист")},
        {QStringLiteral("ytDlpImport.finalNotProbedCount"), QStringLiteral("Не проверено")},
        {QStringLiteral("ytDlpImport.finalConflictBlockedCount"), QStringLiteral("Заблокировано конфликтом")},
        {QStringLiteral("ytDlpImport.recentUrls"), QStringLiteral("Недавние ссылки")},
        {QStringLiteral("ytDlpImport.recentFolders"), QStringLiteral("Недавние папки")},
        {QStringLiteral("ytDlpImport.sourcesSection"), QStringLiteral("Источники")},
        {QStringLiteral("ytDlpImport.sourcesHint"),
         QStringLiteral("Управляйте очередью источников до запуска и между повторными попытками.")},
        {QStringLiteral("ytDlpImport.clearFailedProbes"), QStringLiteral("Удалить неудачные проверки")},
        {QStringLiteral("ytDlpImport.retryFailedProbes"), QStringLiteral("Повторить неудачные проверки")},
        {QStringLiteral("ytDlpImport.retryFailedImports"), QStringLiteral("Повторить неудачные импорты")},
        {QStringLiteral("ytDlpImport.reopenLatestReport"), QStringLiteral("Открыть последний отчёт")},
        {QStringLiteral("ytDlpImport.sourceStatusPending"), QStringLiteral("Ожидает")},
        {QStringLiteral("ytDlpImport.sourceStatusPendingProbe"), QStringLiteral("Ожидает проверки")},
        {QStringLiteral("ytDlpImport.sourceStatusProbing"), QStringLiteral("Проверяется")},
        {QStringLiteral("ytDlpImport.sourceStatusReady"), QStringLiteral("Готово")},
        {QStringLiteral("ytDlpImport.sourceStatusReadyWithIssues"), QStringLiteral("Готово с замечаниями")},
        {QStringLiteral("ytDlpImport.sourceStatusProbeFailed"), QStringLiteral("Проверка не удалась")},
        {QStringLiteral("ytDlpImport.sourceStatusImporting"), QStringLiteral("Импортируется")},
        {QStringLiteral("ytDlpImport.sourceStatusCompleted"), QStringLiteral("Завершено")},
        {QStringLiteral("ytDlpImport.sourceStatusCompletedWithFailures"),
         QStringLiteral("Завершено с ошибками")},
        {QStringLiteral("ytDlpImport.sourceStatusCanceled"), QStringLiteral("Отменено")},
        {QStringLiteral("ytDlpImport.sourceEntryCount"), QStringLiteral("%1 элементов")},
        {QStringLiteral("ytDlpImport.sourcePreviewStale"), QStringLiteral("Предпросмотр устарел")},
        {QStringLiteral("ytDlpImport.diagnosticsNamingAuto"),
         QStringLiteral("Именование: позиция в плейлисте + название")},
        {QStringLiteral("ytDlpImport.diagnosticsNamingTitleOnly"),
         QStringLiteral("Именование: только название")},
        {QStringLiteral("ytDlpImport.diagnosticsNamingSourceAndEntryTitle"),
         QStringLiteral("Именование: название источника + название элемента")},
        {QStringLiteral("ytDlpImport.diagnosticsFallbackMissingSourceTitle"),
         QStringLiteral("Резервный вариант: у источника нет названия")},
        {QStringLiteral("ytDlpImport.diagnosticsFallbackRedundantSourceTitle"),
         QStringLiteral("Резервный вариант: название источника совпадает с названием элемента")},
        {QStringLiteral("ytDlpImport.diagnosticsFallbackNonPlaylistSource"),
         QStringLiteral("Резервный вариант: для этого источника нет позиции в плейлисте")},
        {QStringLiteral("ytDlpImport.diagnosticsConflictScopeQueue"),
         QStringLiteral("Область конфликта: другой элемент в этой задаче")},
        {QStringLiteral("ytDlpImport.diagnosticsConflictScopeExistingTarget"),
         QStringLiteral("Область конфликта: файл уже существует на диске")},
        {QStringLiteral("ytDlpImport.diagnosticsResolutionAutoRenamed"),
         QStringLiteral("Решение: имя изменится автоматически")},
        {QStringLiteral("ytDlpImport.diagnosticsResolutionSkipOnConflict"),
         QStringLiteral("Решение: элемент будет пропущен")},
        {QStringLiteral("ytDlpImport.diagnosticsResolutionFailOnConflict"),
         QStringLiteral("Решение: загрузка завершится ошибкой до начала скачивания")},
        {QStringLiteral("ytDlpImport.clearButton"), QStringLiteral("Очистить")},
        {QStringLiteral("ytDlpImport.cancelButton"), QStringLiteral("Отменить")},
        {QStringLiteral("ytDlpImport.hideSession"), QStringLiteral("Скрыть сеанс")},
        {QStringLiteral("ytDlpImport.showSession"), QStringLiteral("Показать сеанс")},
        {QStringLiteral("ytDlpImport.sessionReadyHidden"),
         QStringLiteral("Текущий сеанс импорта по ссылке продолжает быть доступен в фоне.")},
        {QStringLiteral("ytDlpImport.startImport"), QStringLiteral("Запустить импорт")},
        {QStringLiteral("ytDlpImport.stateIdle"), QStringLiteral("Ожидание")},
        {QStringLiteral("ytDlpImport.stateProbing"), QStringLiteral("Проверка")},
        {QStringLiteral("ytDlpImport.stateReady"), QStringLiteral("Готово")},
        {QStringLiteral("ytDlpImport.stateRunning"), QStringLiteral("Импорт")},
        {QStringLiteral("ytDlpImport.stateCanceled"), QStringLiteral("Отменено")},
        {QStringLiteral("ytDlpImport.stateFailed"), QStringLiteral("Ошибка")},
        {QStringLiteral("ytDlpImport.stateSucceeded"), QStringLiteral("Завершено")},
        {QStringLiteral("ytDlpImport.selectOutputFolderTitle"),
         QStringLiteral("Выберите папку для импорта по ссылке")},
        {QStringLiteral("ytDlpImport.errorInvalidOutputDirectory"),
         QStringLiteral("Указана некорректная выходная папка.")},
        {QStringLiteral("ytDlpImport.clipboardEmpty"),
         QStringLiteral("В буфере обмена нет текста для вставки в поле URL.")},
        {QStringLiteral("settings.audio"), QStringLiteral("Аудио")},
        {QStringLiteral("settings.pitch"), QStringLiteral("Тональность (полутоны):")},
        {QStringLiteral("settings.resetPitch"), QStringLiteral("Сбросить тональность")},
        {QStringLiteral("settings.colors"), QStringLiteral("Цвета")},
        {QStringLiteral("settings.presetThemes"), QStringLiteral("Предустановленные темы")},
        {QStringLiteral("settings.dark"), QStringLiteral("Темная")},
        {QStringLiteral("settings.light"), QStringLiteral("Светлая")},
        {QStringLiteral("settings.reset"), QStringLiteral("Сбросить")},
        {QStringLiteral("settings.close"), QStringLiteral("Закрыть")},
        {QStringLiteral("settings.aboutVersion"), QStringLiteral("WaveFlux v1.2.0")},
        {QStringLiteral("settings.aboutTagline"),
         QStringLiteral("Минималистичный аудиоплеер с визуализацией волны")},
        {QStringLiteral("player.previous"), QStringLiteral("Предыдущий")},
        {QStringLiteral("player.pause"), QStringLiteral("Пауза")},
        {QStringLiteral("player.play"), QStringLiteral("Воспроизвести")},
        {QStringLiteral("player.stop"), QStringLiteral("Стоп")},
        {QStringLiteral("player.next"), QStringLiteral("Следующий")},
        {QStringLiteral("player.shuffleEnable"), QStringLiteral("Включить перемешивание")},
        {QStringLiteral("player.shuffleDisable"), QStringLiteral("Выключить перемешивание")},
        {QStringLiteral("player.repeatOff"), QStringLiteral("Повтор: выкл")},
        {QStringLiteral("player.repeatAll"), QStringLiteral("Повтор: все треки")},
        {QStringLiteral("player.repeatOne"), QStringLiteral("Повтор: текущий трек")},
        {QStringLiteral("player.resetSpeed"), QStringLiteral("Сбросить скорость")},
        {QStringLiteral("player.spaceHoldSpeed2x"), QStringLiteral("удерживать для временной скорости 2x")},
        {QStringLiteral("player.resetPitch"), QStringLiteral("Сбросить тональность")},
        {QStringLiteral("player.semitones"), QStringLiteral("полутонов")},
        {QStringLiteral("playlist.searchPlaceholder"),
         QStringLiteral("Поиск... title: artist: album: path: is:lossless is:hires")},
        {QStringLiteral("playlist.sort"), QStringLiteral("Сортировка")},
        {QStringLiteral("playlist.sortPlaylist"), QStringLiteral("Сортировать плейлист")},
        {QStringLiteral("playlist.random"), QStringLiteral("Случайно")},
        {QStringLiteral("playlist.randomize"), QStringLiteral("Перемешать плейлист")},
        {QStringLiteral("playlist.locate"), QStringLiteral("Найти")},
        {QStringLiteral("playlist.locateCurrent"), QStringLiteral("Прокрутить к текущему треку")},
        {QStringLiteral("playlist.clear"), QStringLiteral("Очистить")},
        {QStringLiteral("playlist.clearPlaylist"), QStringLiteral("Очистить плейлист")},
        {QStringLiteral("playlist.tracks"), QStringLiteral("треков")},
        {QStringLiteral("playlist.matches"), QStringLiteral("совпадений")},
        {QStringLiteral("playlist.dropHint"),
         QStringLiteral("Перетащите аудиофайлы или .xspf плейлисты сюда\nили используйте Файл > Открыть")},
        {QStringLiteral("playlist.noMatches"), QStringLiteral("Нет треков по вашему запросу")},
        {QStringLiteral("playlist.byNameAsc"), QStringLiteral("По имени (А-Я)")},
        {QStringLiteral("playlist.byNameDesc"), QStringLiteral("По имени (Я-А)")},
        {QStringLiteral("playlist.byDateOldest"), QStringLiteral("По дате (сначала старые)")},
        {QStringLiteral("playlist.byDateNewest"), QStringLiteral("По дате (сначала новые)")},
        {QStringLiteral("tagEditor.title"), QStringLiteral("Редактор тегов")},
        {QStringLiteral("tagEditor.titleLabel"), QStringLiteral("Название:")},
        {QStringLiteral("tagEditor.artist"), QStringLiteral("Исполнитель:")},
        {QStringLiteral("tagEditor.album"), QStringLiteral("Альбом:")},
        {QStringLiteral("tagEditor.genre"), QStringLiteral("Жанр:")},
        {QStringLiteral("tagEditor.year"), QStringLiteral("Год:")},
        {QStringLiteral("tagEditor.trackNumber"), QStringLiteral("Трек #:")},
        {QStringLiteral("tagEditor.cover"), QStringLiteral("Обложка:")},
        {QStringLiteral("tagEditor.coverSelect"), QStringLiteral("Выбрать...")},
        {QStringLiteral("tagEditor.coverClear"), QStringLiteral("Удалить")},
        {QStringLiteral("tagEditor.coverKeep"), QStringLiteral("Оставить текущую встроенную обложку")},
        {QStringLiteral("tagEditor.coverSelected"), QStringLiteral("Выбрано: ")},
        {QStringLiteral("tagEditor.coverRemovePending"), QStringLiteral("Обложка будет удалена при сохранении")},
        {QStringLiteral("tagEditor.coverPickerTitle"), QStringLiteral("Выберите изображение обложки")},
        {QStringLiteral("tagEditor.file"), QStringLiteral("Файл: ")},
        {QStringLiteral("tagEditor.error"), QStringLiteral("Ошибка: ")},
        {QStringLiteral("tagEditor.bulkTitle"), QStringLiteral("Редактирование тегов выбранных")},
        {QStringLiteral("tagEditor.bulkHint"), QStringLiteral("Отметьте поля, которые нужно перезаписать для всех выбранных треков.")},
        {QStringLiteral("tagEditor.bulkApply"), QStringLiteral("Применить к выбранным")},
        {QStringLiteral("audioConverter.title"), QStringLiteral("Аудиоконвертер")},
        {QStringLiteral("audioConverter.sourceSection"), QStringLiteral("Исходный трек")},
        {QStringLiteral("audioConverter.sourcePath"), QStringLiteral("Путь: ")},
        {QStringLiteral("audioConverter.duration"), QStringLiteral("Длительность: ")},
        {QStringLiteral("audioConverter.originalFormat"), QStringLiteral("Исходный формат: ")},
        {QStringLiteral("audioConverter.sourceSpec"), QStringLiteral("Исходные параметры: ")},
        {QStringLiteral("audioConverter.outputSection"), QStringLiteral("Выходной файл")},
        {QStringLiteral("audioConverter.outputSectionHint"), QStringLiteral("Выберите, куда сохранить результат конвертации. Существующие файлы не заменяются без подтверждения.")},
        {QStringLiteral("audioConverter.outputPlaceholder"), QStringLiteral("Выберите путь для нового аудиофайла")},
        {QStringLiteral("audioConverter.browse"), QStringLiteral("Обзор...")},
        {QStringLiteral("audioConverter.useSuggested"), QStringLiteral("Подставить вариант")},
        {QStringLiteral("audioConverter.outputHint"), QStringLiteral("Предлагаемый путь: ")},
        {QStringLiteral("audioConverter.formatSection"), QStringLiteral("Формат и качество")},
        {QStringLiteral("audioConverter.formatSectionHint"), QStringLiteral("Выберите целевой формат и качество, с которыми будет записан новый файл.")},
        {QStringLiteral("audioConverter.format"), QStringLiteral("Формат")},
        {QStringLiteral("audioConverter.formatUnavailableLabel"), QStringLiteral("%1 (недоступен)")},
        {QStringLiteral("audioConverter.formatUnavailableHint"), QStringLiteral("%1 недоступен в этой установке. Установите недостающие компоненты конвертации и попробуйте снова: %2")},
        {QStringLiteral("audioConverter.formatUnavailableGenericHint"), QStringLiteral("%1 недоступен в этой установке. Установите недостающие компоненты конвертации и попробуйте снова.")},
        {QStringLiteral("audioConverter.codec"), QStringLiteral("Кодек")},
        {QStringLiteral("audioConverter.container"), QStringLiteral("Контейнер")},
        {QStringLiteral("audioConverter.bitrate"), QStringLiteral("Битрейт")},
        {QStringLiteral("audioConverter.sampleRate"), QStringLiteral("Частота дискретизации")},
        {QStringLiteral("audioConverter.channels"), QStringLiteral("Каналы")},
        {QStringLiteral("audioConverter.channelMono"), QStringLiteral("Моно")},
        {QStringLiteral("audioConverter.channelStereo"), QStringLiteral("Стерео")},
        {QStringLiteral("audioConverter.transformSection"), QStringLiteral("Преобразование")},
        {QStringLiteral("audioConverter.transformSectionHint"), QStringLiteral("Меняйте скорость и тональность только если результат должен звучать иначе, чем исходный файл.")},
        {QStringLiteral("audioConverter.speed"), QStringLiteral("Скорость: ")},
        {QStringLiteral("audioConverter.pitch"), QStringLiteral("Тональность: ")},
        {QStringLiteral("audioConverter.reset"), QStringLiteral("Сбросить")},
        {QStringLiteral("audioConverter.semitones"), QStringLiteral("полутонов")},
        {QStringLiteral("audioConverter.statusSection"), QStringLiteral("Статус")},
        {QStringLiteral("audioConverter.badgeReady"), QStringLiteral("Готово к запуску")},
        {QStringLiteral("audioConverter.badgeAttention"), QStringLiteral("Нужно действие")},
        {QStringLiteral("audioConverter.badgeConflict"), QStringLiteral("Нужна замена")},
        {QStringLiteral("audioConverter.badgeRunning"), QStringLiteral("В процессе")},
        {QStringLiteral("audioConverter.badgeSucceeded"), QStringLiteral("Готово")},
        {QStringLiteral("audioConverter.badgeCanceled"), QStringLiteral("Отменено")},
        {QStringLiteral("audioConverter.badgeFailed"), QStringLiteral("Ошибка")},
        {QStringLiteral("audioConverter.statusTitleReady"), QStringLiteral("Все готово к конвертации")},
        {QStringLiteral("audioConverter.statusTitleNeedsAttention"), QStringLiteral("Проверьте параметры результата")},
        {QStringLiteral("audioConverter.statusTitleConflict"), QStringLiteral("Нужно подтвердить замену файла")},
        {QStringLiteral("audioConverter.statusTitleRunning"), QStringLiteral("Конвертация выполняется")},
        {QStringLiteral("audioConverter.statusTitleSucceeded"), QStringLiteral("Результат конвертации готов")},
        {QStringLiteral("audioConverter.statusTitleCanceled"), QStringLiteral("Конвертация была отменена")},
        {QStringLiteral("audioConverter.statusTitleFailed"), QStringLiteral("Конвертацию не удалось завершить")},
        {QStringLiteral("audioConverter.progressAccessibleName"), QStringLiteral("Прогресс конвертации")},
        {QStringLiteral("audioConverter.runtimeReady"), QStringLiteral("Выберите путь результата и параметры конвертации.")},
        {QStringLiteral("audioConverter.runtimeStarted"), QStringLiteral("Идет конвертация аудио. Закрытие окна отменит текущую операцию.")},
        {QStringLiteral("audioConverter.runtimeCanceled"), QStringLiteral("Конвертация отменена. Измените параметры и попробуйте снова.")},
        {QStringLiteral("audioConverter.runtimeSucceeded"), QStringLiteral("Конвертация успешно завершена.")},
        {QStringLiteral("audioConverter.runtimeSucceededMetadataSkipped"), QStringLiteral("Конвертация успешно завершена, но базовые метаданные не удалось перенести: %1")},
        {QStringLiteral("audioConverter.runtimeFailedAlreadyRunning"), QStringLiteral("Другая конвертация уже выполняется. Дождитесь завершения или отмените ее перед новым запуском.")},
        {QStringLiteral("audioConverter.runtimeFailedStartPreparation"), QStringLiteral("Не удалось подготовить конвертацию. Проверьте исходный файл, путь результата и доступность компонентов конвертации, затем попробуйте снова.")},
        {QStringLiteral("audioConverter.runtimeFailedStartPlayback"), QStringLiteral("Не удалось запустить обработку нового результата. Попробуйте снова или выберите другие параметры.")},
        {QStringLiteral("audioConverter.runtimeFailedPipeline"), QStringLiteral("Конвертация остановилась из-за сбоя обработки аудио. Проверьте исходный файл и доступность компонентов конвертации, затем попробуйте снова.")},
        {QStringLiteral("audioConverter.runtimeFailedExistingOutput"), QStringLiteral("Файл с таким именем уже существует. Подтвердите замену перед запуском конвертации.")},
        {QStringLiteral("audioConverter.runtimeFailedReplaceExisting"), QStringLiteral("Не удалось заменить существующий файл результата: %1")},
        {QStringLiteral("audioConverter.runtimeFailedFinalizeOutput"), QStringLiteral("Не удалось сохранить итоговый файл по выбранному пути: %1")},
        {QStringLiteral("audioConverter.runtimeFailedGeneric"), QStringLiteral("Конвертация завершилась ошибкой. Проверьте исходный файл и путь результата, затем попробуйте снова.")},
        {QStringLiteral("audioConverter.readyHint"), QStringLiteral("Выберите путь результата и параметры конвертации.")},
        {QStringLiteral("audioConverter.stateRunning"), QStringLiteral("Идет конвертация аудио. Закрытие окна отменит текущую операцию.")},
        {QStringLiteral("audioConverter.stateSucceeded"), QStringLiteral("Конвертация успешно завершена.")},
        {QStringLiteral("audioConverter.stateCanceled"), QStringLiteral("Конвертация отменена. Измените параметры и попробуйте снова.")},
        {QStringLiteral("audioConverter.stateFailed"), QStringLiteral("Конвертация завершилась ошибкой. Исправьте проблему и попробуйте снова.")},
        {QStringLiteral("audioConverter.resultPath"), QStringLiteral("Сохраненный файл: ")},
        {QStringLiteral("audioConverter.closeAccessibleDescription"), QStringLiteral("Закрыть окно конвертера, не меняя текущие параметры конвертации.")},
        {QStringLiteral("audioConverter.escapeRunningConfirmTitle"), QStringLiteral("Отменить конвертацию и закрыть окно")},
        {QStringLiteral("audioConverter.escapeRunningConfirmMessage"), QStringLiteral("Конвертация аудио еще выполняется. Отменить текущую операцию и закрыть окно?")},
        {QStringLiteral("audioConverter.showInPlaylist"), QStringLiteral("Показать в плейлисте")},
        {QStringLiteral("audioConverter.summaryFormat"), QStringLiteral("Формат")},
        {QStringLiteral("audioConverter.summaryTransform"), QStringLiteral("Преобразование")},
        {QStringLiteral("audioConverter.summaryOutput"), QStringLiteral("Файл результата")},
        {QStringLiteral("audioConverter.convert"), QStringLiteral("Конвертировать")},
        {QStringLiteral("audioConverter.replace"), QStringLiteral("Заменить")},
        {QStringLiteral("audioConverter.cancel"), QStringLiteral("Отмена")},
        {QStringLiteral("audioConverter.close"), QStringLiteral("Закрыть")},
        {QStringLiteral("audioConverter.saveDialogTitle"), QStringLiteral("Выберите выходной аудиофайл")},
        {QStringLiteral("audioConverter.confirmReplaceTitle"), QStringLiteral("Замена существующего файла")},
        {QStringLiteral("audioConverter.confirmReplaceMessage"),
         QStringLiteral("Файл с таким именем уже существует.\n\nОн будет безвозвратно заменен сконвертированным файлом. Продолжить?")},
        {QStringLiteral("audioConverter.notAvailable"), QStringLiteral("-")},
        {QStringLiteral("audioConverter.errorLocalOnly"), QStringLiteral("В этой версии можно конвертировать только локальные файлы.")},
        {QStringLiteral("audioConverter.errorInvalidOutputPath"), QStringLiteral("Выбран некорректный путь выходного файла.")},
        {QStringLiteral("audioConverter.errorTrackRequired"), QStringLiteral("Выберите один локальный трек для конвертации.")},
        {QStringLiteral("audioConverter.errorCueUnsupported"), QStringLiteral("Конвертация CUE-сегментов будет добавлена позже.")},
        {QStringLiteral("audioConverter.preflightSourceRequired"), QStringLiteral("Выберите один локальный трек для конвертации.")},
        {QStringLiteral("audioConverter.preflightSourceUnreadable"), QStringLiteral("Исходный файл отсутствует или недоступен для чтения: %1")},
        {QStringLiteral("audioConverter.preflightFormatUnsupported"), QStringLiteral("Выбранный выходной формат не поддерживается: %1")},
        {QStringLiteral("audioConverter.preflightOutputRequired"), QStringLiteral("Выберите путь выходного файла.")},
        {QStringLiteral("audioConverter.preflightOutputInvalidPath"), QStringLiteral("Укажите абсолютный локальный путь результата или выберите его через файловый диалог.")},
        {QStringLiteral("audioConverter.preflightOutputMatchesSource"), QStringLiteral("Выходной файл должен отличаться от исходного файла.")},
        {QStringLiteral("audioConverter.preflightOutputDirectoryMissing"), QStringLiteral("Выходная папка не существует: %1")},
        {QStringLiteral("audioConverter.preflightOutputDirectoryNotWritable"), QStringLiteral("Выходная папка недоступна для записи: %1")},
        {QStringLiteral("audioConverter.preflightOutputDirectoryWriteProbeFailed"), QStringLiteral("Не удалось записать в выходную папку: %1")},
        {QStringLiteral("audioConverter.preflightExistingOutputConfirm"), QStringLiteral("По этому пути уже есть файл. Подтвердите замену, чтобы продолжить: %1")},
        {QStringLiteral("audioConverter.preflightExistingOutputNotWritable"), QStringLiteral("Существующий выходной файл недоступен для записи: %1")},
        {QStringLiteral("audioConverter.preflightMissingPlugins"), QStringLiteral("Для формата %1 недоступны необходимые компоненты конвертации: %2")},
        {QStringLiteral("batchAudioConverter.title"), QStringLiteral("Пакетная конвертация аудио")},
        {QStringLiteral("batchAudioConverter.summaryLine"), QStringLiteral("Выбрано: %1, будет обработано: %2, пропущено: %3.")},
        {QStringLiteral("batchAudioConverter.summarySection"), QStringLiteral("Сводка партии")},
        {QStringLiteral("batchAudioConverter.summaryHint"), QStringLiteral("Проверьте состав очереди перед запуском.")},
        {QStringLiteral("batchAudioConverter.selectedCount"), QStringLiteral("Выбрано треков")},
        {QStringLiteral("batchAudioConverter.willProcess"), QStringLiteral("Будет обработано")},
        {QStringLiteral("batchAudioConverter.skippedCount"), QStringLiteral("Пропущено")},
        {QStringLiteral("batchAudioConverter.outputSection"), QStringLiteral("Выходная схема")},
        {QStringLiteral("batchAudioConverter.outputHint"), QStringLiteral("Предпросмотр выходных путей обновляется до запуска партии.")},
        {QStringLiteral("batchAudioConverter.presetsSection"), QStringLiteral("Переиспользуемые пресеты")},
        {QStringLiteral("batchAudioConverter.presetsHint"), QStringLiteral("Сохраняйте и повторно применяйте batch-настройки без runtime-состояния.")},
        {QStringLiteral("batchAudioConverter.preset"), QStringLiteral("Пресет")},
        {QStringLiteral("batchAudioConverter.noPresets"), QStringLiteral("Сохраненных пресетов пока нет")},
        {QStringLiteral("batchAudioConverter.saveAsPreset"), QStringLiteral("Сохранить как пресет")},
        {QStringLiteral("batchAudioConverter.applyPreset"), QStringLiteral("Применить пресет")},
        {QStringLiteral("batchAudioConverter.renamePreset"), QStringLiteral("Переименовать пресет")},
        {QStringLiteral("batchAudioConverter.deletePreset"), QStringLiteral("Удалить пресет")},
        {QStringLiteral("batchAudioConverter.presetNamePlaceholder"), QStringLiteral("Имя пресета")},
        {QStringLiteral("batchAudioConverter.presetNameRequired"), QStringLiteral("Нужно указать имя пресета.")},
        {QStringLiteral("batchAudioConverter.deletePresetTitle"), QStringLiteral("Удаление batch-пресета")},
        {QStringLiteral("batchAudioConverter.deletePresetMessage"), QStringLiteral("Удалить пресет \"%1\"?")},
        {QStringLiteral("batchAudioConverter.outputDirectory"), QStringLiteral("Выходная папка")},
        {QStringLiteral("batchAudioConverter.outputDirectoryPlaceholder"), QStringLiteral("Оставьте пустым, чтобы использовать папки исходников")},
        {QStringLiteral("batchAudioConverter.browseFolder"), QStringLiteral("Обзор...")},
        {QStringLiteral("batchAudioConverter.addFiles"), QStringLiteral("Добавить файлы...")},
        {QStringLiteral("batchAudioConverter.addFolder"), QStringLiteral("Добавить папку...")},
        {QStringLiteral("batchAudioConverter.useSourceFolders"), QStringLiteral("Папки исходников")},
        {QStringLiteral("batchAudioConverter.namingPolicy"), QStringLiteral("Правило именования")},
        {QStringLiteral("batchAudioConverter.namingBasename"), QStringLiteral("Базовое имя файла")},
        {QStringLiteral("batchAudioConverter.namingArtistTitle"), QStringLiteral("Исполнитель - Название")},
        {QStringLiteral("batchAudioConverter.namingAlbumTrackTitle"), QStringLiteral("Альбом - Трек - Название")},
        {QStringLiteral("batchAudioConverter.conflictPolicy"), QStringLiteral("Политика конфликтов")},
        {QStringLiteral("batchAudioConverter.conflictAutoRename"), QStringLiteral("Автопереименование")},
        {QStringLiteral("batchAudioConverter.conflictOverwrite"), QStringLiteral("Перезаписать, если можно")},
        {QStringLiteral("batchAudioConverter.conflictSkip"), QStringLiteral("Пропускать при конфликте")},
        {QStringLiteral("batchAudioConverter.conflictFail"), QStringLiteral("Считать конфликт ошибкой")},
        {QStringLiteral("batchAudioConverter.playlistMode"), QStringLiteral("Добавление в плейлист")},
        {QStringLiteral("batchAudioConverter.playlistModeImmediate"), QStringLiteral("Добавлять сразу")},
        {QStringLiteral("batchAudioConverter.playlistModeDeferred"), QStringLiteral("Добавить после завершения партии")},
        {QStringLiteral("batchAudioConverter.playlistModeDisabled"), QStringLiteral("Не добавлять в плейлист")},
        {QStringLiteral("batchAudioConverter.playlistModeHint"), QStringLiteral("Определяет, добавлять ли успешные результаты по мере завершения, после проверки или не добавлять вовсе.")},
        {QStringLiteral("batchAudioConverter.previewNamingPattern"), QStringLiteral("Политика имени: %1.")},
        {QStringLiteral("batchAudioConverter.previewFallbackPattern"), QStringLiteral("Откат к %1, потому что не хватает метаданных: %2.")},
        {QStringLiteral("batchAudioConverter.previewDirectorySourceFolder"), QStringLiteral("Выходная папка: папка исходника.")},
        {QStringLiteral("batchAudioConverter.previewDirectoryBatchOutput"), QStringLiteral("Выходная папка: общая batch-папка.")},
        {QStringLiteral("batchAudioConverter.previewCollisionPattern"), QStringLiteral("Обработка конфликта: %1.")},
        {QStringLiteral("batchAudioConverter.previewFinalizationPattern"), QStringLiteral("Режим записи: %1.")},
        {QStringLiteral("batchAudioConverter.previewCollisionPlanned"), QStringLiteral("конфликта нет")},
        {QStringLiteral("batchAudioConverter.previewCollisionAutoRenamed"), QStringLiteral("путь автопереименован, чтобы избежать конфликта")},
        {QStringLiteral("batchAudioConverter.previewCollisionOverwriteExisting"), QStringLiteral("существующий файл будет заменен")},
        {QStringLiteral("batchAudioConverter.previewCollisionSkipConflict"), QStringLiteral("элемент будет пропущен при конфликте")},
        {QStringLiteral("batchAudioConverter.previewCollisionFailConflict"), QStringLiteral("элемент завершится ошибкой при конфликте")},
        {QStringLiteral("batchAudioConverter.previewCollisionQueueConflict"), QStringLiteral("этот путь уже занят другим элементом очереди")},
        {QStringLiteral("batchAudioConverter.previewCollisionOverwriteBlocked"), QStringLiteral("перезапись заблокирована, потому что этот путь уже занят другим элементом очереди")},
        {QStringLiteral("batchAudioConverter.previewFinalizationTempCommit"), QStringLiteral("сначала временный файл, затем фиксация нового результата")},
        {QStringLiteral("batchAudioConverter.previewFinalizationTempReplace"), QStringLiteral("сначала временный файл, затем замена существующего результата")},
        {QStringLiteral("batchAudioConverter.previewFinalizationNotStarted"), QStringLiteral("выходной файл записываться не будет")},
        {QStringLiteral("batchAudioConverter.metadataArtist"), QStringLiteral("исполнитель")},
        {QStringLiteral("batchAudioConverter.metadataTitle"), QStringLiteral("название")},
        {QStringLiteral("batchAudioConverter.metadataAlbum"), QStringLiteral("альбом")},
        {QStringLiteral("batchAudioConverter.metadataTrackNumber"), QStringLiteral("номер трека")},
        {QStringLiteral("batchAudioConverter.addResultsToPlaylist"), QStringLiteral("Добавлять успешные результаты в текущий плейлист")},
        {QStringLiteral("batchAudioConverter.formatHint"), QStringLiteral("Эти параметры применяются ко всем элементам очереди.")},
        {QStringLiteral("batchAudioConverter.transformHint"), QStringLiteral("Скорость и тональность применяются ко всей партии.")},
        {QStringLiteral("batchAudioConverter.queueSection"), QStringLiteral("Предпросмотр очереди")},
        {QStringLiteral("batchAudioConverter.queueHint"), QStringLiteral("Каждый элемент показывает запланированный выходной путь и текущее состояние.")},
        {QStringLiteral("batchAudioConverter.removeSelected"), QStringLiteral("Удалить выбранные")},
        {QStringLiteral("batchAudioConverter.clearFailed"), QStringLiteral("Очистить ошибки")},
        {QStringLiteral("batchAudioConverter.clearCompleted"), QStringLiteral("Очистить завершенные")},
        {QStringLiteral("batchAudioConverter.retrySelected"), QStringLiteral("Повторить выбранные")},
        {QStringLiteral("batchAudioConverter.retryFailed"), QStringLiteral("Повторить ошибки")},
        {QStringLiteral("batchAudioConverter.retrySkipped"), QStringLiteral("Повторить пропущенные")},
        {QStringLiteral("batchAudioConverter.clearSelection"), QStringLiteral("Снять выделение")},
        {QStringLiteral("batchAudioConverter.filterAll"), QStringLiteral("Все")},
        {QStringLiteral("batchAudioConverter.filterPending"), QStringLiteral("Ожидают")},
        {QStringLiteral("batchAudioConverter.filterFailed"), QStringLiteral("Проблемные")},
        {QStringLiteral("batchAudioConverter.filterSucceeded"), QStringLiteral("Успешные")},
        {QStringLiteral("batchAudioConverter.viewCompact"), QStringLiteral("Компактно")},
        {QStringLiteral("batchAudioConverter.viewExpanded"), QStringLiteral("Подробно")},
        {QStringLiteral("batchAudioConverter.moveUp"), QStringLiteral("Вверх")},
        {QStringLiteral("batchAudioConverter.moveDown"), QStringLiteral("Вниз")},
        {QStringLiteral("batchAudioConverter.retry"), QStringLiteral("Повторить")},
        {QStringLiteral("batchAudioConverter.remove"), QStringLiteral("Удалить")},
        {QStringLiteral("batchAudioConverter.queueCompactPattern"), QStringLiteral("%1. %2")},
        {QStringLiteral("batchAudioConverter.runtimeSection"), QStringLiteral("Монитор выполнения")},
        {QStringLiteral("batchAudioConverter.runtimeHint"), QStringLiteral("Прогресс остаётся видимым после ошибок и после завершения.")},
        {QStringLiteral("batchAudioConverter.viewExpandedReport"), QStringLiteral("Развернутый отчет")},
        {QStringLiteral("batchAudioConverter.copyReport"), QStringLiteral("Скопировать отчет")},
        {QStringLiteral("batchAudioConverter.openOutputFolder"), QStringLiteral("Открыть папку результатов")},
        {QStringLiteral("batchAudioConverter.addSucceededOutputsToPlaylist"), QStringLiteral("Добавить успешные результаты в плейлист")},
        {QStringLiteral("batchAudioConverter.exportText"), QStringLiteral("Экспорт TXT")},
        {QStringLiteral("batchAudioConverter.exportJson"), QStringLiteral("Экспорт JSON")},
        {QStringLiteral("batchAudioConverter.exportCsv"), QStringLiteral("Экспорт CSV")},
        {QStringLiteral("batchAudioConverter.currentTrack"), QStringLiteral("Текущий трек: ")},
        {QStringLiteral("batchAudioConverter.noCurrentTrack"), QStringLiteral("Нет активного элемента")},
        {QStringLiteral("batchAudioConverter.currentTrackProgress"), QStringLiteral("Прогресс текущего элемента")},
        {QStringLiteral("batchAudioConverter.batchProgress"), QStringLiteral("Прогресс партии")},
        {QStringLiteral("batchAudioConverter.summaryDone"), QStringLiteral("Завершено %1 из %2. Успешно: %3. Ошибок: %4. Отменено: %5. Пропущено: %6.")},
        {QStringLiteral("batchAudioConverter.stickyFinalSummary"), QStringLiteral("Итог партии")},
        {QStringLiteral("batchAudioConverter.footerSummary"), QStringLiteral("В ожидании: %1. Выполняется: %2. Успешно: %3. Ошибок: %4.")},
        {QStringLiteral("batchAudioConverter.runtimeNoOutputFolder"), QStringLiteral("Папка результатов пока недоступна.")},
        {QStringLiteral("batchAudioConverter.runtimeOpenedOutputFolder"), QStringLiteral("Папка результатов открыта.")},
        {QStringLiteral("batchAudioConverter.runtimeFailedToOpenOutputFolder"), QStringLiteral("Не удалось открыть папку результатов.")},
        {QStringLiteral("batchAudioConverter.runtimeNoReportToCopy"), QStringLiteral("Готового отчета для копирования пока нет.")},
        {QStringLiteral("batchAudioConverter.runtimeCopiedReport"), QStringLiteral("Отчет партии скопирован в буфер обмена.")},
        {QStringLiteral("batchAudioConverter.runtimeFailedToCopyReport"), QStringLiteral("Не удалось скопировать отчет партии в буфер обмена.")},
        {QStringLiteral("batchAudioConverter.runtimeAddedSucceededOutputs"), QStringLiteral("В плейлист добавлено успешных результатов: %1.")},
        {QStringLiteral("batchAudioConverter.runtimeNoDeferredOutputs"), QStringLiteral("Отложенных результатов для добавления в плейлист больше нет.")},
        {QStringLiteral("batchAudioConverter.convertSelected"), QStringLiteral("Конвертировать выбранные")},
        {QStringLiteral("batchAudioConverter.selectInputFilesTitle"), QStringLiteral("Выберите файлы для пакетной конвертации")},
        {QStringLiteral("batchAudioConverter.selectInputFolderTitle"), QStringLiteral("Выберите папку для пакетной конвертации")},
        {QStringLiteral("batchAudioConverter.selectOutputFolderTitle"), QStringLiteral("Выберите выходную папку для партии")},
        {QStringLiteral("batchAudioConverter.errorSelectionRequired"), QStringLiteral("Выберите хотя бы один локальный файл для пакетной конвертации.")},
        {QStringLiteral("batchAudioConverter.errorInvalidSourceFolder"), QStringLiteral("Выбрана некорректная папка источников или в ней нет поддерживаемых файлов.")},
        {QStringLiteral("batchAudioConverter.errorInvalidOutputDirectory"), QStringLiteral("Выбрана некорректная выходная папка.")},
        {QStringLiteral("batchAudioConverter.statePending"), QStringLiteral("Ожидает")},
        {QStringLiteral("batchAudioConverter.stateRunning"), QStringLiteral("Выполняется")},
        {QStringLiteral("batchAudioConverter.stateSucceeded"), QStringLiteral("Успешно")},
        {QStringLiteral("batchAudioConverter.stateFailed"), QStringLiteral("Ошибка")},
        {QStringLiteral("batchAudioConverter.stateCanceled"), QStringLiteral("Отменено")},
        {QStringLiteral("batchAudioConverter.stateSkipped"), QStringLiteral("Пропущено")},
        {QStringLiteral("playlist.play"), QStringLiteral("Воспроизвести")},
        {QStringLiteral("playlist.playNext"), QStringLiteral("Воспроизвести следующим")},
        {QStringLiteral("playlist.addToQueue"), QStringLiteral("Добавить в очередь")},
        {QStringLiteral("playlist.clearQueue"), QStringLiteral("Очистить очередь")},
        {QStringLiteral("playlist.removeSelected"), QStringLiteral("Удалить выбранные")},
        {QStringLiteral("playlist.editTagsSelected"), QStringLiteral("Редактировать теги выбранных...")},
        {QStringLiteral("playlist.audioConverterSelected"), QStringLiteral("Конвертировать выбранные...")},
        {QStringLiteral("menu.importUrl"), QStringLiteral("Импорт по ссылке...")},
        {QStringLiteral("playlist.exportSelected"), QStringLiteral("Экспортировать выбранные...")},
        {QStringLiteral("playlist.moveToTrash"), QStringLiteral("Переместить в корзину")},
        {QStringLiteral("playlist.confirmTrashTitle"), QStringLiteral("Перемещение трека в корзину")},
        {QStringLiteral("playlist.confirmTrashMessage"),
         QStringLiteral("Файл трека будет перемещен в корзину и удален из плейлиста. Продолжить?")},
        {QStringLiteral("playlist.openInFileManager"), QStringLiteral("Показать в файловом менеджере")},
        {QStringLiteral("playlist.editTags"), QStringLiteral("Редактировать теги...")},
        {QStringLiteral("playlist.audioConverter"), QStringLiteral("Аудиоконвертер...")},
        {QStringLiteral("playlist.remove"), QStringLiteral("Удалить")},
        {QStringLiteral("tray.showHide"), QStringLiteral("Показать/Скрыть")},
        {QStringLiteral("tray.play"), QStringLiteral("Воспроизвести")},
        {QStringLiteral("tray.pause"), QStringLiteral("Пауза")},
        {QStringLiteral("tray.stop"), QStringLiteral("Стоп")},
        {QStringLiteral("tray.previous"), QStringLiteral("Предыдущий")},
        {QStringLiteral("tray.next"), QStringLiteral("Следующий")},
        {QStringLiteral("tray.settings"), QStringLiteral("Настройки...")},
        {QStringLiteral("tray.quit"), QStringLiteral("Выход")},
        {QStringLiteral("settings.skin"), QStringLiteral("Скин:")},
        {QStringLiteral("settings.skinNormal"), QStringLiteral("Обычный")},
        {QStringLiteral("settings.skinCompact"), QStringLiteral("Компактный")},
        {QStringLiteral("settings.skinDescription"), QStringLiteral("Компактный режим для небольших экранов")},
        {QStringLiteral("settings.waveformSection"), QStringLiteral("Волна")},
        {QStringLiteral("settings.themeSection"), QStringLiteral("Тема")},
        {QStringLiteral("settings.sectionAppearanceDescription"),
         QStringLiteral("Язык, режим скина и параметры компоновки интерфейса.")},
        {QStringLiteral("settings.sectionSystemDescription"),
         QStringLiteral("Поведение трея и подтверждения безопасных действий.")},
        {QStringLiteral("settings.sectionAudioDescription"),
         QStringLiteral("Управление воспроизведением, скорость/тон и режим перемешивания.")},
        {QStringLiteral("settings.sectionWaveformDescription"),
         QStringLiteral("Геометрия волны, подсказки и CUE-оверлеи.")},
        {QStringLiteral("settings.sectionColorsDescription"),
         QStringLiteral("Цвета волны, прогресса и акцента.")},
        {QStringLiteral("settings.sectionThemeDescription"),
         QStringLiteral("Предустановки темы и общий сброс оформления.")},
        {QStringLiteral("settings.searchPlaceholder"), QStringLiteral("Поиск настроек...")},
        {QStringLiteral("settings.quickActions"), QStringLiteral("Быстрые действия")},
        {QStringLiteral("settings.quickResetAudio"), QStringLiteral("Сбросить только аудио")},
        {QStringLiteral("settings.quickResetWaveform"), QStringLiteral("Сбросить только волну")},
        {QStringLiteral("settings.quickResetAll"), QStringLiteral("Сбросить всё к дефолту")},
        {QStringLiteral("settings.resetConfirmTitleAudio"), QStringLiteral("Подтвердите сброс аудио")},
        {QStringLiteral("settings.resetConfirmTitleWaveform"), QStringLiteral("Подтвердите сброс волны")},
        {QStringLiteral("settings.resetConfirmTitleAll"), QStringLiteral("Подтвердите полный сброс")},
        {QStringLiteral("settings.resetConfirmTitleTheme"), QStringLiteral("Подтвердите сброс темы")},
        {QStringLiteral("settings.resetConfirmMessage"),
         QStringLiteral("Перед применением сброса проверьте, что будет изменено:")},
        {QStringLiteral("settings.resetConfirmNoChanges"), QStringLiteral("Изменений не требуется.")},
        {QStringLiteral("settings.resetConfirmApply"), QStringLiteral("Применить сброс")},
        {QStringLiteral("settings.resetConfirmCancel"), QStringLiteral("Отмена")},
        {QStringLiteral("settings.valueEnabled"), QStringLiteral("Включено")},
        {QStringLiteral("settings.valueDisabled"), QStringLiteral("Выключено")},
        {QStringLiteral("settings.valueSystemDefault"), QStringLiteral("Системное значение")},
        {QStringLiteral("settings.waveformHeight"), QStringLiteral("Высота волны:")},
        {QStringLiteral("settings.compactWaveformHeight"), QStringLiteral("Высота волны (компакт):")},
        {QStringLiteral("settings.waveformZoomHintsVisible"), QStringLiteral("Показывать подсказки зума волны")},
        {QStringLiteral("settings.waveformZoomHintsDescription"),
         QStringLiteral("Показывать бейдж зума и подсказок при увеличении волны")},
        {QStringLiteral("settings.waveformCueOverlayEnabled"), QStringLiteral("Показывать CUE-сегменты на волне")},
        {QStringLiteral("settings.waveformCueOverlayEnabledDescription"),
         QStringLiteral("Отрисовывать области CUE-треков поверх волны для наглядной навигации")},
        {QStringLiteral("settings.waveformCueLabelsVisible"), QStringLiteral("Показывать подписи CUE-сегментов")},
        {QStringLiteral("settings.waveformCueLabelsVisibleDescription"),
         QStringLiteral("Показывать название и длительность CUE-сегмента внутри волны")},
        {QStringLiteral("settings.waveformCueAutoHideOnZoom"), QStringLiteral("Скрывать CUE-сегменты при зуме")},
        {QStringLiteral("settings.waveformCueAutoHideOnZoomDescription"),
         QStringLiteral("Автоматически скрывать CUE-оверлей во время зума волны и быстрого скраба")},
        {QStringLiteral("settings.speed"), QStringLiteral("Скорость:")},
        {QStringLiteral("settings.resetSpeed"), QStringLiteral("Сбросить скорость")},
        {QStringLiteral("settings.showSpeedPitch"), QStringLiteral("Показывать Скорость/Тон")},
        {QStringLiteral("settings.showSpeedPitchDescription"), QStringLiteral("Отображать слайдеры скорости и тона в панели управления")},
        {QStringLiteral("settings.reversePlayback"), QStringLiteral("Обратное проигрывание трека")},
        {QStringLiteral("settings.reversePlaybackDescription"),
         QStringLiteral("Проигрывать трек с конца к началу, сохраняя текущую скорость")},
        {QStringLiteral("settings.reversePlaybackUnavailableDescription"),
         QStringLiteral("Обратное проигрывание недоступно для этого backend-а или runtime-профиля.")},
        {QStringLiteral("settings.pitchUnavailableDescription"),
         QStringLiteral("Сдвиг тона недоступен для tracker-модулей при воспроизведении через OpenMPT.")},
        {QStringLiteral("settings.speedUnavailableDescription"),
         QStringLiteral("Изменение скорости недоступно для tracker-модулей при воспроизведении через OpenMPT.")},
        {QStringLiteral("settings.audioQualityProfile"), QStringLiteral("Профиль качества звука")},
        {QStringLiteral("settings.audioQualityProfileDescription"),
         QStringLiteral("Выберите характер обработки: Standard - сбалансированный, Hi-Fi - более чистый, Studio - максимально прозрачный.")},
        {QStringLiteral("settings.audioQualityProfileUnavailableDescription"),
         QStringLiteral("Для tracker-модулей используется профиль Standard, потому что воспроизведение через OpenMPT не использует цепочку мастеринга GStreamer.")},
        {QStringLiteral("settings.audioQualityStandard"), QStringLiteral("Standard")},
        {QStringLiteral("settings.audioQualityHiFi"), QStringLiteral("Hi-Fi")},
        {QStringLiteral("settings.audioQualityStudio"), QStringLiteral("Studio")},
        {QStringLiteral("settings.dynamicSpectrum"), QStringLiteral("Динамический анализатор")},
        {QStringLiteral("settings.dynamicSpectrumDescription"), QStringLiteral("Визуализация аудио в реальном времени (может влиять на производительность)")},
        {QStringLiteral("settings.dynamicSpectrumUnavailableDescription"),
         QStringLiteral("Анализатор спектра недоступен для текущего backend-а или отсутствует runtime support.")},
        {QStringLiteral("settings.deterministicShuffle"), QStringLiteral("Детерминированный порядок перемешивания")},
        {QStringLiteral("settings.deterministicShuffleDescription"),
         QStringLiteral("Использовать фиксированное начальное значение для воспроизводимого порядка перемешивания")},
        {QStringLiteral("settings.repeatableShuffle"), QStringLiteral("Повторять порядок между циклами")},
        {QStringLiteral("settings.repeatableShuffleDescription"),
         QStringLiteral("Если отключено, в каждом новом цикле будет создаваться новый порядок перемешивания")},
        {QStringLiteral("settings.shuffleSeedDependencyHint"),
         QStringLiteral("Доступно только при включённом детерминированном порядке перемешивания.")},
        {QStringLiteral("settings.repeatableShuffleDependencyHint"),
         QStringLiteral("Включите детерминированный порядок перемешивания, чтобы управлять повторяемостью.")},
        {QStringLiteral("settings.shuffleSeed"), QStringLiteral("Начальное значение перемешивания:")},
        {QStringLiteral("settings.regenerateSeed"), QStringLiteral("Сгенерировать")},
        {QStringLiteral("settings.waveformCueLabelsDependencyHint"),
         QStringLiteral("Включите CUE-оверлей сегментов для настройки подписей.")},
        {QStringLiteral("settings.waveformCueAutoHideDependencyHint"),
         QStringLiteral("Включите CUE-оверлей сегментов для настройки автоскрытия.")},
        {QStringLiteral("player.speed"), QStringLiteral("Скорость")},
        {QStringLiteral("player.pitch"), QStringLiteral("Тон")},
        // InfoSidebar
        {QStringLiteral("sidebar.spectrumAnalyzer"), QStringLiteral("АНАЛИЗАТОР СПЕКТРА")},
        {QStringLiteral("sidebar.technicalSpecs"), QStringLiteral("ХАРАКТЕРИСТИКИ")},
        {QStringLiteral("sidebar.engine"), QStringLiteral("Движок:")},
        {QStringLiteral("sidebar.engineValue"), QStringLiteral("FluxAudio")},
        {QStringLiteral("sidebar.codec"), QStringLiteral("Кодек:")},
        {QStringLiteral("sidebar.sampleRate"), QStringLiteral("Частота:")},
        {QStringLiteral("sidebar.bitrate"), QStringLiteral("Битрейт:")},
        {QStringLiteral("sidebar.bitDepth"), QStringLiteral("Глубина:")},
        {QStringLiteral("sidebar.bpm"), QStringLiteral("Удары в минуту:")},
        {QStringLiteral("sidebar.trackerModule"), QStringLiteral("TRACKER-МОДУЛЬ")},
        {QStringLiteral("sidebar.trackerType"), QStringLiteral("Трекер:")},
        {QStringLiteral("sidebar.trackerChannels"), QStringLiteral("Каналы:")},
        {QStringLiteral("sidebar.trackerPatterns"), QStringLiteral("Паттерны:")},
        {QStringLiteral("sidebar.trackerInstruments"), QStringLiteral("Инструменты:")},
        {QStringLiteral("sidebar.buffer"), QStringLiteral("Буфер:")},
        {QStringLiteral("sidebar.bufferValue"), QStringLiteral("512 МБ предзагружено")},
        {QStringLiteral("sidebar.albumArt"), QStringLiteral("ОБЛОЖКА")},
        {QStringLiteral("sidebar.unknown"), QStringLiteral("Неизвестно")},
        {QStringLiteral("sidebar.lossless"), QStringLiteral("Без потерь")},
        {QStringLiteral("sidebar.bitPcm"), QStringLiteral("-бит PCM")},
        // ControlBar
        {QStringLiteral("player.mute"), QStringLiteral("Без звука")},
        {QStringLiteral("player.maxVolume"), QStringLiteral("Максимум")},
        {QStringLiteral("player.equalizer"), QStringLiteral("Эквалайзер")},
        {QStringLiteral("player.equalizerUnavailable"), QStringLiteral("Эквалайзер недоступен")},
        {QStringLiteral("queue.open"), QStringLiteral("Открыть панель далее в очереди")},
        {QStringLiteral("queue.upNext"), QStringLiteral("Далее в очереди")},
        {QStringLiteral("queue.clear"), QStringLiteral("Очистить очередь")},
        {QStringLiteral("queue.empty"), QStringLiteral("Очередь пуста")},
        {QStringLiteral("equalizer.title"), QStringLiteral("Эквалайзер")},
        {QStringLiteral("equalizer.subtitle"), QStringLiteral("Параметрический EQ (equalizer-nbands)")},
        {QStringLiteral("equalizer.reset"), QStringLiteral("Сброс")},
        {QStringLiteral("equalizer.unavailable"), QStringLiteral("Плагин эквалайзера недоступен")},
        {QStringLiteral("equalizer.unavailableDescription"), QStringLiteral("Установите GStreamer плагин 'equalizer' (equalizer-nbands) для включения EQ.")},
        {QStringLiteral("equalizer.preset"), QStringLiteral("Пресет")},
        {QStringLiteral("equalizer.applyPreset"), QStringLiteral("Применить")},
        {QStringLiteral("equalizer.presetFlat"), QStringLiteral("Flat")},
        {QStringLiteral("equalizer.presetBassBoost"), QStringLiteral("Бас буст")},
        {QStringLiteral("equalizer.presetVocal"), QStringLiteral("Вокал")},
        {QStringLiteral("equalizer.presetHighBoost"), QStringLiteral("Верхние частоты")},
        {QStringLiteral("equalizer.presetRock"), QStringLiteral("Рок")},
        {QStringLiteral("equalizer.presetPop"), QStringLiteral("Поп")},
        {QStringLiteral("equalizer.presetJazz"), QStringLiteral("Джаз")},
        {QStringLiteral("equalizer.presetElectronic"), QStringLiteral("Электроника")},
        {QStringLiteral("equalizer.presetClassical"), QStringLiteral("Классика")},
        {QStringLiteral("equalizer.builtIn"), QStringLiteral("Встроенные")},
        {QStringLiteral("equalizer.user"), QStringLiteral("Пользовательские")},
        {QStringLiteral("equalizer.userEmpty"), QStringLiteral("Пользовательских пресетов пока нет.")},
        {QStringLiteral("equalizer.saveAs"), QStringLiteral("Сохранить как пресет")},
        {QStringLiteral("equalizer.rename"), QStringLiteral("Переименовать")},
        {QStringLiteral("equalizer.delete"), QStringLiteral("Удалить")},
        {QStringLiteral("equalizer.import"), QStringLiteral("Импорт")},
        {QStringLiteral("equalizer.export"), QStringLiteral("Экспорт")},
        {QStringLiteral("equalizer.portalTitleImport"), QStringLiteral("Импорт пресетов EQ (JSON)")},
        {QStringLiteral("equalizer.portalTitleExport"), QStringLiteral("Экспорт пресетов EQ (JSON)")},
        {QStringLiteral("equalizer.exportUser"), QStringLiteral("Экспорт пользовательских")},
        {QStringLiteral("equalizer.exportBundle"), QStringLiteral("Экспорт полного набора")},
        {QStringLiteral("equalizer.namePlaceholder"), QStringLiteral("Название пресета")},
        {QStringLiteral("equalizer.nameRequired"), QStringLiteral("Введите название пресета.")},
        {QStringLiteral("equalizer.errorPresetIdRequired"), QStringLiteral("Для экспорта требуется идентификатор пресета.")},
        {QStringLiteral("equalizer.errorInvalidImportPath"), QStringLiteral("Некорректный путь файла для импорта пресета.")},
        {QStringLiteral("equalizer.errorInvalidExportPath"), QStringLiteral("Некорректный путь файла для экспорта пресета.")},
        {QStringLiteral("equalizer.errorInvalidExportMode"), QStringLiteral("Некорректный режим экспорта пресета.")},
        {QStringLiteral("equalizer.errorExportFailed"), QStringLiteral("Не удалось экспортировать пресеты эквалайзера.")},
        {QStringLiteral("equalizer.mergeKeepBoth"), QStringLiteral("Слияние: сохранить оба")},
        {QStringLiteral("equalizer.mergeReplace"), QStringLiteral("Слияние: заменить существующие")},
        {QStringLiteral("equalizer.deleteConfirmTitle"), QStringLiteral("Удаление пресета")},
        {QStringLiteral("equalizer.deleteConfirmMessage"),
         QStringLiteral("Удалить пресет \"%1\"? Это действие нельзя отменить.")},
        {QStringLiteral("equalizer.exportDone"), QStringLiteral("Экспорт пресета завершен")},
        {QStringLiteral("equalizer.exportFailed"), QStringLiteral("Ошибка экспорта пресета")},
        {QStringLiteral("equalizer.exportPathLabel"), QStringLiteral("Путь")},
        {QStringLiteral("equalizer.exportCountLabel"), QStringLiteral("Пресеты")},
        {QStringLiteral("equalizer.hotkeysLegend"),
         QStringLiteral("Горячие клавиши: Открыть %1, Импорт %2, Экспорт %3")},
        {QStringLiteral("equalizer.shortcutImportTooltip"),
         QStringLiteral("Импорт пресетов (%1)")},
        {QStringLiteral("equalizer.shortcutExportTooltip"),
         QStringLiteral("Экспорт выбранного пресета (%1)")},
        {QStringLiteral("equalizer.importDone"), QStringLiteral("Импорт пресетов завершен")},
        {QStringLiteral("equalizer.importPartial"), QStringLiteral("Импорт пресетов завершен с проблемами")},
        {QStringLiteral("equalizer.importFailed"), QStringLiteral("Импорт пресетов не выполнен")},
        {QStringLiteral("equalizer.importSummary"), QStringLiteral("Сводка импорта")},
        {QStringLiteral("equalizer.importMergePolicy"), QStringLiteral("Политика слияния")},
        {QStringLiteral("equalizer.importImported"), QStringLiteral("Добавлено")},
        {QStringLiteral("equalizer.importReplaced"), QStringLiteral("Заменено")},
        {QStringLiteral("equalizer.importSkipped"), QStringLiteral("Пропущено")},
        {QStringLiteral("equalizer.importIssues"), QStringLiteral("Проблемы")},
        {QStringLiteral("xspf.importDone"), QStringLiteral("Импорт XSPF завершен")},
        {QStringLiteral("xspf.importPartial"), QStringLiteral("Импорт XSPF завершен с проблемами")},
        {QStringLiteral("xspf.importFailed"), QStringLiteral("Импорт XSPF не выполнен")},
        {QStringLiteral("xspf.importSummary"), QStringLiteral("Сводка импорта")},
        {QStringLiteral("xspf.importSource"), QStringLiteral("Плейлист: %1")},
        {QStringLiteral("xspf.importAdded"), QStringLiteral("Добавлено: %1")},
        {QStringLiteral("xspf.importSkipped"), QStringLiteral("Пропущено: %1")},
        {QStringLiteral("xspf.importUnknownSource"), QStringLiteral("неизвестный источник")},
        {QStringLiteral("equalizer.statusSuccess"), QStringLiteral("Успешно")},
        {QStringLiteral("equalizer.statusError"), QStringLiteral("Ошибка")},
        {QStringLiteral("equalizer.statusInfo"), QStringLiteral("Инфо")},
        {QStringLiteral("equalizer.statusDetails"), QStringLiteral("Детали")},
        // CompactSkin
        {QStringLiteral("compact.hidePlaylist"), QStringLiteral("Скрыть плейлист")},
        {QStringLiteral("compact.showPlaylist"), QStringLiteral("Показать плейлист")},
        // HeaderBar menu items
        {QStringLiteral("menu.file"), QStringLiteral("Файл")},
        {QStringLiteral("menu.edit"), QStringLiteral("Правка")},
        {QStringLiteral("menu.view"), QStringLiteral("Вид")},
        {QStringLiteral("menu.playback"), QStringLiteral("Воспроизведение")},
        {QStringLiteral("menu.library"), QStringLiteral("Библиотека")},
        {QStringLiteral("menu.help"), QStringLiteral("Справка")},
        {QStringLiteral("menu.openFiles"), QStringLiteral("Открыть файлы...")},
        {QStringLiteral("menu.addFolder"), QStringLiteral("Добавить папку...")},
        {QStringLiteral("menu.audioConverter"), QStringLiteral("Аудиоконвертер...")},
        {QStringLiteral("menu.exportPlaylist"), QStringLiteral("Экспорт плейлиста...")},
        {QStringLiteral("menu.clearPlaylist"), QStringLiteral("Очистить плейлист")},
        {QStringLiteral("menu.quit"), QStringLiteral("Выход")},
        {QStringLiteral("menu.find"), QStringLiteral("Найти")},
        {QStringLiteral("menu.selectAll"), QStringLiteral("Выбрать все")},
        {QStringLiteral("menu.clearSelection"), QStringLiteral("Снять выделение")},
        {QStringLiteral("menu.viewCollectionsPanel"), QStringLiteral("Панель коллекций")},
        {QStringLiteral("menu.viewInfoSidebar"), QStringLiteral("Инфо-панель")},
        {QStringLiteral("menu.viewSpeedPitch"), QStringLiteral("Управление скоростью/тоном")},
        {QStringLiteral("menu.profilerOverlay"), QStringLiteral("Оверлей профайлера")},
        {QStringLiteral("menu.profilerEnable"), QStringLiteral("Включить профайлер")},
        {QStringLiteral("menu.profilerReset"), QStringLiteral("Сбросить профайлер")},
        {QStringLiteral("menu.profilerExportJson"), QStringLiteral("Экспорт профайлера JSON")},
        {QStringLiteral("menu.profilerExportCsv"), QStringLiteral("Экспорт профайлера CSV")},
        {QStringLiteral("menu.profilerExportBundle"), QStringLiteral("Экспорт профайлера Bundle")},
        {QStringLiteral("menu.seekBack5"), QStringLiteral("Перемотка -5с")},
        {QStringLiteral("menu.seekForward5"), QStringLiteral("Перемотка +5с")},
        {QStringLiteral("menu.repeatMode"), QStringLiteral("Повтор")},
        {QStringLiteral("menu.newEmptyPlaylist"), QStringLiteral("Новый пустой плейлист")},
        // Help
        {QStringLiteral("help.about"), QStringLiteral("О программе")},
        {QStringLiteral("help.shortcuts"), QStringLiteral("Комбинации клавиш")},
        {QStringLiteral("help.aboutDialogTitle"), QStringLiteral("О WaveFlux")},
        {QStringLiteral("help.aboutAppName"), QStringLiteral("WaveFlux")},
        {QStringLiteral("help.aboutVersionLabel"), QStringLiteral("Версия:")},
        {QStringLiteral("help.aboutVersionValue"), QStringLiteral("1.2")},
        {QStringLiteral("help.aboutDescription"),
         QStringLiteral("WaveFlux — сфокусированный настольный аудиоплеер для локальной медиатеки и интернет-стримов с визуализацией волны, очередью и точным управлением воспроизведением.")},
        {QStringLiteral("help.aboutAuthorLabel"), QStringLiteral("Автор:")},
        {QStringLiteral("help.aboutAuthorName"), QStringLiteral("leocallidus")},
        {QStringLiteral("help.aboutAuthorUrl"), QStringLiteral("https://github.com/leocallidus")},
        {QStringLiteral("help.aboutYearLabel"), QStringLiteral("Год создания:")},
        {QStringLiteral("help.aboutYearValue"), QStringLiteral("2026")},
        {QStringLiteral("help.shortcutsDialogTitle"), QStringLiteral("Комбинации клавиш")},
        {QStringLiteral("help.shortcutsDialogSubtitle"), QStringLiteral("Справочник по клавиатурной навигации и быстрым действиям.")},
        {QStringLiteral("help.shortcutsColumnAction"), QStringLiteral("Действие")},
        {QStringLiteral("help.shortcutsColumnKeys"), QStringLiteral("Комбинация")},
        {QStringLiteral("help.shortcutsColumnContext"), QStringLiteral("Контекст")},
        {QStringLiteral("help.shortcutsGroupPlayback"), QStringLiteral("Воспроизведение")},
        {QStringLiteral("help.shortcutsGroupNavigation"), QStringLiteral("Навигация и интерфейс")},
        {QStringLiteral("help.shortcutsGroupPlaylist"), QStringLiteral("Плейлист и библиотека")},
        {QStringLiteral("help.shortcutsGroupProfiler"), QStringLiteral("Профайлер и служебные")},
        {QStringLiteral("help.shortcutsContextGlobal"), QStringLiteral("Глобально")},
        {QStringLiteral("help.shortcutsContextMainWindow"), QStringLiteral("Главное окно")},
        {QStringLiteral("help.shortcutsContextPlaylist"), QStringLiteral("Плейлист")},
        {QStringLiteral("help.shortcutsContextDialog"), QStringLiteral("Диалог")},
        {QStringLiteral("header.searchPlaceholder"), QStringLiteral("Поиск... title: artist: album: path:")},
        {QStringLiteral("header.searchManualPlaceholder"),
         QStringLiteral("Введите запрос и нажмите Enter или кнопку лупы")},
        {QStringLiteral("header.quickFilters"), QStringLiteral("Быстрые фильтры")},
        {QStringLiteral("header.filterAllFields"), QStringLiteral("Все поля")},
        {QStringLiteral("header.filterTitle"), QStringLiteral("Название")},
        {QStringLiteral("header.filterArtist"), QStringLiteral("Исполнитель")},
        {QStringLiteral("header.filterAlbum"), QStringLiteral("Альбом")},
        {QStringLiteral("header.filterPath"), QStringLiteral("Путь")},
        {QStringLiteral("header.filterLossless"), QStringLiteral("Без потерь")},
        {QStringLiteral("header.filterHiRes"), QStringLiteral("Hi-Res")},
        {QStringLiteral("header.filterReset"), QStringLiteral("Сбросить фильтры")},
        {QStringLiteral("header.menu"), QStringLiteral("Меню")},
        // PlaylistTable columns
        {QStringLiteral("table.title"), QStringLiteral("НАЗВАНИЕ")},
        {QStringLiteral("table.artist"), QStringLiteral("ИСПОЛНИТЕЛЬ")},
        {QStringLiteral("table.album"), QStringLiteral("АЛЬБОМ")},
        {QStringLiteral("table.duration"), QStringLiteral("ВРЕМЯ")},
        {QStringLiteral("table.bitrate"), QStringLiteral("БИТРЕЙТ")},
        // Smart collections
        {QStringLiteral("collections.sectionTitle"), QStringLiteral("КОЛЛЕКЦИИ")},
        {QStringLiteral("collections.openPanel"), QStringLiteral("Открыть коллекции")},
        {QStringLiteral("collections.currentPlaylist"), QStringLiteral("Текущий плейлист")},
        {QStringLiteral("playlists.sectionTitle"), QStringLiteral("ПЛЕЙЛИСТЫ")},
        {QStringLiteral("playlists.add"), QStringLiteral("Добавить плейлист")},
        {QStringLiteral("playlists.saveCurrent"), QStringLiteral("Сохранить текущий плейлист")},
        {QStringLiteral("playlists.name"), QStringLiteral("Название плейлиста")},
        {QStringLiteral("playlists.namePlaceholder"), QStringLiteral("Мой плейлист")},
        {QStringLiteral("playlists.save"), QStringLiteral("Сохранить")},
        {QStringLiteral("playlists.nameRequired"), QStringLiteral("Введите название плейлиста.")},
        {QStringLiteral("playlists.saveChanges"), QStringLiteral("Сохранить изменения")},
        {QStringLiteral("playlists.empty"), QStringLiteral("Сохраненных плейлистов пока нет.")},
        {QStringLiteral("playlists.emptyTracks"), QStringLiteral("В этом плейлисте нет треков.")},
        {QStringLiteral("playlists.tracks"), QStringLiteral("Треки")},
        {QStringLiteral("playlists.trackCount"), QStringLiteral("%1 треков")},
        {QStringLiteral("playlists.edit"), QStringLiteral("Редактировать плейлист")},
        {QStringLiteral("playlists.editTitle"), QStringLiteral("Редактирование плейлиста")},
        {QStringLiteral("playlists.duplicate"), QStringLiteral("Дублировать плейлист")},
        {QStringLiteral("playlists.moveUp"), QStringLiteral("Переместить выше")},
        {QStringLiteral("playlists.moveDown"), QStringLiteral("Переместить ниже")},
        {QStringLiteral("playlists.removeTrack"), QStringLiteral("Удалить трек")},
        {QStringLiteral("playlists.copySuffix"), QStringLiteral(" (копия)")},
        {QStringLiteral("playlists.rename"), QStringLiteral("Переименовать плейлист")},
        {QStringLiteral("playlists.renameTitle"), QStringLiteral("Переименовать плейлист")},
        {QStringLiteral("playlists.renameApply"), QStringLiteral("Переименовать")},
        {QStringLiteral("playlists.delete"), QStringLiteral("Удалить плейлист")},
        {QStringLiteral("playlists.deleteConfirmTitle"), QStringLiteral("Удалить плейлист")},
        {QStringLiteral("playlists.deleteConfirmMessage"),
         QStringLiteral("Удалить плейлист \"%1\"? Это действие нельзя отменить.")},
        {QStringLiteral("playlists.errorTitle"), QStringLiteral("Ошибка плейлиста")},
        {QStringLiteral("collections.create"), QStringLiteral("Создать")},
        {QStringLiteral("collections.delete"), QStringLiteral("Удалить")},
        {QStringLiteral("collections.deleteConfirmTitle"), QStringLiteral("Удалить коллекцию")},
        {QStringLiteral("collections.deleteConfirmMessage"),
         QStringLiteral("Удалить смарт-коллекцию \"%1\"? Это действие нельзя отменить.")},
        {QStringLiteral("collections.disabled"), QStringLiteral("Коллекции недоступны (SQLite-библиотека отключена).")},
        {QStringLiteral("collections.empty"), QStringLiteral("Смарт-коллекции пока не созданы.")},
        {QStringLiteral("collections.emptyTracks"), QStringLiteral("В этой коллекции нет треков.")},
        {QStringLiteral("collections.applyErrorTitle"), QStringLiteral("Ошибка коллекций")},
        {QStringLiteral("collections.createDialogTitle"), QStringLiteral("Создать умную коллекцию")},
        {QStringLiteral("collections.template"), QStringLiteral("Шаблон")},
        {QStringLiteral("collections.name"), QStringLiteral("Название")},
        {QStringLiteral("collections.namePlaceholder"), QStringLiteral("Название коллекции")},
        {QStringLiteral("collections.logic"), QStringLiteral("Логика")},
        {QStringLiteral("collections.logicAll"), QStringLiteral("Соответствуют всем правилам")},
        {QStringLiteral("collections.logicAny"), QStringLiteral("Соответствуют любому правилу")},
        {QStringLiteral("collections.rules"), QStringLiteral("Правила")},
        {QStringLiteral("collections.addRule"), QStringLiteral("Создать правило")},
        {QStringLiteral("collections.value"), QStringLiteral("Значение")},
        {QStringLiteral("collections.sort"), QStringLiteral("Сортировка")},
        {QStringLiteral("collections.sortAsc"), QStringLiteral("По возрастанию")},
        {QStringLiteral("collections.sortDesc"), QStringLiteral("По убыванию")},
        {QStringLiteral("collections.limit"), QStringLiteral("Лимит")},
        {QStringLiteral("collections.limitHint"), QStringLiteral("0 = без лимита")},
        {QStringLiteral("collections.cancel"), QStringLiteral("Отмена")},
        {QStringLiteral("collections.nameRequired"), QStringLiteral("Введите название коллекции.")},
        {QStringLiteral("collections.rulesRequired"), QStringLiteral("Добавьте хотя бы одно корректное правило.")},
        {QStringLiteral("collections.createFailed"), QStringLiteral("Не удалось создать смарт-коллекцию.")},
        {QStringLiteral("collections.enabled"), QStringLiteral("Включена")},
        {QStringLiteral("collections.pinned"), QStringLiteral("Закрепить")},
        {QStringLiteral("collections.boolTrue"), QStringLiteral("Да")},
        {QStringLiteral("collections.boolFalse"), QStringLiteral("Нет")},
        {QStringLiteral("collections.templateNone"), QStringLiteral("Шаблон: Пустой")},
        {QStringLiteral("collections.templateRecentlyAdded"), QStringLiteral("Шаблон: Недавно добавленные")},
        {QStringLiteral("collections.templateFrequentlyPlayed"), QStringLiteral("Шаблон: Часто слушаемые")},
        {QStringLiteral("collections.templateNeverPlayed"), QStringLiteral("Шаблон: Никогда не слушал")},
        {QStringLiteral("collections.templateHiRes"), QStringLiteral("Шаблон: Hi-Res")},
        {QStringLiteral("collections.fieldAllText"), QStringLiteral("Любое текстовое поле")},
        {QStringLiteral("collections.fieldTitle"), QStringLiteral("Название")},
        {QStringLiteral("collections.fieldArtist"), QStringLiteral("Исполнитель")},
        {QStringLiteral("collections.fieldAlbum"), QStringLiteral("Альбом")},
        {QStringLiteral("collections.fieldPath"), QStringLiteral("Путь")},
        {QStringLiteral("collections.fieldFormat"), QStringLiteral("Формат")},
        {QStringLiteral("collections.fieldAddedDays"), QStringLiteral("Дней с добавления")},
        {QStringLiteral("collections.fieldPlayCount"), QStringLiteral("Кол-во прослушиваний")},
        {QStringLiteral("collections.fieldSkipCount"), QStringLiteral("Кол-во пропусков")},
        {QStringLiteral("collections.fieldRating"), QStringLiteral("Оценка")},
        {QStringLiteral("collections.fieldSampleRate"), QStringLiteral("Частота дискретизации")},
        {QStringLiteral("collections.fieldBitDepth"), QStringLiteral("Битность")},
        {QStringLiteral("collections.fieldLastPlayedDays"), QStringLiteral("Дней с последнего прослушивания")},
        {QStringLiteral("collections.fieldFavorite"), QStringLiteral("Избранное")},
        {QStringLiteral("collections.fieldAddedAt"), QStringLiteral("Дата добавления")},
        {QStringLiteral("collections.fieldLastPlayedAt"), QStringLiteral("Дата последнего прослушивания")},
        {QStringLiteral("collections.opMatch"), QStringLiteral("совпадение")},
        {QStringLiteral("collections.opContains"), QStringLiteral("содержит")},
        {QStringLiteral("collections.opStartsWith"), QStringLiteral("начинается с")},
        {QStringLiteral("collections.opEq"), QStringLiteral("=")},
        {QStringLiteral("collections.opNe"), QStringLiteral("!=")},
        {QStringLiteral("collections.opGe"), QStringLiteral(">=")},
        {QStringLiteral("collections.opLe"), QStringLiteral("<=")},
        {QStringLiteral("collections.opGt"), QStringLiteral(">")},
        {QStringLiteral("collections.opLt"), QStringLiteral("<")}
    };
    return texts;
}

QVariantList defaultEqualizerBandGains()
{
    QVariantList gains;
    gains.reserve(10);
    for (int i = 0; i < 10; ++i) {
        gains.push_back(0.0);
    }
    return gains;
}

QVariantList normalizeEqualizerBandGains(const QVariantList &values)
{
    QVariantList normalized;
    normalized.reserve(10);
    for (int i = 0; i < 10; ++i) {
        const double source = (i < values.size()) ? values.at(i).toDouble() : 0.0;
        normalized.push_back(qBound(-24.0, source, 12.0));
    }
    return normalized;
}

bool equalizerBandGainsEqual(const QVariantList &a, const QVariantList &b)
{
    if (a.size() != b.size()) {
        return false;
    }

    for (int i = 0; i < a.size(); ++i) {
        if (qAbs(a.at(i).toDouble() - b.at(i).toDouble()) > 0.01) {
            return false;
        }
    }
    return true;
}

QVariantList normalizeEqualizerUserPresets(const QVariantList &values)
{
    QVariantList normalized;
    normalized.reserve(values.size());

    QSet<QString> usedIds;
    int nextGeneratedId = 1;
    for (const QVariant &value : values) {
        if (!value.canConvert<QVariantMap>()) {
            continue;
        }

        const QVariantMap source = value.toMap();
        QString id = source.value(QStringLiteral("id")).toString().trimmed();
        while (id.isEmpty() || usedIds.contains(id)) {
            id = QStringLiteral("user:migrated_%1").arg(nextGeneratedId++);
        }

        QString name = source.value(QStringLiteral("name")).toString().trimmed();
        if (name.isEmpty()) {
            name = QStringLiteral("Preset %1").arg(normalized.size() + 1);
        }

        QVariantMap preset;
        preset.insert(QStringLiteral("id"), id);
        preset.insert(QStringLiteral("name"), name);
        preset.insert(QStringLiteral("gains"),
                      normalizeEqualizerBandGains(source.value(QStringLiteral("gains")).toList()));
        preset.insert(QStringLiteral("builtIn"), false);
        preset.insert(QStringLiteral("updatedAtMs"),
                      source.value(QStringLiteral("updatedAtMs")).toLongLong());
        normalized.push_back(preset);
        usedIds.insert(id);
    }

    return normalized;
}

bool equalizerUserPresetsEqual(const QVariantList &a, const QVariantList &b)
{
    if (a.size() != b.size()) {
        return false;
    }

    for (int i = 0; i < a.size(); ++i) {
        if (a.at(i).toMap() != b.at(i).toMap()) {
            return false;
        }
    }
    return true;
}
} // namespace

AppSettingsManager::AppSettingsManager(QObject *parent)
    : QObject(parent)
    , m_settings(QStringLiteral("WaveFlux"), QStringLiteral("WaveFlux"))
{
    m_saveSettingsTimer.setSingleShot(true);
    m_saveSettingsTimer.setInterval(120);
    connect(&m_saveSettingsTimer, &QTimer::timeout, this, [this]() {
        if (m_saveSettingsPending) {
            saveSettings();
        }
    });

    KLocalizedString::setApplicationDomain("waveflux");
    loadSettings();
    applyLanguage();
}

AppSettingsManager::~AppSettingsManager()
{
    if (m_saveSettingsPending) {
        saveSettings();
    }
}

QString AppSettingsManager::translate(const QString &key) const
{
    const QHash<QString, QString> &primary =
        m_effectiveLanguage == QStringLiteral("ru") ? russianTexts() : englishTexts();
    auto primaryIt = primary.constFind(key);
    if (primaryIt != primary.constEnd()) {
        return primaryIt.value();
    }

    auto fallbackIt = englishTexts().constFind(key);
    if (fallbackIt != englishTexts().constEnd()) {
        return fallbackIt.value();
    }

    return key;
}

QString AppSettingsManager::translateForCurrentLanguage(const QString &key)
{
    QSettings settings(QStringLiteral("WaveFlux"), QStringLiteral("WaveFlux"));
    settings.beginGroup(QStringLiteral("App"));
    const QString language = normalizeLanguage(
        settings.value(QStringLiteral("language"), QStringLiteral("auto")).toString());
    settings.endGroup();

    const QHash<QString, QString> &primary =
        resolveLanguage(language) == QStringLiteral("ru") ? russianTexts() : englishTexts();
    auto primaryIt = primary.constFind(key);
    if (primaryIt != primary.constEnd()) {
        return primaryIt.value();
    }

    auto fallbackIt = englishTexts().constFind(key);
    if (fallbackIt != englishTexts().constEnd()) {
        return fallbackIt.value();
    }

    return key;
}

QStringList AppSettingsManager::supportedLanguages() const
{
    return {QStringLiteral("auto"), QStringLiteral("en"), QStringLiteral("ru")};
}

QVariantMap AppSettingsManager::loadPlaybackContextProgress() const
{
    QSettings settings(QStringLiteral("WaveFlux"), QStringLiteral("WaveFlux"));
    settings.beginGroup(QStringLiteral("App"));
    const QVariantMap payload = settings.value(QStringLiteral("playbackContextProgress")).toMap();
    settings.endGroup();
    return payload;
}

void AppSettingsManager::savePlaybackContextProgress(const QVariantMap &progress)
{
    m_settings.beginGroup(QStringLiteral("App"));
    m_settings.setValue(QStringLiteral("playbackContextProgress"), progress);
    m_settings.endGroup();
    m_settings.sync();
}

QVariantMap AppSettingsManager::loadNormalPlaylistSortState() const
{
    QVariantMap state;
    state.insert(QStringLiteral("column"),
                 m_settings.value(QStringLiteral("ui/normalPlaylistSortColumn"),
                                  QStringLiteral("none")).toString());
    state.insert(QStringLiteral("order"),
                 m_settings.value(QStringLiteral("ui/normalPlaylistSortOrder"), 0).toInt());
    return state;
}

void AppSettingsManager::saveNormalPlaylistSortState(const QVariantMap &state)
{
    const QString column = state.value(QStringLiteral("column"),
                                       QStringLiteral("none")).toString().trimmed();
    const int order = qBound(0, state.value(QStringLiteral("order"), 0).toInt(), 2);
    m_settings.setValue(QStringLiteral("ui/normalPlaylistSortColumn"),
                        column.isEmpty() ? QStringLiteral("none") : column);
    m_settings.setValue(QStringLiteral("ui/normalPlaylistSortOrder"), order);
    m_settings.sync();
}

QVariantMap AppSettingsManager::inspectYtDlpExecutable()
{
    const ToolInspectionResult result = inspectExternalTool(*this,
                                                            QStringLiteral("yt-dlp"),
                                                            QStringLiteral("yt-dlp"),
                                                            m_ytDlpExecutablePath);
    if (result.ok && m_ytDlpLastValidatedPath != result.resolvedPath) {
        m_ytDlpLastValidatedPath = result.resolvedPath;
        scheduleSaveSettings();
        emit ytDlpLastValidatedPathChanged();
    }
    return toolInspectionToVariantMap(result);
}

QVariantMap AppSettingsManager::inspectFfmpegExecutable()
{
    const ToolInspectionResult result = inspectExternalTool(*this,
                                                            QStringLiteral("ffmpeg"),
                                                            QStringLiteral("ffmpeg"),
                                                            m_ffmpegExecutablePath);
    if (result.ok && m_ffmpegLastValidatedPath != result.resolvedPath) {
        m_ffmpegLastValidatedPath = result.resolvedPath;
        scheduleSaveSettings();
        emit ffmpegLastValidatedPathChanged();
    }
    return toolInspectionToVariantMap(result);
}

QVariantMap AppSettingsManager::validateYtDlpImportRuntime(const QString &selectedFormat)
{
    const QString normalizedFormat = selectedFormat.trimmed().toLower();
    const bool requiresFfmpeg = normalizedFormat.isEmpty()
        || normalizedFormat == QStringLiteral("mp3")
        || normalizedFormat == QStringLiteral("m4a")
        || normalizedFormat == QStringLiteral("opus");

    const QVariantMap ytDlp = inspectYtDlpExecutable();
    const QVariantMap ffmpeg = requiresFfmpeg
        ? inspectFfmpegExecutable()
        : QVariantMap{
              {QStringLiteral("ok"), true},
              {QStringLiteral("toolKey"), QStringLiteral("ffmpeg")},
              {QStringLiteral("status"), QStringLiteral("skipped")},
              {QStringLiteral("source"), QStringLiteral("none")},
              {QStringLiteral("configuredPath"), m_ffmpegExecutablePath},
              {QStringLiteral("resolvedPath"), QString()},
              {QStringLiteral("version"), QString()},
              {QStringLiteral("errorCategory"), QString()},
              {QStringLiteral("errorCode"), QString()},
              {QStringLiteral("message"), QString()}
          };

    QVariantList dependencyErrors;
    if (!ytDlp.value(QStringLiteral("ok")).toBool()) {
        dependencyErrors.push_back(ytDlp);
    }
    if (requiresFfmpeg && !ffmpeg.value(QStringLiteral("ok")).toBool()) {
        dependencyErrors.push_back(ffmpeg);
    }

    QVariantMap result;
    result.insert(QStringLiteral("ok"), dependencyErrors.isEmpty());
    result.insert(QStringLiteral("status"),
                  dependencyErrors.isEmpty()
                      ? QStringLiteral("ready")
                      : QStringLiteral("missing-dependency"));
    result.insert(QStringLiteral("selectedFormat"), normalizedFormat);
    result.insert(QStringLiteral("requiresFfmpeg"), requiresFfmpeg);
    result.insert(QStringLiteral("versionPolicy"), QStringLiteral("diagnostic-only"));
    result.insert(QStringLiteral("ytDlp"), ytDlp);
    result.insert(QStringLiteral("ffmpeg"), ffmpeg);
    result.insert(QStringLiteral("dependencyErrors"), dependencyErrors);
    return result;
}

void AppSettingsManager::setLanguage(const QString &language)
{
    const QString normalized = normalizeLanguage(language);
    if (m_language == normalized) {
        return;
    }

    m_language = normalized;
    emit languageChanged();
    scheduleSaveSettings();
    applyLanguage();
}

void AppSettingsManager::setTrayEnabled(bool enabled)
{
    if (m_trayEnabled == enabled) {
        return;
    }

    m_trayEnabled = enabled;
    emit trayEnabledChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setSidebarVisible(bool visible)
{
    if (m_sidebarVisible == visible) {
        return;
    }

    m_sidebarVisible = visible;
    emit sidebarVisibleChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setCollectionsSidebarVisible(bool visible)
{
    if (m_collectionsSidebarVisible == visible) {
        return;
    }

    m_collectionsSidebarVisible = visible;
    emit collectionsSidebarVisibleChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setSkinMode(const QString &mode)
{
    const QString normalized = (mode == QStringLiteral("compact")) ? mode : QStringLiteral("normal");
    if (m_skinMode == normalized) {
        return;
    }

    m_skinMode = normalized;
    emit skinModeChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setWaveformHeight(int height)
{
    const int clamped = qBound(40, height, 1000);
    if (m_waveformHeight == clamped) {
        return;
    }

    m_waveformHeight = clamped;
    emit waveformHeightChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setCompactWaveformHeight(int height)
{
    const int clamped = qBound(24, height, 1000);
    if (m_compactWaveformHeight == clamped) {
        return;
    }

    m_compactWaveformHeight = clamped;
    emit compactWaveformHeightChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setWaveformZoomHintsVisible(bool visible)
{
    if (m_waveformZoomHintsVisible == visible) {
        return;
    }

    m_waveformZoomHintsVisible = visible;
    emit waveformZoomHintsVisibleChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setCueWaveformOverlayEnabled(bool enabled)
{
    if (m_cueWaveformOverlayEnabled == enabled) {
        return;
    }

    m_cueWaveformOverlayEnabled = enabled;
    emit cueWaveformOverlayEnabledChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setCueWaveformOverlayLabelsEnabled(bool enabled)
{
    if (m_cueWaveformOverlayLabelsEnabled == enabled) {
        return;
    }

    m_cueWaveformOverlayLabelsEnabled = enabled;
    emit cueWaveformOverlayLabelsEnabledChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setCueWaveformOverlayAutoHideOnZoom(bool enabled)
{
    if (m_cueWaveformOverlayAutoHideOnZoom == enabled) {
        return;
    }

    m_cueWaveformOverlayAutoHideOnZoom = enabled;
    emit cueWaveformOverlayAutoHideOnZoomChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setShowSpeedPitchControls(bool show)
{
    if (m_showSpeedPitchControls == show) {
        return;
    }

    m_showSpeedPitchControls = show;
    emit showSpeedPitchControlsChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setReversePlayback(bool enabled)
{
    if (m_reversePlayback == enabled) {
        return;
    }

    m_reversePlayback = enabled;
    emit reversePlaybackChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setAudioQualityProfile(const QString &profile)
{
    const QString normalized = normalizeAudioQualityProfile(profile);
    if (m_audioQualityProfile == normalized) {
        return;
    }

    m_audioQualityProfile = normalized;
    emit audioQualityProfileChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setDynamicSpectrum(bool enabled)
{
    if (m_dynamicSpectrum == enabled) {
        return;
    }

    m_dynamicSpectrum = enabled;
    emit dynamicSpectrumChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setConfirmTrashDeletion(bool enabled)
{
    if (m_confirmTrashDeletion == enabled) {
        return;
    }

    m_confirmTrashDeletion = enabled;
    emit confirmTrashDeletionChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setAutomaticPlaylistSearch(bool enabled)
{
    if (m_automaticPlaylistSearch == enabled) {
        return;
    }

    m_automaticPlaylistSearch = enabled;
    emit automaticPlaylistSearchChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setAutoAddTracksFromPlaylistFolder(bool enabled)
{
    if (m_autoAddTracksFromPlaylistFolder == enabled) {
        return;
    }

    m_autoAddTracksFromPlaylistFolder = enabled;
    emit autoAddTracksFromPlaylistFolderChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setDeterministicShuffleEnabled(bool enabled)
{
    if (m_deterministicShuffleEnabled == enabled) {
        return;
    }

    m_deterministicShuffleEnabled = enabled;
    emit deterministicShuffleEnabledChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setShuffleSeed(quint32 seed)
{
    if (m_shuffleSeed == seed) {
        return;
    }

    m_shuffleSeed = seed;
    emit shuffleSeedChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setRepeatableShuffle(bool enabled)
{
    if (m_repeatableShuffle == enabled) {
        return;
    }

    m_repeatableShuffle = enabled;
    emit repeatableShuffleChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setSqliteLibraryEnabled(bool enabled)
{
    if (m_sqliteLibraryEnabled == enabled) {
        return;
    }

    m_sqliteLibraryEnabled = enabled;
    emit sqliteLibraryEnabledChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setYtDlpExecutablePath(const QString &path)
{
    const QString normalized = normalizeExecutablePath(path);
    if (m_ytDlpExecutablePath == normalized) {
        return;
    }

    m_ytDlpExecutablePath = normalized;
    emit ytDlpExecutablePathChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setFfmpegExecutablePath(const QString &path)
{
    const QString normalized = normalizeExecutablePath(path);
    if (m_ffmpegExecutablePath == normalized) {
        return;
    }

    m_ffmpegExecutablePath = normalized;
    emit ffmpegExecutablePathChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setEqualizerBandGains(const QVariantList &gains)
{
    const QVariantList normalized = normalizeEqualizerBandGains(gains);
    const bool gainsChanged = !equalizerBandGainsEqual(m_equalizerBandGains, normalized);
    const bool lastManualChanged = !equalizerBandGainsEqual(m_equalizerLastManualGains, normalized);
    if (!gainsChanged && !lastManualChanged) {
        return;
    }

    m_equalizerBandGains = normalized;
    m_equalizerLastManualGains = normalized;
    if (gainsChanged) {
        emit equalizerBandGainsChanged();
    }
    if (lastManualChanged) {
        emit equalizerLastManualGainsChanged();
    }
    scheduleSaveSettings();
}

void AppSettingsManager::setEqualizerUserPresets(const QVariantList &presets)
{
    const QVariantList normalized = normalizeEqualizerUserPresets(presets);
    if (equalizerUserPresetsEqual(m_equalizerUserPresets, normalized)) {
        return;
    }

    m_equalizerUserPresets = normalized;
    emit equalizerUserPresetsChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setEqualizerActivePresetId(const QString &presetId)
{
    const QString normalized = presetId.trimmed();
    if (m_equalizerActivePresetId == normalized) {
        return;
    }

    m_equalizerActivePresetId = normalized;
    emit equalizerActivePresetIdChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setEqualizerLastManualGains(const QVariantList &gains)
{
    const QVariantList normalized = normalizeEqualizerBandGains(gains);
    const bool lastManualChanged = !equalizerBandGainsEqual(m_equalizerLastManualGains, normalized);
    const bool gainsChanged = !equalizerBandGainsEqual(m_equalizerBandGains, normalized);
    if (!lastManualChanged && !gainsChanged) {
        return;
    }

    m_equalizerLastManualGains = normalized;
    m_equalizerBandGains = normalized;
    if (lastManualChanged) {
        emit equalizerLastManualGainsChanged();
    }
    if (gainsChanged) {
        emit equalizerBandGainsChanged();
    }
    scheduleSaveSettings();
}

void AppSettingsManager::setBatchAudioConverterLastSettings(const QVariantMap &settings)
{
    const QVariantMap normalized = normalizeBatchAudioConverterLastSettings(settings);
    if (m_batchAudioConverterLastSettings == normalized) {
        return;
    }

    m_batchAudioConverterLastSettings = normalized;
    emit batchAudioConverterLastSettingsChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setBatchAudioConverterUserPresets(const QVariantList &presets)
{
    const QVariantList normalized = normalizeBatchAudioConverterUserPresets(presets);
    if (m_batchAudioConverterUserPresets == normalized) {
        return;
    }

    m_batchAudioConverterUserPresets = normalized;
    emit batchAudioConverterUserPresetsChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setBatchAudioConverterDraft(const QVariantMap &draft)
{
    const QVariantMap normalized = normalizeBatchAudioConverterDraft(draft);
    if (m_batchAudioConverterDraft == normalized) {
        return;
    }

    m_batchAudioConverterDraft = normalized;
    emit batchAudioConverterDraftChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setBatchAudioConverterFinishedJobs(const QVariantList &jobs)
{
    const QVariantList normalized = normalizeBatchAudioConverterFinishedJobs(jobs);
    if (m_batchAudioConverterFinishedJobs == normalized) {
        return;
    }

    m_batchAudioConverterFinishedJobs = normalized;
    emit batchAudioConverterFinishedJobsChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setYtDlpImportLastSettings(const QVariantMap &settings)
{
    const QVariantMap normalized = normalizeYtDlpImportLastSettings(settings);
    if (m_ytDlpImportLastSettings == normalized) {
        return;
    }

    m_ytDlpImportLastSettings = normalized;
    emit ytDlpImportLastSettingsChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setYtDlpImportDraft(const QVariantMap &draft)
{
    const QVariantMap normalized = normalizeYtDlpImportDraft(draft);
    if (m_ytDlpImportDraft == normalized) {
        return;
    }

    m_ytDlpImportDraft = normalized;
    emit ytDlpImportDraftChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setYtDlpImportRecentSources(const QVariantList &sources)
{
    const QVariantList normalized = normalizeYtDlpImportRecentSources(sources);
    if (m_ytDlpImportRecentSources == normalized) {
        return;
    }

    m_ytDlpImportRecentSources = normalized;
    emit ytDlpImportRecentSourcesChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setYtDlpImportRecentCanonicalSources(const QVariantList &sources)
{
    const QVariantList normalized = normalizeYtDlpImportRecentSources(sources);
    if (m_ytDlpImportRecentCanonicalSources == normalized) {
        return;
    }

    m_ytDlpImportRecentCanonicalSources = normalized;
    emit ytDlpImportRecentCanonicalSourcesChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setYtDlpImportRecentOutputDirectories(const QVariantList &directories)
{
    const QVariantList normalized = normalizeYtDlpImportRecentOutputDirectories(directories);
    if (m_ytDlpImportRecentOutputDirectories == normalized) {
        return;
    }

    m_ytDlpImportRecentOutputDirectories = normalized;
    emit ytDlpImportRecentOutputDirectoriesChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::loadSettings()
{
    m_settings.beginGroup("App");
    m_language = normalizeLanguage(m_settings.value("language", QStringLiteral("auto")).toString());
    m_trayEnabled = m_settings.value("trayEnabled", false).toBool();
    m_sidebarVisible = m_settings.value("sidebarVisible", true).toBool();
    m_collectionsSidebarVisible = m_settings.value("collectionsSidebarVisible", true).toBool();
    const QString skinValue = m_settings.value("skinMode", QStringLiteral("normal")).toString();
    m_skinMode = (skinValue == QStringLiteral("compact")) ? skinValue : QStringLiteral("normal");
    m_waveformHeight = qBound(40, m_settings.value("waveformHeight", 100).toInt(), 1000);
    m_compactWaveformHeight = qBound(24, m_settings.value("compactWaveformHeight", 32).toInt(), 1000);
    m_waveformZoomHintsVisible = m_settings.value("waveform.zoomHintsVisible", true).toBool();
    m_cueWaveformOverlayEnabled = m_settings.value("waveform.cueOverlayEnabled", true).toBool();
    m_cueWaveformOverlayLabelsEnabled = m_settings.value("waveform.cueOverlayLabelsEnabled", true).toBool();
    m_cueWaveformOverlayAutoHideOnZoom = m_settings.value("waveform.cueOverlayAutoHideOnZoom", true).toBool();
    m_showSpeedPitchControls = m_settings.value("showSpeedPitchControls", false).toBool();
    m_reversePlayback = m_settings.value("reversePlayback", false).toBool();
    m_audioQualityProfile =
        normalizeAudioQualityProfile(m_settings.value("audioQualityProfile", QStringLiteral("standard")).toString());
    m_dynamicSpectrum = m_settings.value("dynamicSpectrum", false).toBool();
    m_confirmTrashDeletion = m_settings.value("confirmTrashDeletion", true).toBool();
    m_automaticPlaylistSearch = m_settings.value("automaticPlaylistSearch", false).toBool();
    m_autoAddTracksFromPlaylistFolder =
        m_settings.value("autoAddTracksFromPlaylistFolder", true).toBool();
    m_deterministicShuffleEnabled = m_settings.value("deterministicShuffleEnabled", false).toBool();
    m_shuffleSeed = normalizeShuffleSeed(
        m_settings.value("shuffleSeed", static_cast<qulonglong>(kDefaultShuffleSeed)));
    m_repeatableShuffle = m_settings.value("repeatableShuffle", true).toBool();
    if (m_settings.contains("library.sqlite.enabled")) {
        m_sqliteLibraryEnabled = m_settings.value("library.sqlite.enabled").toBool();
    } else {
        m_sqliteLibraryEnabled = m_settings.value("sqliteLibraryEnabled", true).toBool();
    }
    m_ytDlpExecutablePath = normalizeExecutablePath(
        m_settings.value("ytDlp.executablePath", QString()).toString());
    m_ffmpegExecutablePath = normalizeExecutablePath(
        m_settings.value("ffmpeg.executablePath", QString()).toString());
    m_ytDlpLastValidatedPath = normalizeExecutablePath(
        m_settings.value("ytDlp.lastValidatedPath", QString()).toString());
    m_ffmpegLastValidatedPath = normalizeExecutablePath(
        m_settings.value("ffmpeg.lastValidatedPath", QString()).toString());
    const QVariantList legacyEqualizerBandGains = normalizeEqualizerBandGains(
        m_settings.value("equalizerBandGains", defaultEqualizerBandGains()).toList());
    m_equalizerLastManualGains = normalizeEqualizerBandGains(
        m_settings.value("equalizer.lastManualGains", legacyEqualizerBandGains).toList());
    m_equalizerBandGains = m_equalizerLastManualGains;
    m_equalizerUserPresets = normalizeEqualizerUserPresets(
        m_settings.value("equalizer.userPresets", QVariantList()).toList());
    m_equalizerActivePresetId =
        m_settings.value("equalizer.activePresetId", QString()).toString().trimmed();
    m_batchAudioConverterLastSettings = normalizeBatchAudioConverterLastSettings(
        m_settings.value("batchAudioConverter.lastSettings", QVariantMap()).toMap());
    m_batchAudioConverterUserPresets = normalizeBatchAudioConverterUserPresets(
        m_settings.value("batchAudioConverter.userPresets", QVariantList()).toList());
    m_batchAudioConverterDraft = normalizeBatchAudioConverterDraft(
        m_settings.value("batchAudioConverter.draft", QVariantMap()).toMap());
    m_batchAudioConverterFinishedJobs = normalizeBatchAudioConverterFinishedJobs(
        m_settings.value("batchAudioConverter.finishedJobs", QVariantList()).toList());
    m_ytDlpImportLastSettings = normalizeYtDlpImportLastSettings(
        m_settings.value("ytDlpImport.lastSettings", QVariantMap()).toMap());
    m_ytDlpImportDraft = normalizeYtDlpImportDraft(
        m_settings.value("ytDlpImport.draft", QVariantMap()).toMap());
    m_ytDlpImportRecentSources = normalizeYtDlpImportRecentSources(
        m_settings.value("ytDlpImport.recentSources", QVariantList()).toList());
    m_ytDlpImportRecentCanonicalSources = normalizeYtDlpImportRecentSources(
        m_settings.value("ytDlpImport.recentCanonicalSources", QVariantList()).toList());
    m_ytDlpImportRecentOutputDirectories = normalizeYtDlpImportRecentOutputDirectories(
        m_settings.value("ytDlpImport.recentOutputDirectories", QVariantList()).toList());
    m_settings.endGroup();
}

void AppSettingsManager::scheduleSaveSettings()
{
    m_saveSettingsPending = true;
    m_saveSettingsTimer.start();
}

void AppSettingsManager::saveSettings()
{
    m_saveSettingsPending = false;
    if (m_saveSettingsTimer.isActive()) {
        m_saveSettingsTimer.stop();
    }

    m_settings.beginGroup("App");
    m_settings.setValue("language", m_language);
    m_settings.setValue("trayEnabled", m_trayEnabled);
    m_settings.setValue("sidebarVisible", m_sidebarVisible);
    m_settings.setValue("collectionsSidebarVisible", m_collectionsSidebarVisible);
    m_settings.setValue("skinMode", m_skinMode);
    m_settings.setValue("waveformHeight", m_waveformHeight);
    m_settings.setValue("compactWaveformHeight", m_compactWaveformHeight);
    m_settings.setValue("waveform.zoomHintsVisible", m_waveformZoomHintsVisible);
    m_settings.setValue("waveform.cueOverlayEnabled", m_cueWaveformOverlayEnabled);
    m_settings.setValue("waveform.cueOverlayLabelsEnabled", m_cueWaveformOverlayLabelsEnabled);
    m_settings.setValue("waveform.cueOverlayAutoHideOnZoom", m_cueWaveformOverlayAutoHideOnZoom);
    m_settings.setValue("showSpeedPitchControls", m_showSpeedPitchControls);
    m_settings.setValue("reversePlayback", m_reversePlayback);
    m_settings.setValue("audioQualityProfile", m_audioQualityProfile);
    m_settings.setValue("dynamicSpectrum", m_dynamicSpectrum);
    m_settings.setValue("confirmTrashDeletion", m_confirmTrashDeletion);
    m_settings.setValue("automaticPlaylistSearch", m_automaticPlaylistSearch);
    m_settings.setValue("autoAddTracksFromPlaylistFolder", m_autoAddTracksFromPlaylistFolder);
    m_settings.setValue("deterministicShuffleEnabled", m_deterministicShuffleEnabled);
    m_settings.setValue("shuffleSeed", static_cast<qulonglong>(m_shuffleSeed));
    m_settings.setValue("repeatableShuffle", m_repeatableShuffle);
    m_settings.setValue("library.sqlite.enabled", m_sqliteLibraryEnabled);
    m_settings.setValue("ytDlp.executablePath", m_ytDlpExecutablePath);
    m_settings.setValue("ffmpeg.executablePath", m_ffmpegExecutablePath);
    m_settings.setValue("ytDlp.lastValidatedPath", m_ytDlpLastValidatedPath);
    m_settings.setValue("ffmpeg.lastValidatedPath", m_ffmpegLastValidatedPath);
    m_settings.setValue("equalizerBandGains", m_equalizerBandGains); // legacy compatibility key
    m_settings.setValue("equalizer.lastManualGains", m_equalizerLastManualGains);
    m_settings.setValue("equalizer.userPresets", m_equalizerUserPresets);
    m_settings.setValue("equalizer.activePresetId", m_equalizerActivePresetId);
    m_settings.setValue("batchAudioConverter.lastSettings", m_batchAudioConverterLastSettings);
    m_settings.setValue("batchAudioConverter.userPresets", m_batchAudioConverterUserPresets);
    m_settings.setValue("batchAudioConverter.draft", m_batchAudioConverterDraft);
    m_settings.setValue("batchAudioConverter.finishedJobs", m_batchAudioConverterFinishedJobs);
    m_settings.setValue("ytDlpImport.lastSettings", m_ytDlpImportLastSettings);
    m_settings.setValue("ytDlpImport.draft", m_ytDlpImportDraft);
    m_settings.setValue("ytDlpImport.recentSources", m_ytDlpImportRecentSources);
    m_settings.setValue("ytDlpImport.recentCanonicalSources", m_ytDlpImportRecentCanonicalSources);
    m_settings.setValue("ytDlpImport.recentOutputDirectories", m_ytDlpImportRecentOutputDirectories);
    m_settings.endGroup();
    m_settings.sync();
}

void AppSettingsManager::applyLanguage()
{
    const QString resolved = resolveLanguage(m_language);
    KLocalizedString::setLanguages(QStringList{resolved});

    if (resolved != m_effectiveLanguage) {
        m_effectiveLanguage = resolved;
        emit effectiveLanguageChanged();
    }

    ++m_translationRevision;
    emit translationsChanged();
}

QString AppSettingsManager::normalizeLanguage(const QString &language)
{
    const QString normalized = language.trimmed().toLower();
    if (normalized == QStringLiteral("en") ||
        normalized == QStringLiteral("ru") ||
        normalized == QStringLiteral("auto")) {
        return normalized;
    }
    return QStringLiteral("auto");
}

QString AppSettingsManager::resolveLanguage(const QString &language)
{
    if (language == QStringLiteral("en") || language == QStringLiteral("ru")) {
        return language;
    }

    const QString system = QLocale::system().name().left(2).toLower();
    if (system == QStringLiteral("ru")) {
        return QStringLiteral("ru");
    }
    return QStringLiteral("en");
}

QString AppSettingsManager::normalizeAudioQualityProfile(const QString &profile)
{
    const QString normalized = profile.trimmed().toLower();
    if (normalized == QStringLiteral("hifi") || normalized == QStringLiteral("hi-fi")) {
        return QStringLiteral("hifi");
    }
    if (normalized == QStringLiteral("studio")) {
        return QStringLiteral("studio");
    }
    return QStringLiteral("standard");
}

QString AppSettingsManager::normalizeExecutablePath(const QString &path)
{
    return normalizeExecutablePathValue(path);
}

QVariantMap AppSettingsManager::normalizeBatchAudioConverterLastSettings(const QVariantMap &settings)
{
    QVariantMap normalized;
    normalized.insert(QStringLiteral("outputDirectory"),
                      settings.value(QStringLiteral("outputDirectory")).toString().trimmed());
    normalized.insert(QStringLiteral("namingPolicy"),
                      normalizeBatchNamingPolicy(settings.value(QStringLiteral("namingPolicy")).toString()));
    normalized.insert(QStringLiteral("format"),
                      normalizeBatchFormat(settings.value(QStringLiteral("format")).toString()));
    normalized.insert(QStringLiteral("conflictPolicy"),
                      normalizeBatchConflictPolicy(settings.value(QStringLiteral("conflictPolicy")).toString()));
    normalized.insert(QStringLiteral("retryPolicy"),
                      normalizeBatchRetryPolicy(settings.value(QStringLiteral("retryPolicy")).toString()));
    normalized.insert(QStringLiteral("playlistAddMode"),
                      normalizeBatchPlaylistAddMode(
                          settings.value(QStringLiteral("playlistAddMode")).toString(),
                          settings.value(QStringLiteral("addResultsToPlaylist"), true).toBool()));
    normalized.insert(QStringLiteral("bitrate"),
                      normalizeBatchBitrate(settings.value(QStringLiteral("bitrate"), 320).toInt()));
    normalized.insert(QStringLiteral("sampleRate"),
                      normalizeBatchSampleRate(settings.value(QStringLiteral("sampleRate"), 44100).toInt()));
    normalized.insert(QStringLiteral("channelMode"),
                      normalizeBatchChannelMode(settings.value(QStringLiteral("channelMode")).toString()));
    normalized.insert(QStringLiteral("playbackRate"),
                      normalizeBatchPlaybackRate(settings.value(QStringLiteral("playbackRate"), 1.0).toDouble()));
    normalized.insert(QStringLiteral("pitchSemitones"),
                      normalizeBatchPitchSemitones(settings.value(QStringLiteral("pitchSemitones")).toInt()));
    normalized.insert(QStringLiteral("addResultsToPlaylist"),
                      normalized.value(QStringLiteral("playlistAddMode")).toString()
                          != QStringLiteral("disabled"));
    return normalized;
}

QVariantMap AppSettingsManager::normalizeBatchAudioConverterPresetSettings(const QVariantMap &settings)
{
    QVariantMap normalized;
    normalized.insert(QStringLiteral("outputDirectory"),
                      settings.value(QStringLiteral("outputDirectory")).toString().trimmed());
    normalized.insert(QStringLiteral("namingPolicy"),
                      normalizeBatchNamingPolicy(settings.value(QStringLiteral("namingPolicy")).toString()));
    normalized.insert(QStringLiteral("format"),
                      normalizeBatchFormat(settings.value(QStringLiteral("format")).toString()));
    normalized.insert(QStringLiteral("conflictPolicy"),
                      normalizeBatchConflictPolicy(settings.value(QStringLiteral("conflictPolicy")).toString()));
    normalized.insert(QStringLiteral("playlistAddMode"),
                      normalizeBatchPlaylistAddMode(
                          settings.value(QStringLiteral("playlistAddMode")).toString(),
                          settings.value(QStringLiteral("addResultsToPlaylist"), true).toBool()));
    normalized.insert(QStringLiteral("bitrate"),
                      normalizeBatchBitrate(settings.value(QStringLiteral("bitrate"), 320).toInt()));
    normalized.insert(QStringLiteral("sampleRate"),
                      normalizeBatchSampleRate(settings.value(QStringLiteral("sampleRate"), 44100).toInt()));
    normalized.insert(QStringLiteral("channelMode"),
                      normalizeBatchChannelMode(settings.value(QStringLiteral("channelMode")).toString()));
    normalized.insert(QStringLiteral("playbackRate"),
                      normalizeBatchPlaybackRate(settings.value(QStringLiteral("playbackRate"), 1.0).toDouble()));
    normalized.insert(QStringLiteral("pitchSemitones"),
                      normalizeBatchPitchSemitones(settings.value(QStringLiteral("pitchSemitones")).toInt()));
    normalized.insert(QStringLiteral("addResultsToPlaylist"),
                      normalized.value(QStringLiteral("playlistAddMode")).toString()
                          != QStringLiteral("disabled"));
    return normalized;
}

QVariantList AppSettingsManager::normalizeBatchAudioConverterUserPresets(const QVariantList &presets)
{
    QVariantList normalized;
    normalized.reserve(presets.size());
    for (const QVariant &value : presets) {
        const QVariantMap preset = value.toMap();
        const QString id = preset.value(QStringLiteral("id")).toString().trimmed();
        const QString name = preset.value(QStringLiteral("name")).toString().simplified().trimmed();
        if (id.isEmpty() || name.isEmpty()) {
            continue;
        }
        QVariantMap normalizedPreset;
        normalizedPreset.insert(QStringLiteral("id"), id);
        normalizedPreset.insert(QStringLiteral("name"), name);
        normalizedPreset.insert(QStringLiteral("settings"),
                                normalizeBatchAudioConverterPresetSettings(
                                    preset.value(QStringLiteral("settings")).toMap()));
        normalizedPreset.insert(QStringLiteral("updatedAtMs"),
                                preset.value(QStringLiteral("updatedAtMs")).toLongLong());
        normalized.push_back(normalizedPreset);
    }
    return normalized;
}

QVariantMap AppSettingsManager::normalizeBatchAudioConverterDraft(const QVariantMap &draft)
{
    if (draft.value(QStringLiteral("schema")).toString() != QString::fromLatin1(kBatchDraftSchema)) {
        return {};
    }

    const QVariantMap settings = draft.value(QStringLiteral("settings")).toMap();
    const QVariantList items = draft.value(QStringLiteral("items")).toList();
    if (settings.isEmpty() || items.isEmpty()) {
        return {};
    }

    QVariantMap normalized;
    normalized.insert(QStringLiteral("schema"), QString::fromLatin1(kBatchDraftSchema));
    normalized.insert(QStringLiteral("persistedAtMs"),
                      qMax<qint64>(0, draft.value(QStringLiteral("persistedAtMs")).toLongLong()));
    normalized.insert(QStringLiteral("settings"), normalizeBatchAudioConverterLastSettings(settings));
    normalized.insert(QStringLiteral("jobMetadata"), draft.value(QStringLiteral("jobMetadata")).toMap());
    normalized.insert(QStringLiteral("items"), items);
    return normalized;
}

QVariantList AppSettingsManager::normalizeBatchAudioConverterFinishedJobs(const QVariantList &jobs)
{
    QVariantList normalized;
    normalized.reserve(jobs.size());
    for (const QVariant &value : jobs) {
        const QVariantMap report = value.toMap();
        if (report.value(QStringLiteral("schema")).toString() != QString::fromLatin1(kBatchReportSchema)) {
            continue;
        }
        if (report.value(QStringLiteral("jobMetadata")).toMap()
                .value(QStringLiteral("jobId")).toString().trimmed().isEmpty()) {
            continue;
        }
        normalized.push_back(report);
    }
    while (normalized.size() > kBatchFinishedJobsRetention) {
        normalized.removeFirst();
    }
    return normalized;
}

QVariantMap AppSettingsManager::normalizeYtDlpImportLastSettings(const QVariantMap &settings)
{
    if (settings.isEmpty()) {
        return {};
    }

    QVariantMap normalized;
    normalized.insert(QStringLiteral("outputDirectory"),
                      normalizeLocalPathValue(
                          settings.value(QStringLiteral("outputDirectory")).toString()));
    normalized.insert(QStringLiteral("selectedFormat"),
                      normalizeYtDlpFormat(
                          settings.value(QStringLiteral("selectedFormat")).toString()));
    normalized.insert(QStringLiteral("namingPolicy"),
                      normalizeYtDlpNamingPolicy(
                          settings.value(QStringLiteral("namingPolicy")).toString()));
    normalized.insert(QStringLiteral("conflictPolicy"),
                      normalizeYtDlpConflictPolicy(
                          settings.value(QStringLiteral("conflictPolicy")).toString()));
    normalized.insert(QStringLiteral("parallelDownloads"),
                      normalizeYtDlpParallelDownloads(
                          settings.value(QStringLiteral("parallelDownloads"),
                                         kYtDlpDefaultParallelDownloads)));
    return normalized;
}

QVariantMap AppSettingsManager::normalizeYtDlpImportDraft(const QVariantMap &draft)
{
    if (draft.value(QStringLiteral("schema")).toString() != QString::fromLatin1(kYtDlpDraftSchema)) {
        return {};
    }

    const qint64 persistedAtMs =
        qMax<qint64>(0, draft.value(QStringLiteral("persistedAtMs")).toLongLong());
    if (persistedAtMs <= 0) {
        return {};
    }
    if (QDateTime::currentMSecsSinceEpoch() - persistedAtMs > kYtDlpDraftLifetimeMs) {
        return {};
    }

    const QVariantMap settings = normalizeYtDlpImportLastSettings(
        draft.value(QStringLiteral("settings")).toMap());
    const QVariantMap jobMetadata = draft.value(QStringLiteral("jobMetadata")).toMap();
    const QVariantList sources = draft.value(QStringLiteral("sources")).toList();
    const QVariantList items = draft.value(QStringLiteral("items")).toList();
    if (settings.isEmpty() || sources.isEmpty()) {
        return {};
    }
    if (jobMetadata.value(QStringLiteral("jobId")).toString().trimmed().isEmpty()) {
        return {};
    }

    QVariantMap normalized;
    normalized.insert(QStringLiteral("schema"), QString::fromLatin1(kYtDlpDraftSchema));
    normalized.insert(QStringLiteral("persistedAtMs"), persistedAtMs);
    normalized.insert(QStringLiteral("settings"), settings);
    normalized.insert(QStringLiteral("jobMetadata"), jobMetadata);
    normalized.insert(QStringLiteral("sources"), sources);
    if (!items.isEmpty()) {
        normalized.insert(QStringLiteral("items"), items);
    }
    return normalized;
}

QVariantList AppSettingsManager::normalizeYtDlpImportRecentSources(const QVariantList &sources)
{
    return normalizeUniqueStringHistory(sources,
                                        kYtDlpRecentSourcesRetention,
                                        normalizeHttpUrlForHistory);
}

QVariantList AppSettingsManager::normalizeYtDlpImportRecentOutputDirectories(
    const QVariantList &directories)
{
    return normalizeUniqueStringHistory(directories,
                                        kYtDlpRecentOutputDirectoriesRetention,
                                        normalizeLocalPathValue);
}
