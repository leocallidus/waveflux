#include "YtDlpImportService.h"

#include "AppSettingsManager.h"

#include <QDir>
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QHash>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QTimer>
#include <QUuid>
#include <QUrl>
#include <QUrlQuery>
#include <QtGlobal>
#include <algorithm>
#include <utility>

namespace {
constexpr int kProbeStartTimeoutMs = 3000;
constexpr int kImportStartTimeoutMs = 3000;
constexpr auto kImportDraftSchema = "waveflux.ytdlp-import.v2";
constexpr int kCompletedReportRetention = 12;
constexpr qint64 kProbeSnapshotFreshnessMs = 24LL * 60LL * 60LL * 1000LL;
constexpr int kDefaultParallelDownloads = 1;
constexpr int kMinParallelDownloads = 1;
constexpr int kMaxParallelDownloads = 4;
constexpr int kImportCancelTerminateTimeoutMs = 1500;

QString localizedText(const QString &key)
{
    return AppSettingsManager::translateForCurrentLanguage(key);
}

qint64 nowMs()
{
    return QDateTime::currentMSecsSinceEpoch();
}

QString newIdentity()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString firstNonEmptyString(const QJsonObject &object, std::initializer_list<const char *> keys)
{
    for (const char *key : keys) {
        const QString value = object.value(QLatin1String(key)).toString().trimmed();
        if (!value.isEmpty()) {
            return value;
        }
    }
    return QString();
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

QString bestYtDlpDiagnosticLine(const QString &text)
{
    const QStringList rawLines = text.split(QRegularExpression(QStringLiteral("[\r\n]+")),
                                            Qt::SkipEmptyParts);
    QString firstMeaningfulLine;
    for (const QString &rawLine : rawLines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty()) {
            continue;
        }
        if (line.startsWith(QStringLiteral("ERROR:"), Qt::CaseInsensitive)) {
            return line;
        }
        const bool isKnownNoise =
            line.startsWith(QStringLiteral("WARNING:"), Qt::CaseInsensitive)
            || line.startsWith(QStringLiteral("Deprecated Feature:"), Qt::CaseInsensitive)
            || line.startsWith(QStringLiteral("Please remove "), Qt::CaseInsensitive)
            || line.startsWith(QStringLiteral("See "), Qt::CaseInsensitive);
        if (!isKnownNoise && firstMeaningfulLine.isEmpty()) {
            firstMeaningfulLine = line;
        }
    }
    return !firstMeaningfulLine.isEmpty() ? firstMeaningfulLine : firstNonEmptyLine(text);
}

qint64 integerField(const QJsonObject &object, const char *key, qint64 fallback = 0)
{
    const QJsonValue value = object.value(QLatin1String(key));
    if (!value.isDouble()) {
        return fallback;
    }

    const double number = value.toDouble();
    if (!qIsFinite(number)) {
        return fallback;
    }

    return qMax<qint64>(0, qRound64(number));
}

QString thumbnailUrl(const QJsonObject &object)
{
    const QString direct = object.value(QStringLiteral("thumbnail")).toString().trimmed();
    if (!direct.isEmpty()) {
        return direct;
    }

    const QJsonArray thumbnails = object.value(QStringLiteral("thumbnails")).toArray();
    for (int i = thumbnails.size() - 1; i >= 0; --i) {
        const QJsonObject thumbnail = thumbnails.at(i).toObject();
        const QString url = thumbnail.value(QStringLiteral("url")).toString().trimmed();
        if (!url.isEmpty()) {
            return url;
        }
    }

    return QString();
}

bool isLikelyWebUrl(const QString &value)
{
    const QUrl url(value);
    if (!url.isValid()) {
        return false;
    }

    const QString scheme = url.scheme().trimmed().toLower();
    return scheme == QStringLiteral("http") || scheme == QStringLiteral("https");
}

QString normalizedWebpageUrl(const QJsonObject &object)
{
    const QString webpageUrl = firstNonEmptyString(
        object, {"webpage_url", "original_url", "webpage_url_basename"});
    if (isLikelyWebUrl(webpageUrl)) {
        return webpageUrl;
    }

    const QString rawUrl = object.value(QStringLiteral("url")).toString().trimmed();
    if (isLikelyWebUrl(rawUrl)) {
        return rawUrl;
    }

    return QString();
}

bool isPlaylistCandidateUrl(const QString &value)
{
    const QUrl url(value);
    if (!url.isValid()) {
        return false;
    }

    const QUrlQuery query(url);
    if (query.hasQueryItem(QStringLiteral("list"))
        && !query.queryItemValue(QStringLiteral("list")).trimmed().isEmpty()) {
        return true;
    }

    return url.path().contains(QStringLiteral("/playlist"));
}

QString synthesizedEntrySourceUrl(const QJsonObject &object,
                                  const QString &extractor,
                                  const QString &entryId)
{
    const QString rawUrl = object.value(QStringLiteral("url")).toString().trimmed();
    if (isLikelyWebUrl(rawUrl)) {
        return rawUrl;
    }

    if (entryId.isEmpty()) {
        return QString();
    }

    if (extractor.compare(QStringLiteral("Youtube"), Qt::CaseInsensitive) == 0
        || extractor.compare(QStringLiteral("YoutubeTab"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("https://www.youtube.com/watch?v=%1").arg(entryId);
    }

    return QString();
}

QString normalizedAvailability(const QJsonObject &object)
{
    const QString availability =
        object.value(QStringLiteral("availability")).toString().trimmed().toLower();
    if (!availability.isEmpty()) {
        return availability;
    }

    const QString title = object.value(QStringLiteral("title")).toString().trimmed().toLower();
    if (title.contains(QStringLiteral("private video"))) {
        return QStringLiteral("private");
    }
    if (title.contains(QStringLiteral("deleted video"))) {
        return QStringLiteral("deleted");
    }

    return QStringLiteral("public");
}

bool isAvailabilityPlayable(const QString &availability)
{
    return availability != QStringLiteral("private")
        && availability != QStringLiteral("deleted")
        && availability != QStringLiteral("unavailable")
        && availability != QStringLiteral("subscriber_only")
        && availability != QStringLiteral("needs_auth")
        && availability != QStringLiteral("premium_only")
        && availability != QStringLiteral("nested_playlist");
}

QString normalizedExtractor(const QJsonObject &object)
{
    return firstNonEmptyString(object, {"extractor_key", "extractor", "ie_key"});
}

YtDlpImportService::ProbeEntry normalizeEntry(const QJsonObject &object,
                                              int metadataOrder)
{
    YtDlpImportService::ProbeEntry entry;
    entry.metadataOrder = metadataOrder;
    entry.extractor = normalizedExtractor(object);
    entry.title = firstNonEmptyString(object, {"title", "alt_title"});
    entry.entryId = firstNonEmptyString(object, {"id", "url"});
    entry.playlistIndex = object.contains(QStringLiteral("playlist_index"))
        ? qMax(-1, object.value(QStringLiteral("playlist_index")).toInt(-1))
        : -1;
    entry.duration = integerField(object, "duration", 0);
    entry.thumbnail = thumbnailUrl(object);
    entry.webpageUrl = normalizedWebpageUrl(object);
    entry.sourceUrl = !entry.webpageUrl.isEmpty()
        ? entry.webpageUrl
        : object.value(QStringLiteral("original_url")).toString().trimmed();
    if (entry.sourceUrl.isEmpty()) {
        entry.sourceUrl = synthesizedEntrySourceUrl(object, entry.extractor, entry.entryId);
    }

    const bool nestedEntry = object.value(QStringLiteral("entries")).isArray();
    entry.availability = nestedEntry
        ? QStringLiteral("nested_playlist")
        : normalizedAvailability(object);
    entry.isPlayable = !object.isEmpty() && isAvailabilityPlayable(entry.availability);
    return entry;
}

YtDlpImportService::ProbeEntry unavailablePlaceholderEntry(int metadataOrder)
{
    YtDlpImportService::ProbeEntry entry;
    entry.metadataOrder = metadataOrder;
    entry.availability = QStringLiteral("unavailable");
    entry.isPlayable = false;
    return entry;
}

QString defaultOutputDirectory()
{
    QString directory = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation).trimmed();
    if (directory.isEmpty()) {
        directory = QDir::homePath();
    }
    return QDir::cleanPath(directory);
}

QString normalizeLocalPath(const QString &path)
{
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }

    if (trimmed.size() >= 4
        && trimmed.at(0) == QLatin1Char('/')
        && trimmed.at(1).isLetter()
        && trimmed.at(2) == QLatin1Char(':')
        && (trimmed.at(3) == QLatin1Char('/') || trimmed.at(3) == QLatin1Char('\\'))) {
        return QDir::cleanPath(QDir::fromNativeSeparators(trimmed.mid(1)));
    }

    return QDir::cleanPath(QDir::fromNativeSeparators(trimmed));
}

QString normalizeConflictPolicy(const QString &value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("overwrite-if-allowed")
        || normalized == QStringLiteral("skip-on-conflict")
        || normalized == QStringLiteral("fail-on-conflict")) {
        return normalized;
    }
    return QStringLiteral("auto-rename");
}

int normalizeParallelDownloads(int value)
{
    return qBound(kMinParallelDownloads, value, kMaxParallelDownloads);
}

QString normalizedSourceUrl(const QString &rawInput)
{
    const QString trimmed = rawInput.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }

    QUrl url(trimmed);
    if (!url.isValid()) {
        return QString();
    }

    const QString scheme = url.scheme().trimmed().toLower();
    if (scheme != QStringLiteral("http") && scheme != QStringLiteral("https")) {
        return QString();
    }

    url.setScheme(scheme);
    url.setHost(url.host().trimmed().toLower());
    url.setFragment(QString());
    return url.toString(QUrl::FullyEncoded).trimmed();
}

QString unsupportedSchemeForInput(const QString &rawInput)
{
    const QString trimmed = rawInput.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }

    static const QRegularExpression explicitSchemePattern(
        QStringLiteral(R"(^([A-Za-z][A-Za-z0-9+\.\-]*):)"));
    const QRegularExpressionMatch match = explicitSchemePattern.match(trimmed);
    if (!match.hasMatch()) {
        return QString();
    }

    const QString scheme = match.captured(1).trimmed().toLower();
    if (scheme == QStringLiteral("http") || scheme == QStringLiteral("https")) {
        return QString();
    }
    return scheme;
}

QStringList splitSourceLines(const QString &sourceText)
{
    return sourceText.split(QRegularExpression(QStringLiteral("[\r\n]+")));
}

QString sanitizeFileStem(QString value)
{
    value = value.trimmed();
    if (value.isEmpty()) {
        value = QStringLiteral("Imported Track");
    }

    static const QRegularExpression invalidCharacters(QStringLiteral(R"([\\/:*?"<>|\x00-\x1F])"));
    value.replace(invalidCharacters, QStringLiteral("_"));
    value.replace(QRegularExpression(QStringLiteral(R"(\s+)")), QStringLiteral(" "));
    value = value.trimmed();
    while (value.endsWith(QLatin1Char('.')) || value.endsWith(QLatin1Char(' '))) {
        value.chop(1);
    }

    if (value.isEmpty()) {
        value = QStringLiteral("Imported Track");
    }
    return value;
}

struct FormatArgumentPlan {
    QString selectedFormat;
    QString outputExtension;
    QString audioFormatArgument;
    QStringList extraArguments;
    QVariantMap toVariantMap() const
    {
        QVariantMap map;
        map.insert(QStringLiteral("selectedFormat"), selectedFormat);
        map.insert(QStringLiteral("outputExtension"), outputExtension);
        map.insert(QStringLiteral("audioFormatArgument"), audioFormatArgument);
        QVariantList arguments;
        arguments.reserve(extraArguments.size());
        for (const QString &arg : extraArguments) {
            arguments.push_back(arg);
        }
        map.insert(QStringLiteral("extraArguments"), arguments);
        return map;
    }
};

FormatArgumentPlan formatArgumentPlanForSelection(const QString &selectedFormat)
{
    const QString normalized = selectedFormat.trimmed().toLower();
    if (normalized == QStringLiteral("m4a")) {
        return {QStringLiteral("m4a"),
                QStringLiteral("m4a"),
                QStringLiteral("m4a"),
                {}};
    }
    if (normalized == QStringLiteral("opus")) {
        return {QStringLiteral("opus"),
                QStringLiteral("opus"),
                QStringLiteral("opus"),
                {QStringLiteral("--audio-quality"), QStringLiteral("0")}};
    }
    return {QStringLiteral("mp3"),
            QStringLiteral("mp3"),
            QStringLiteral("mp3"),
            {QStringLiteral("--audio-quality"), QStringLiteral("0")}};
}

FormatArgumentPlan formatArgumentPlanFromVariantMap(const QVariantMap &map,
                                                    const QString &fallbackSelection)
{
    if (map.isEmpty()) {
        return formatArgumentPlanForSelection(fallbackSelection);
    }

    FormatArgumentPlan plan;
    plan.selectedFormat = map.value(QStringLiteral("selectedFormat")).toString().trimmed().toLower();
    plan.audioFormatArgument =
        map.value(QStringLiteral("audioFormatArgument")).toString().trimmed().toLower();
    plan.outputExtension =
        map.value(QStringLiteral("outputExtension")).toString().trimmed().toLower();

    const QVariantList extraArguments = map.value(QStringLiteral("extraArguments")).toList();
    plan.extraArguments.reserve(extraArguments.size());
    for (const QVariant &value : extraArguments) {
        const QString argument = value.toString();
        if (!argument.isEmpty()) {
            plan.extraArguments.push_back(argument);
        }
    }

    if (plan.selectedFormat.isEmpty() || plan.audioFormatArgument.isEmpty()
        || plan.outputExtension.isEmpty()) {
        return formatArgumentPlanForSelection(fallbackSelection);
    }
    return plan;
}

QString outputTemplateForFile(const QString &plannedOutputFile)
{
    const QFileInfo info(plannedOutputFile);
    return info.path() + QDir::separator() + info.completeBaseName() + QStringLiteral(".%(ext)s");
}

QStringList probeArgumentsForSourceUrl(const QString &sourceUrl)
{
    QStringList arguments{QStringLiteral("--dump-single-json"),
                          QStringLiteral("--skip-download"),
                          QStringLiteral("--no-warnings")};
    if (isPlaylistCandidateUrl(sourceUrl)) {
        arguments.push_back(QStringLiteral("--flat-playlist"));
        arguments.push_back(QStringLiteral("--yes-playlist"));
    }
    arguments.append({QStringLiteral("--"),
                      sourceUrl});
    return arguments;
}

QString positionalPrefix(int index)
{
    const int normalized = qMax(0, index);
    return QStringLiteral("%1").arg(normalized, 4, 10, QLatin1Char('0'));
}

struct NamingPlan {
    QString requestedPolicy;
    QString appliedPolicy;
    QString fileName;
    QString sourceTitle;
    QString entryTitle;
    QString fallbackReasonKey;
    QStringList missingMetadataFields;
};

NamingPlan buildNamingPlanForEntry(const YtDlpImportService::ProbeEntry &entry,
                                   const QVariantMap &sourceMetadata,
                                   const QString &namingPolicy,
                                   const QString &outputExtension,
                                   bool isPlaylist)
{
    NamingPlan plan;
    plan.requestedPolicy = namingPolicy.trimmed().toLower();
    if (plan.requestedPolicy != QStringLiteral("title-only")
        && plan.requestedPolicy != QStringLiteral("source-title-entry-title")) {
        plan.requestedPolicy = QStringLiteral("auto");
    }

    plan.entryTitle = entry.title.trimmed();
    const QString baseTitle = sanitizeFileStem(plan.entryTitle);
    const QString sourceTitle = sourceMetadata.value(QStringLiteral("playlistTitle")).toString().trimmed().isEmpty()
        ? sourceMetadata.value(QStringLiteral("title")).toString().trimmed()
        : sourceMetadata.value(QStringLiteral("playlistTitle")).toString().trimmed();
    plan.sourceTitle = sourceTitle;

    if (plan.requestedPolicy == QStringLiteral("source-title-entry-title")) {
        const QString sourceStem = sanitizeFileStem(sourceTitle);
        if (sourceTitle.isEmpty()) {
            plan.appliedPolicy = QStringLiteral("title-only");
            plan.fallbackReasonKey = QStringLiteral("missing-source-title");
            plan.missingMetadataFields.push_back(QStringLiteral("sourceTitle"));
        } else if (sourceStem.compare(baseTitle, Qt::CaseInsensitive) == 0) {
            plan.appliedPolicy = QStringLiteral("title-only");
            plan.fallbackReasonKey = QStringLiteral("redundant-source-title");
            plan.missingMetadataFields.push_back(QStringLiteral("distinctSourceTitle"));
        } else {
            plan.appliedPolicy = QStringLiteral("source-title-entry-title");
            plan.fileName = sourceStem + QStringLiteral(" - ") + baseTitle
                + QLatin1Char('.') + outputExtension;
            return plan;
        }
    }

    if (plan.requestedPolicy == QStringLiteral("auto")
        && isPlaylist) {
        const int index = entry.playlistIndex > 0 ? entry.playlistIndex : (entry.metadataOrder + 1);
        plan.appliedPolicy = QStringLiteral("auto");
        plan.fileName = positionalPrefix(index) + QStringLiteral(" - ") + baseTitle
            + QLatin1Char('.') + outputExtension;
        return plan;
    }

    if (plan.requestedPolicy == QStringLiteral("auto")) {
        plan.appliedPolicy = QStringLiteral("title-only");
        plan.fallbackReasonKey = QStringLiteral("non-playlist-source");
        plan.missingMetadataFields.push_back(QStringLiteral("playlistIndex"));
    } else if (plan.appliedPolicy.isEmpty()) {
        plan.appliedPolicy = QStringLiteral("title-only");
    }
    plan.fileName = baseTitle + QLatin1Char('.') + outputExtension;
    return plan;
}

QString uniqueOutputPath(const QString &requestedOutputPath,
                         const QSet<QString> &reservedPaths,
                         const QSet<QString> &existingPaths)
{
    const QFileInfo info(requestedOutputPath);
    const QString directory = info.absolutePath();
    const QString baseName = info.completeBaseName();
    const QString suffix = info.suffix();

    auto hasConflict = [&reservedPaths, &existingPaths](const QString &candidate) {
        return reservedPaths.contains(candidate)
            || existingPaths.contains(candidate)
            || QFileInfo::exists(candidate);
    };

    if (!hasConflict(requestedOutputPath)) {
        return requestedOutputPath;
    }

    for (int attempt = 2; attempt < 10000; ++attempt) {
        const QString fileName = suffix.isEmpty()
            ? QStringLiteral("%1 (%2)").arg(baseName).arg(attempt)
            : QStringLiteral("%1 (%2).%3").arg(baseName).arg(attempt).arg(suffix);
        const QString candidate = QDir(directory).filePath(fileName);
        if (!hasConflict(candidate)) {
            return candidate;
        }
    }

    return requestedOutputPath;
}

QString stagingRootDirectory(const QString &outputDirectory)
{
    return QDir(outputDirectory).filePath(QStringLiteral(".waveflux-yt-dlp-staging"));
}

QString stagingOutputTemplateForItem(const QString &outputDirectory, const QString &itemId)
{
    return QDir(stagingRootDirectory(outputDirectory))
               .filePath(itemId + QStringLiteral("/payload.%(ext)s"));
}

QStringList readLinesFromBuffer(QByteArray *buffer, const QByteArray &chunk)
{
    QStringList lines;
    if (!buffer) {
        return lines;
    }

    buffer->append(chunk);
    while (true) {
        const int lineBreakIndex = buffer->indexOf('\n');
        if (lineBreakIndex < 0) {
            break;
        }

        QByteArray line = buffer->left(lineBreakIndex);
        buffer->remove(0, lineBreakIndex + 1);
        if (line.endsWith('\r')) {
            line.chop(1);
        }
        lines.push_back(QString::fromLocal8Bit(line).trimmed());
    }

    return lines;
}

QVariantList entriesToVariantList(const QList<YtDlpImportService::ProbeEntry> &entries)
{
    QVariantList items;
    items.reserve(entries.size());
    for (const YtDlpImportService::ProbeEntry &entry : entries) {
        items.push_back(YtDlpImportService::probeEntryToVariantMap(entry));
    }
    return items;
}

QVariantList importItemsToVariantList(const QList<YtDlpImportService::ImportItem> &items)
{
    QVariantList result;
    result.reserve(items.size());
    for (const YtDlpImportService::ImportItem &item : items) {
        result.push_back(YtDlpImportService::importItemToVariantMap(item));
    }
    return result;
}

QString itemStateKey(YtDlpImportService::ItemState state)
{
    switch (state) {
    case YtDlpImportService::Running:
        return QStringLiteral("running");
    case YtDlpImportService::Succeeded:
        return QStringLiteral("succeeded");
    case YtDlpImportService::Failed:
        return QStringLiteral("failed");
    case YtDlpImportService::Canceled:
        return QStringLiteral("canceled");
    case YtDlpImportService::Skipped:
        return QStringLiteral("skipped");
    case YtDlpImportService::Pending:
    default:
        return QStringLiteral("pending");
    }
}

QString sourceStatusKey(YtDlpImportService::SourceStatus status)
{
    switch (status) {
    case YtDlpImportService::SourceProbing:
        return QStringLiteral("probing");
    case YtDlpImportService::SourceReady:
        return QStringLiteral("ready");
    case YtDlpImportService::SourceReadyWithIssues:
        return QStringLiteral("ready-with-issues");
    case YtDlpImportService::SourceProbeFailed:
        return QStringLiteral("probe-failed");
    case YtDlpImportService::SourceImporting:
        return QStringLiteral("importing");
    case YtDlpImportService::SourceCompleted:
        return QStringLiteral("completed");
    case YtDlpImportService::SourceCompletedWithFailures:
        return QStringLiteral("completed-with-failures");
    case YtDlpImportService::SourceCanceled:
        return QStringLiteral("canceled");
    case YtDlpImportService::SourcePendingProbe:
    default:
        return QStringLiteral("pending-probe");
    }
}

QString failureTypeKey(YtDlpImportService::ItemFailureType failureType)
{
    switch (failureType) {
    case YtDlpImportService::ContentFailure:
        return QStringLiteral("content");
    case YtDlpImportService::NetworkFailure:
        return QStringLiteral("network");
    case YtDlpImportService::DependencyFailure:
        return QStringLiteral("dependency");
    case YtDlpImportService::PermissionFailure:
        return QStringLiteral("permission");
    case YtDlpImportService::DiskFailure:
        return QStringLiteral("disk");
    case YtDlpImportService::OutputFailure:
        return QStringLiteral("output");
    case YtDlpImportService::PostprocessFailure:
        return QStringLiteral("postprocess");
    case YtDlpImportService::CanceledFailure:
        return QStringLiteral("canceled");
    case YtDlpImportService::GenericFailure:
        return QStringLiteral("generic");
    case YtDlpImportService::NoFailure:
    default:
        return QStringLiteral("none");
    }
}

YtDlpImportService::ItemFailureType failureTypeFromCategoryKey(const QString &categoryKey)
{
    const QString normalized = categoryKey.trimmed().toLower();
    if (normalized == QStringLiteral("content")) {
        return YtDlpImportService::ContentFailure;
    }
    if (normalized == QStringLiteral("network")) {
        return YtDlpImportService::NetworkFailure;
    }
    if (normalized == QStringLiteral("dependency")) {
        return YtDlpImportService::DependencyFailure;
    }
    if (normalized == QStringLiteral("permission")) {
        return YtDlpImportService::PermissionFailure;
    }
    if (normalized == QStringLiteral("disk")) {
        return YtDlpImportService::DiskFailure;
    }
    if (normalized == QStringLiteral("output")) {
        return YtDlpImportService::OutputFailure;
    }
    if (normalized == QStringLiteral("postprocess")) {
        return YtDlpImportService::PostprocessFailure;
    }
    if (normalized == QStringLiteral("canceled")) {
        return YtDlpImportService::CanceledFailure;
    }
    if (normalized == QStringLiteral("none")) {
        return YtDlpImportService::NoFailure;
    }
    return YtDlpImportService::GenericFailure;
}

QString retryEligibilityKey(YtDlpImportService::RetryEligibility eligibility)
{
    switch (eligibility) {
    case YtDlpImportService::RetryAllowed:
        return QStringLiteral("allowed");
    case YtDlpImportService::RetryBlocked:
        return QStringLiteral("blocked");
    case YtDlpImportService::RetryNotApplicable:
    default:
        return QStringLiteral("not-applicable");
    }
}

QString persistenceEligibilityKey(YtDlpImportService::PersistenceEligibility eligibility)
{
    switch (eligibility) {
    case YtDlpImportService::DraftPersistable:
        return QStringLiteral("draft");
    case YtDlpImportService::ReportPersistable:
        return QStringLiteral("report");
    case YtDlpImportService::NotPersistable:
    default:
        return QStringLiteral("none");
    }
}

YtDlpImportService::ItemState itemStateFromKey(const QString &key)
{
    const QString normalized = key.trimmed().toLower();
    if (normalized == QStringLiteral("running")) {
        return YtDlpImportService::Running;
    }
    if (normalized == QStringLiteral("succeeded")) {
        return YtDlpImportService::Succeeded;
    }
    if (normalized == QStringLiteral("failed")) {
        return YtDlpImportService::Failed;
    }
    if (normalized == QStringLiteral("canceled")) {
        return YtDlpImportService::Canceled;
    }
    if (normalized == QStringLiteral("skipped")) {
        return YtDlpImportService::Skipped;
    }
    return YtDlpImportService::Pending;
}

YtDlpImportService::SourceStatus sourceStatusFromKey(const QString &key)
{
    const QString normalized = key.trimmed().toLower();
    if (normalized == QStringLiteral("probing")) {
        return YtDlpImportService::SourceProbing;
    }
    if (normalized == QStringLiteral("ready")) {
        return YtDlpImportService::SourceReady;
    }
    if (normalized == QStringLiteral("ready-with-issues")) {
        return YtDlpImportService::SourceReadyWithIssues;
    }
    if (normalized == QStringLiteral("probe-failed")) {
        return YtDlpImportService::SourceProbeFailed;
    }
    if (normalized == QStringLiteral("importing")) {
        return YtDlpImportService::SourceImporting;
    }
    if (normalized == QStringLiteral("completed")) {
        return YtDlpImportService::SourceCompleted;
    }
    if (normalized == QStringLiteral("completed-with-failures")) {
        return YtDlpImportService::SourceCompletedWithFailures;
    }
    if (normalized == QStringLiteral("canceled")) {
        return YtDlpImportService::SourceCanceled;
    }
    return YtDlpImportService::SourcePendingProbe;
}

YtDlpImportService::RetryEligibility retryEligibilityFromKey(const QString &key)
{
    const QString normalized = key.trimmed().toLower();
    if (normalized == QStringLiteral("allowed")) {
        return YtDlpImportService::RetryAllowed;
    }
    if (normalized == QStringLiteral("blocked")) {
        return YtDlpImportService::RetryBlocked;
    }
    return YtDlpImportService::RetryNotApplicable;
}

YtDlpImportService::PersistenceEligibility persistenceEligibilityFromKey(const QString &key)
{
    const QString normalized = key.trimmed().toLower();
    if (normalized == QStringLiteral("draft")) {
        return YtDlpImportService::DraftPersistable;
    }
    if (normalized == QStringLiteral("report")) {
        return YtDlpImportService::ReportPersistable;
    }
    return YtDlpImportService::NotPersistable;
}

YtDlpImportService::SourceStatus restoredSourceStatus(const QVariantMap &sourceMap)
{
    const QVariantMap metadataSnapshot = sourceMap.value(QStringLiteral("metadataSnapshot")).toMap();
    const QVariantMap finalResultState = sourceMap.value(QStringLiteral("finalResultState")).toMap();
    const bool hasMetadata = !metadataSnapshot.isEmpty();
    const bool hasIssues = metadataSnapshot.value(QStringLiteral("hasUnavailableEntries")).toBool();
    const YtDlpImportService::SourceStatus stored =
        sourceStatusFromKey(sourceMap.value(QStringLiteral("sourceStatus")).toString());

    switch (stored) {
    case YtDlpImportService::SourceProbeFailed:
        return YtDlpImportService::SourceProbeFailed;
    case YtDlpImportService::SourceReady:
    case YtDlpImportService::SourceReadyWithIssues:
        return hasIssues ? YtDlpImportService::SourceReadyWithIssues
                         : YtDlpImportService::SourceReady;
    case YtDlpImportService::SourceProbing:
    case YtDlpImportService::SourceImporting:
    case YtDlpImportService::SourceCompleted:
    case YtDlpImportService::SourceCompletedWithFailures:
    case YtDlpImportService::SourceCanceled:
        if (hasMetadata) {
            return hasIssues ? YtDlpImportService::SourceReadyWithIssues
                             : YtDlpImportService::SourceReady;
        }
        if (!finalResultState.value(QStringLiteral("message")).toString().trimmed().isEmpty()) {
            return YtDlpImportService::SourceProbeFailed;
        }
        return YtDlpImportService::SourcePendingProbe;
    case YtDlpImportService::SourcePendingProbe:
    default:
        return YtDlpImportService::SourcePendingProbe;
    }
}

YtDlpImportService::RetryEligibility retryEligibilityForFailureType(
    YtDlpImportService::ItemFailureType failureType,
    bool isPlayable = true)
{
    if (!isPlayable) {
        return YtDlpImportService::RetryBlocked;
    }

    switch (failureType) {
    case YtDlpImportService::NetworkFailure:
    case YtDlpImportService::OutputFailure:
    case YtDlpImportService::DiskFailure:
    case YtDlpImportService::PostprocessFailure:
    case YtDlpImportService::CanceledFailure:
    case YtDlpImportService::GenericFailure:
        return YtDlpImportService::RetryAllowed;
    case YtDlpImportService::DependencyFailure:
    case YtDlpImportService::PermissionFailure:
    case YtDlpImportService::ContentFailure:
        return YtDlpImportService::RetryBlocked;
    case YtDlpImportService::NoFailure:
    default:
        return YtDlpImportService::RetryNotApplicable;
    }
}

bool isTerminalState(YtDlpImportService::ItemState state)
{
    return state == YtDlpImportService::Succeeded
        || state == YtDlpImportService::Failed
        || state == YtDlpImportService::Canceled
        || state == YtDlpImportService::Skipped;
}

QString normalizedErrorCategoryKey(const QString &rawMessage)
{
    const QString message = rawMessage.trimmed().toLower();
    if (message.isEmpty()) {
        return QStringLiteral("none");
    }
    if (message.contains(QStringLiteral("canceled"))
        || message.contains(QStringLiteral("cancelled"))
        || message.contains(QStringLiteral("отмен"))) {
        return QStringLiteral("canceled");
    }
    if (message.contains(QStringLiteral("permission denied"))
        || message.contains(QStringLiteral("access is denied"))
        || message.contains(QStringLiteral("operation not permitted"))
        || message.contains(QStringLiteral("read-only file system"))
        || message.contains(QStringLiteral("permission"))
        || message.contains(QStringLiteral("доступ"))
        || message.contains(QStringLiteral("разреш"))) {
        return QStringLiteral("permission");
    }
    if (message.contains(QStringLiteral("no space left"))
        || message.contains(QStringLiteral("disk full"))
        || message.contains(QStringLiteral("not enough space"))
        || message.contains(QStringLiteral("quota exceeded"))
        || message.contains(QStringLiteral("места"))
        || message.contains(QStringLiteral("диск заполнен"))) {
        return QStringLiteral("disk");
    }
    if (message.contains(QStringLiteral("ffmpeg"))
        || message.contains(QStringLiteral("postprocess"))
        || message.contains(QStringLiteral("post-processing"))) {
        return QStringLiteral("postprocess");
    }
    if (message.contains(QStringLiteral("output directory"))
        || message.contains(QStringLiteral("expected file"))
        || message.contains(QStringLiteral("cannot write"))
        || message.contains(QStringLiteral("output file"))
        || message.contains(QStringLiteral("выходн"))
        || message.contains(QStringLiteral("не удалось запис")))
    {
        return QStringLiteral("output");
    }
    if (message.contains(QStringLiteral("not found in path"))
        || message.contains(QStringLiteral("configured path"))
        || message.contains(QStringLiteral("runtime settings"))
        || message.contains(QStringLiteral("executable"))
        || message.contains(QStringLiteral("dependency"))
        || message.contains(QStringLiteral("зависим"))
        || message.contains(QStringLiteral("исполняем"))) {
        return QStringLiteral("dependency");
    }
    if (message.contains(QStringLiteral("timed out"))
        || message.contains(QStringLiteral("network"))
        || message.contains(QStringLiteral("connection"))
        || message.contains(QStringLiteral("http error"))
        || message.contains(QStringLiteral("unable to download"))
        || message.contains(QStringLiteral("temporarily unavailable"))
        || message.contains(QStringLiteral("name resolution"))
        || message.contains(QStringLiteral("сеть"))
        || message.contains(QStringLiteral("соединен"))
        || message.contains(QStringLiteral("timeout"))) {
        return QStringLiteral("network");
    }
    if (message.contains(QStringLiteral("private"))
        || message.contains(QStringLiteral("deleted"))
        || message.contains(QStringLiteral("unavailable"))
        || message.contains(QStringLiteral("unsupported"))
        || message.contains(QStringLiteral("not playable"))
        || message.contains(QStringLiteral("недоступ"))
        || message.contains(QStringLiteral("не поддерж"))) {
        return QStringLiteral("content");
    }
    return QStringLiteral("generic");
}

QString localizedErrorCategoryLabel(const QString &categoryKey)
{
    const QString normalized = categoryKey.trimmed().toLower();
    if (normalized.isEmpty() || normalized == QStringLiteral("none")) {
        return QString();
    }
    if (normalized == QStringLiteral("dependency")) {
        return localizedText(QStringLiteral("ytDlpImport.errorCategoryDependency"));
    }
    if (normalized == QStringLiteral("content")) {
        return localizedText(QStringLiteral("ytDlpImport.errorCategoryContent"));
    }
    if (normalized == QStringLiteral("network")) {
        return localizedText(QStringLiteral("ytDlpImport.errorCategoryNetwork"));
    }
    if (normalized == QStringLiteral("permission")) {
        return localizedText(QStringLiteral("ytDlpImport.errorCategoryPermission"));
    }
    if (normalized == QStringLiteral("disk")) {
        return localizedText(QStringLiteral("ytDlpImport.errorCategoryDisk"));
    }
    if (normalized == QStringLiteral("output")) {
        return localizedText(QStringLiteral("ytDlpImport.errorCategoryOutput"));
    }
    if (normalized == QStringLiteral("postprocess")) {
        return localizedText(QStringLiteral("ytDlpImport.errorCategoryPostprocess"));
    }
    if (normalized == QStringLiteral("canceled")) {
        return localizedText(QStringLiteral("ytDlpImport.errorCategoryCanceled"));
    }
    if (normalized == QStringLiteral("mixed")) {
        return localizedText(QStringLiteral("ytDlpImport.errorCategoryMixed"));
    }
    return localizedText(QStringLiteral("ytDlpImport.errorCategoryGeneric"));
}

QVariantMap buildProblemItemSummary(const YtDlpImportService::ImportItem &item)
{
    QVariantMap map;
    const QString message = !item.errorText.trimmed().isEmpty()
        ? item.errorText.trimmed()
        : item.statusText.trimmed();
    const QString categoryKey = item.state == YtDlpImportService::Canceled
        ? QStringLiteral("canceled")
        : item.state == YtDlpImportService::Skipped
            ? failureTypeKey(item.failureType)
            : normalizedErrorCategoryKey(message);
    map.insert(QStringLiteral("itemId"), item.entryId);
    map.insert(QStringLiteral("title"), item.title);
    map.insert(QStringLiteral("state"), itemStateKey(item.state));
    map.insert(QStringLiteral("message"), message);
    map.insert(QStringLiteral("errorCategory"), categoryKey);
    map.insert(QStringLiteral("errorCategoryLabel"), localizedErrorCategoryLabel(categoryKey));
    map.insert(QStringLiteral("sourceUrl"), item.sourceUrl);
    map.insert(QStringLiteral("plannedOutputFile"), item.plannedOutputFile);
    map.insert(QStringLiteral("sourceId"), item.sourceId);
    map.insert(QStringLiteral("retryAllowed"), item.retryEligibility == YtDlpImportService::RetryAllowed);
    map.insert(QStringLiteral("conflictBlocked"),
               item.conflictResolution.resolutionKey == QStringLiteral("skip-on-conflict")
                   || item.conflictResolution.resolutionKey == QStringLiteral("fail-on-conflict"));
    map.insert(QStringLiteral("reportKind"), QStringLiteral("item"));
    return map;
}

QVariantMap buildProblemSourceSummary(const YtDlpImportService::ImportSource &source)
{
    QVariantMap map;
    const QVariantMap metadata = source.metadataSnapshot;
    const QVariantMap immutable = source.immutableSourceInput;
    const QVariantMap finalState = source.finalResultState;
    const QString message = finalState.value(QStringLiteral("message")).toString().trimmed();
    const QString title = metadata.value(QStringLiteral("playlistTitle")).toString().trimmed().isEmpty()
        ? metadata.value(QStringLiteral("title")).toString().trimmed()
        : metadata.value(QStringLiteral("playlistTitle")).toString().trimmed();
    const QString sourceUrl = immutable.value(QStringLiteral("normalizedUrl")).toString().trimmed();
    const QString sourceCategory = source.failureType == YtDlpImportService::NoFailure
        ? QStringLiteral("generic")
        : failureTypeKey(source.failureType);
    QString stateKey = QStringLiteral("not-probed");
    if (source.status == YtDlpImportService::SourceProbeFailed) {
        stateKey = QStringLiteral("probe-failed");
    }
    map.insert(QStringLiteral("sourceId"), source.sourceId);
    map.insert(QStringLiteral("title"), !title.isEmpty() ? title : sourceUrl);
    map.insert(QStringLiteral("state"), stateKey);
    map.insert(QStringLiteral("message"),
               !message.isEmpty() ? message : localizedText(QStringLiteral("ytDlpImport.importRequiresProbe")));
    map.insert(QStringLiteral("errorCategory"), sourceCategory);
    map.insert(QStringLiteral("errorCategoryLabel"),
               localizedErrorCategoryLabel(sourceCategory));
    map.insert(QStringLiteral("sourceUrl"), sourceUrl);
    map.insert(QStringLiteral("retryAllowed"), source.retryEligibility == YtDlpImportService::RetryAllowed);
    map.insert(QStringLiteral("conflictBlocked"), false);
    map.insert(QStringLiteral("reportKind"), QStringLiteral("source"));
    return map;
}

QString summaryCategoryKey(const QVariantList &problemItems)
{
    QSet<QString> categories;
    for (const QVariant &value : problemItems) {
        const QString category = value.toMap().value(QStringLiteral("errorCategory")).toString().trimmed();
        if (!category.isEmpty() && category != QStringLiteral("none")) {
            categories.insert(category);
        }
    }
    if (categories.isEmpty()) {
        return QStringLiteral("none");
    }
    if (categories.size() == 1) {
        return *categories.cbegin();
    }
    return QStringLiteral("mixed");
}

QString summaryOutcomeKey(int succeeded, int failed, int canceled, bool wasCanceled)
{
    if (wasCanceled) {
        return succeeded > 0 ? QStringLiteral("partial-canceled") : QStringLiteral("canceled");
    }
    if (failed > 0) {
        return succeeded > 0 ? QStringLiteral("partial-failed") : QStringLiteral("failed");
    }
    return QStringLiteral("succeeded");
}

QVariantMap buildTerminalSummary(const QString &outcomeKey,
                                 const QString &categoryKey,
                                 const QString &detailText,
                                 const QString &lastError,
                                 int totalCount,
                                 int succeeded,
                                 int failed,
                                 int canceled,
                                 int skipped,
                                 int notProbed,
                                 int conflictBlocked,
                                 bool wasCanceled,
                                 const QString &outputDirectory,
                                 const QString &selectedFormat,
                                 const QString &namingPolicy,
                                 const QString &conflictPolicy,
                                 const QVariantList &resultFiles,
                                 const QStringList &orderedResultFiles,
                                 const QVariantList &problemItems)
{
    QVariantMap summary;
    QString headlineText;
    if (outcomeKey == QStringLiteral("partial-canceled")) {
        headlineText = localizedText(QStringLiteral("ytDlpImport.summaryHeadlinePartialCanceled"));
    } else if (outcomeKey == QStringLiteral("canceled")) {
        headlineText = localizedText(QStringLiteral("ytDlpImport.summaryHeadlineCanceled"));
    } else if (outcomeKey == QStringLiteral("partial-failed")) {
        headlineText = localizedText(QStringLiteral("ytDlpImport.summaryHeadlinePartialFailed"));
    } else if (outcomeKey == QStringLiteral("failed")) {
        headlineText = localizedText(QStringLiteral("ytDlpImport.summaryHeadlineFailed"));
    } else {
        headlineText = skipped > 0
            ? localizedText(QStringLiteral("ytDlpImport.summaryHeadlineSucceededWithSkips"))
            : localizedText(QStringLiteral("ytDlpImport.summaryHeadlineSucceeded"));
    }

    summary.insert(QStringLiteral("totalCount"), totalCount);
    summary.insert(QStringLiteral("completedCount"), succeeded + failed + canceled + skipped);
    summary.insert(QStringLiteral("pendingCount"),
                   qMax(0, totalCount - (succeeded + failed + canceled + skipped)));
    summary.insert(QStringLiteral("succeededCount"), succeeded);
    summary.insert(QStringLiteral("importedCount"), orderedResultFiles.size());
    summary.insert(QStringLiteral("failedCount"), failed);
    summary.insert(QStringLiteral("canceledCount"), canceled);
    summary.insert(QStringLiteral("skippedCount"), skipped);
    summary.insert(QStringLiteral("notProbedCount"), notProbed);
    summary.insert(QStringLiteral("conflictBlockedCount"), conflictBlocked);
    summary.insert(QStringLiteral("wasCanceled"), wasCanceled);
    summary.insert(QStringLiteral("outputDirectory"), outputDirectory);
    summary.insert(QStringLiteral("selectedFormat"), selectedFormat);
    summary.insert(QStringLiteral("namingPolicy"), namingPolicy);
    summary.insert(QStringLiteral("conflictPolicy"), conflictPolicy);
    summary.insert(QStringLiteral("statusText"), detailText);
    summary.insert(QStringLiteral("lastError"), lastError);
    summary.insert(QStringLiteral("resultFiles"), resultFiles);
    summary.insert(QStringLiteral("orderedResultFiles"), orderedResultFiles);
    summary.insert(QStringLiteral("outcomeKey"), outcomeKey);
    summary.insert(QStringLiteral("headlineText"), headlineText);
    summary.insert(QStringLiteral("detailText"), detailText);
    summary.insert(QStringLiteral("errorCategory"), categoryKey);
    summary.insert(QStringLiteral("errorCategoryLabel"), localizedErrorCategoryLabel(categoryKey));
    summary.insert(QStringLiteral("hasPartialSuccess"), succeeded > 0 && (failed > 0 || canceled > 0));
    summary.insert(QStringLiteral("problemItems"), problemItems);
    return summary;
}

QVariantMap buildImmediateFailureSummary(const QString &message,
                                         const QString &categoryKey,
                                         int totalCount = 0)
{
    QVariantMap problem;
    problem.insert(QStringLiteral("title"), localizedText(QStringLiteral("ytDlpImport.problemGeneral")));
    problem.insert(QStringLiteral("state"), QStringLiteral("failed"));
    problem.insert(QStringLiteral("message"), message);
    problem.insert(QStringLiteral("errorCategory"), categoryKey);
    problem.insert(QStringLiteral("errorCategoryLabel"), localizedErrorCategoryLabel(categoryKey));

    const QVariantList problemItems = {problem};
    return buildTerminalSummary(QStringLiteral("failed"),
                                categoryKey,
                                message,
                                message,
                                totalCount,
                                0,
                                1,
                                0,
                                0,
                                0,
                                0,
                                false,
                                QString(),
                                QString(),
                                QString(),
                                QStringLiteral("auto-rename"),
                                QVariantList(),
                                QStringList(),
                                problemItems);
}
} // namespace

YtDlpImportService::YtDlpImportService(QObject *parent)
    : QObject(parent)
    , m_outputDirectory(defaultOutputDirectory())
{
}

YtDlpImportService::~YtDlpImportService()
{
    for (ImportItem &item : m_importItems) {
        cleanupItemArtifacts(item);
    }
    cancelProbe();
    cancelImport();
    persistDraftState();
}

QVariantMap YtDlpImportService::importJob() const
{
    return importJobToVariantMap(m_importJob);
}

QVariantList YtDlpImportService::sources() const
{
    QVariantList result;
    result.reserve(m_importSources.size());
    for (const ImportSource &source : m_importSources) {
        result.push_back(importSourceToVariantMap(source));
    }
    return result;
}

QVariantList YtDlpImportService::entries() const
{
    return entriesToVariantList(m_probeResultData.entries);
}

QVariantList YtDlpImportService::items() const
{
    if (m_importItems.isEmpty()) {
        return {};
    }
    return importItemsToVariantList(m_importItems);
}

void YtDlpImportService::setAppSettingsManager(AppSettingsManager *settingsManager)
{
    m_appSettingsManager = settingsManager;
    if (!m_appSettingsManager) {
        return;
    }

    const QVariantMap persistedSettings = m_appSettingsManager->ytDlpImportLastSettings();
    if (!persistedSettings.isEmpty()) {
        m_restoringPersistedDraft = true;
        setParallelDownloads(
            persistedSettings.value(QStringLiteral("parallelDownloads"), kDefaultParallelDownloads).toInt());
        setConflictPolicy(persistedSettings.value(QStringLiteral("conflictPolicy")).toString());
        setOutputDirectory(persistedSettings.value(QStringLiteral("outputDirectory")).toString());
        setSelectedFormat(persistedSettings.value(QStringLiteral("selectedFormat")).toString());
        setNamingPolicy(persistedSettings.value(QStringLiteral("namingPolicy")).toString());
        m_restoringPersistedDraft = false;
    }
    reloadPersistedHistory();
    restorePersistedDraft(m_appSettingsManager->ytDlpImportDraft());
}

bool YtDlpImportService::parseProbeJson(const QByteArray &payload,
                                        const QString &sourceUrl,
                                        ProbeResult *outResult,
                                        QString *outErrorMessage)
{
    if (!outResult) {
        if (outErrorMessage) {
            *outErrorMessage = QStringLiteral("Probe result output pointer is null.");
        }
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (document.isNull() || !document.isObject()) {
        if (outErrorMessage) {
            *outErrorMessage = localizedText(QStringLiteral("ytDlpImport.probeFailedInvalidJson"))
                                   .arg(parseError.errorString());
        }
        return false;
    }

    const QJsonObject root = document.object();
    ProbeResult result;
    result.sourceUrl = sourceUrl.trimmed();
    result.extractor = normalizedExtractor(root);
    result.title = firstNonEmptyString(root, {"title"});
    result.isPlaylist = root.value(QStringLiteral("entries")).isArray();
    result.playlistTitle = result.isPlaylist
        ? result.title
        : firstNonEmptyString(root, {"playlist_title"});
    result.playlistId = result.isPlaylist
        ? firstNonEmptyString(root, {"id", "playlist_id"})
        : firstNonEmptyString(root, {"playlist_id"});
    result.resolvedSourceUrl = normalizedWebpageUrl(root);
    if (result.resolvedSourceUrl.isEmpty()) {
        result.resolvedSourceUrl = result.sourceUrl;
    }
    result.isRedirected = !result.sourceUrl.isEmpty()
        && !result.resolvedSourceUrl.isEmpty()
        && result.sourceUrl != result.resolvedSourceUrl;

    if (result.isPlaylist) {
        const QJsonArray entries = root.value(QStringLiteral("entries")).toArray();
        result.entries.reserve(entries.size());
        for (int i = 0; i < entries.size(); ++i) {
            if (!entries.at(i).isObject()) {
                result.entries.push_back(unavailablePlaceholderEntry(i));
                continue;
            }

            ProbeEntry entry = normalizeEntry(entries.at(i).toObject(), i);
            if (entry.sourceUrl.isEmpty()) {
                entry.sourceUrl = result.sourceUrl;
            }
            result.entries.push_back(entry);
        }
    } else {
        ProbeEntry entry = normalizeEntry(root, 0);
        if (entry.sourceUrl.isEmpty()) {
            entry.sourceUrl = result.sourceUrl;
        }
        if (entry.title.isEmpty()) {
            entry.title = result.title;
        }
        result.entries.push_back(entry);
    }

    bool hasUnavailable = false;
    for (const ProbeEntry &entry : result.entries) {
        if (!entry.isPlayable) {
            hasUnavailable = true;
            break;
        }
    }
    result.hasUnavailableEntries = hasUnavailable;

    *outResult = result;
    if (outErrorMessage) {
        outErrorMessage->clear();
    }
    return true;
}

QVariantMap YtDlpImportService::importJobToVariantMap(const ImportJob &job)
{
    if (job.jobId.trimmed().isEmpty()) {
        return {};
    }

    QVariantMap map;
    map.insert(QStringLiteral("jobId"), job.jobId);
    map.insert(QStringLiteral("createdAtMs"), job.createdAtMs);
    map.insert(QStringLiteral("startedAtMs"), job.startedAtMs);
    map.insert(QStringLiteral("finishedAtMs"), job.finishedAtMs);
    map.insert(QStringLiteral("persistenceEligibility"),
               persistenceEligibilityKey(job.persistenceEligibility));
    map.insert(QStringLiteral("defaultsSnapshot"), job.defaultsSnapshot);
    map.insert(QStringLiteral("isRunning"), job.startedAtMs > 0 && job.finishedAtMs == 0);
    return map;
}

QVariantMap YtDlpImportService::importSourceToVariantMap(const ImportSource &source)
{
    QVariantMap map;
    map.insert(QStringLiteral("sourceId"), source.sourceId);
    map.insert(QStringLiteral("createdAtMs"), source.createdAtMs);
    map.insert(QStringLiteral("lastProbedAtMs"), source.lastProbedAtMs);
    map.insert(QStringLiteral("sourceStatus"), sourceStatusKey(source.status));
    map.insert(QStringLiteral("failureType"), failureTypeKey(source.failureType));
    map.insert(QStringLiteral("retryEligibility"), retryEligibilityKey(source.retryEligibility));
    map.insert(QStringLiteral("persistenceEligibility"),
               persistenceEligibilityKey(source.persistenceEligibility));
    map.insert(QStringLiteral("immutableSourceInput"), source.immutableSourceInput);
    map.insert(QStringLiteral("metadataSnapshot"), source.metadataSnapshot);
    map.insert(QStringLiteral("queueMetadata"), source.queueMetadata);
    map.insert(QStringLiteral("runtimeState"), source.runtimeState);
    map.insert(QStringLiteral("finalResultState"), source.finalResultState);
    return map;
}

QVariantMap YtDlpImportService::probeResultToVariantMap(const ProbeResult &result)
{
    int availableEntryCount = 0;
    int unavailableEntryCount = 0;
    for (const ProbeEntry &entry : result.entries) {
        if (entry.isPlayable) {
            ++availableEntryCount;
        } else {
            ++unavailableEntryCount;
        }
    }

    QVariantMap map;
    map.insert(QStringLiteral("sourceUrl"), result.sourceUrl);
    map.insert(QStringLiteral("resolvedSourceUrl"), result.resolvedSourceUrl);
    map.insert(QStringLiteral("extractor"), result.extractor);
    map.insert(QStringLiteral("title"), result.title);
    map.insert(QStringLiteral("playlistTitle"), result.playlistTitle);
    map.insert(QStringLiteral("playlistId"), result.playlistId);
    map.insert(QStringLiteral("isPlaylist"), result.isPlaylist);
    map.insert(QStringLiteral("hasUnavailableEntries"), result.hasUnavailableEntries);
    map.insert(QStringLiteral("isRedirected"), result.isRedirected);
    map.insert(QStringLiteral("entryCount"), result.entries.size());
    map.insert(QStringLiteral("availableEntryCount"), availableEntryCount);
    map.insert(QStringLiteral("unavailableEntryCount"), unavailableEntryCount);
    map.insert(QStringLiteral("entries"), entriesToVariantList(result.entries));
    map.insert(QStringLiteral("sourceType"),
               result.isPlaylist ? QStringLiteral("playlist") : QStringLiteral("single"));
    return map;
}

QVariantMap YtDlpImportService::probeEntryToVariantMap(const ProbeEntry &entry)
{
    QVariantMap map;
    map.insert(QStringLiteral("sourceUrl"), entry.sourceUrl);
    map.insert(QStringLiteral("extractor"), entry.extractor);
    map.insert(QStringLiteral("title"), entry.title);
    map.insert(QStringLiteral("entryId"), entry.entryId);
    map.insert(QStringLiteral("playlistIndex"), entry.playlistIndex);
    map.insert(QStringLiteral("duration"), entry.duration);
    map.insert(QStringLiteral("thumbnail"), entry.thumbnail);
    map.insert(QStringLiteral("webpageUrl"), entry.webpageUrl);
    map.insert(QStringLiteral("availability"), entry.availability);
    map.insert(QStringLiteral("isPlayable"), entry.isPlayable);
    map.insert(QStringLiteral("metadataOrder"), entry.metadataOrder);
    return map;
}

QVariantMap YtDlpImportService::conflictResolutionToVariantMap(
    const ImportItem::ConflictResolutionInfo &info)
{
    QVariantMap map;
    map.insert(QStringLiteral("requestedOutputFile"), info.requestedOutputFile);
    map.insert(QStringLiteral("resolvedOutputFile"), info.resolvedOutputFile);
    map.insert(QStringLiteral("resolutionKey"), info.resolutionKey);
    map.insert(QStringLiteral("collisionRuleKey"), info.collisionRuleKey);
    map.insert(QStringLiteral("hadConflict"), info.hadConflict);
    map.insert(QStringLiteral("targetExistsOnDisk"), info.targetExistsOnDisk);
    map.insert(QStringLiteral("finalizationStrategyKey"), info.finalizationStrategyKey);
    return map;
}

QVariantMap YtDlpImportService::importItemToVariantMap(const ImportItem &item)
{
    QVariantMap map;
    map.insert(QStringLiteral("itemId"), item.entryId);
    map.insert(QStringLiteral("entryId"), item.entryId);
    map.insert(QStringLiteral("sourceId"), item.sourceId);
    map.insert(QStringLiteral("sourceUrl"), item.sourceUrl);
    map.insert(QStringLiteral("extractor"), item.extractor);
    map.insert(QStringLiteral("title"), item.title);
    map.insert(QStringLiteral("entrySourceId"), item.entrySourceId);
    map.insert(QStringLiteral("duration"), item.duration);
    map.insert(QStringLiteral("thumbnail"), item.thumbnail);
    map.insert(QStringLiteral("webpageUrl"), item.webpageUrl);
    map.insert(QStringLiteral("availability"), item.availability);
    map.insert(QStringLiteral("playlistIndex"), item.playlistIndex);
    map.insert(QStringLiteral("metadataOrder"), item.metadataOrder);
    map.insert(QStringLiteral("isPlayable"), item.isPlayable);
    map.insert(QStringLiteral("state"), itemStateKey(item.state));
    map.insert(QStringLiteral("progress"), item.progress);
    map.insert(QStringLiteral("statusText"), item.statusText);
    map.insert(QStringLiteral("errorText"), item.errorText);
    map.insert(QStringLiteral("plannedOutputFile"), item.plannedOutputFile);
    map.insert(QStringLiteral("finalOutputFile"), item.finalOutputFile);
    map.insert(QStringLiteral("resultTrackInsertIndex"), item.resultTrackInsertIndex);
    map.insert(QStringLiteral("retryCount"), item.retryCount);
    map.insert(QStringLiteral("createdAtMs"), item.createdAtMs);
    map.insert(QStringLiteral("updatedAtMs"), item.updatedAtMs);
    map.insert(QStringLiteral("failureType"), failureTypeKey(item.failureType));
    map.insert(QStringLiteral("retryEligibility"), retryEligibilityKey(item.retryEligibility));
    map.insert(QStringLiteral("persistenceEligibility"),
               persistenceEligibilityKey(item.persistenceEligibility));
    map.insert(QStringLiteral("immutableSourceInput"), item.immutableSourceInput);
    map.insert(QStringLiteral("metadataSnapshot"), item.metadataSnapshot);
    map.insert(QStringLiteral("queueMetadata"), item.queueMetadata);
    map.insert(QStringLiteral("runtimeState"), item.runtimeState);
    map.insert(QStringLiteral("finalResultState"), item.finalResultState);
    map.insert(QStringLiteral("previewDiagnostics"), item.previewDiagnostics);
    map.insert(QStringLiteral("conflictResolution"),
               conflictResolutionToVariantMap(item.conflictResolution));
    return map;
}

void YtDlpImportService::setSourceUrl(const QString &sourceUrl)
{
    const QString normalized = sourceUrl.trimmed();
    if (m_sourceUrl == normalized) {
        return;
    }

    m_sourceUrl = normalized;
    emit sourceUrlChanged();
}

void YtDlpImportService::setOutputDirectory(const QString &outputDirectory)
{
    const QString normalized = normalizeLocalPath(outputDirectory);
    if (m_outputDirectory == normalized) {
        return;
    }

    m_outputDirectory = normalized;
    emit outputDirectoryChanged();
    clearUnstartedOutputPlan();
    refreshPreviewOutputPlan();
    if (!m_restoringPersistedDraft) {
        persistSettingsSnapshot();
        persistRecentOutputDirectory(m_outputDirectory);
        persistDraftState();
    }
}

void YtDlpImportService::setSelectedFormat(const QString &selectedFormat)
{
    const QString normalized = selectedFormat.trimmed().toLower();
    const QString resolved = normalized == QStringLiteral("m4a")
            || normalized == QStringLiteral("opus")
        ? normalized
        : QStringLiteral("mp3");
    if (m_selectedFormat == resolved) {
        return;
    }

    m_selectedFormat = resolved;
    bool sourcesChanged = false;
    for (ImportSource &source : m_importSources) {
        if (source.lastProbedAtMs <= 0) {
            continue;
        }
        const QString previousSnapshot =
            source.runtimeState.value(QStringLiteral("probeFormatSnapshot")).toString().trimmed();
        if (previousSnapshot == m_selectedFormat) {
            continue;
        }
        source.runtimeState.insert(QStringLiteral("isStale"), true);
        source.runtimeState.insert(QStringLiteral("probeFormatMismatch"), true);
        sourcesChanged = true;
    }
    emit selectedFormatChanged();
    clearUnstartedOutputPlan();
    refreshPreviewOutputPlan();
    if (!m_restoringPersistedDraft) {
        persistSettingsSnapshot();
        persistDraftState();
    }
    if (sourcesChanged) {
        publishSources();
    }
}

void YtDlpImportService::setNamingPolicy(const QString &namingPolicy)
{
    const QString normalized = namingPolicy.trimmed().toLower();
    const QString resolved = normalized == QStringLiteral("title-only")
        || normalized == QStringLiteral("source-title-entry-title")
        ? normalized
        : QStringLiteral("auto");
    if (m_namingPolicy == resolved) {
        return;
    }

    m_namingPolicy = resolved;
    emit namingPolicyChanged();
    clearUnstartedOutputPlan();
    refreshPreviewOutputPlan();
    if (!m_restoringPersistedDraft) {
        persistSettingsSnapshot();
        persistDraftState();
    }
}

void YtDlpImportService::setConflictPolicy(const QString &conflictPolicy)
{
    const QString resolved = normalizeConflictPolicy(conflictPolicy);
    if (m_conflictPolicy == resolved) {
        return;
    }

    m_conflictPolicy = resolved;
    emit conflictPolicyChanged();
    clearUnstartedOutputPlan();
    refreshPreviewOutputPlan();
    if (!m_restoringPersistedDraft) {
        persistSettingsSnapshot();
        persistDraftState();
    }
}

void YtDlpImportService::setParallelDownloads(int parallelDownloads)
{
    const int normalized = normalizeParallelDownloads(parallelDownloads);
    if (m_parallelDownloads == normalized) {
        return;
    }

    m_parallelDownloads = normalized;
    emit parallelDownloadsChanged();
    if (!m_restoringPersistedDraft) {
        persistSettingsSnapshot();
        persistDraftState();
    }
}

QVariantMap YtDlpImportService::replaceSourceUrl(const QString &sourceUrl)
{
    return applySourceIntake({sourceUrl}, true);
}

QVariantMap YtDlpImportService::appendSourceUrl(const QString &sourceUrl)
{
    return applySourceIntake({sourceUrl}, false);
}

QVariantMap YtDlpImportService::replaceSourcesFromText(const QString &sourceText)
{
    return applySourceIntake(splitSourceLines(sourceText), true);
}

QVariantMap YtDlpImportService::appendSourcesFromText(const QString &sourceText)
{
    return applySourceIntake(splitSourceLines(sourceText), false);
}

QVariantMap YtDlpImportService::replaceSourcesFromVariantList(const QVariantList &sourceUrls)
{
    QStringList rawInputs;
    rawInputs.reserve(sourceUrls.size());
    for (const QVariant &value : sourceUrls) {
        rawInputs.push_back(value.toString());
    }
    return applySourceIntake(rawInputs, true);
}

QVariantMap YtDlpImportService::appendSourcesFromVariantList(const QVariantList &sourceUrls)
{
    QStringList rawInputs;
    rawInputs.reserve(sourceUrls.size());
    for (const QVariant &value : sourceUrls) {
        rawInputs.push_back(value.toString());
    }
    return applySourceIntake(rawInputs, false);
}

bool YtDlpImportService::canRemoveSource(const QString &sourceId) const
{
    const int sourceIndex = indexOfSourceId(sourceId);
    if (sourceIndex < 0 || m_isProbing) {
        return false;
    }
    if (!m_isRunning) {
        return true;
    }
    return sourceHasOnlyPendingTailEntries(sourceId);
}

bool YtDlpImportService::canMoveSourceUp(const QString &sourceId) const
{
    const int sourceIndex = indexOfSourceId(sourceId);
    return sourceIndex > 0 && !m_isRunning && !m_isProbing && m_importJob.startedAtMs <= 0;
}

bool YtDlpImportService::canMoveSourceDown(const QString &sourceId) const
{
    const int sourceIndex = indexOfSourceId(sourceId);
    return sourceIndex >= 0 && sourceIndex < (m_importSources.size() - 1)
        && !m_isRunning && !m_isProbing && m_importJob.startedAtMs <= 0;
}

bool YtDlpImportService::removeSourceById(const QString &sourceId)
{
    const int sourceIndex = indexOfSourceId(sourceId);
    if (sourceIndex < 0 || !canRemoveSource(sourceId)) {
        return false;
    }

    const QList<int> itemIndexes = itemIndexesForSource(sourceId);
    for (int i = itemIndexes.size() - 1; i >= 0; --i) {
        const int itemIndex = itemIndexes.at(i);
        if (itemIndex >= 0 && itemIndex < m_importItems.size()) {
            ImportItem item = m_importItems.takeAt(itemIndex);
            cleanupItemArtifacts(item);
        }
    }
    m_importSources.removeAt(sourceIndex);
    refreshSourceQueuePositions();
    if (!m_isRunning) {
        rebuildPreviewItemsFromSources();
    }
    publishSources();
    publishItems(true);
    updateBatchProgress();
    return true;
}

int YtDlpImportService::removeSourcesById(const QVariantList &sourceIds)
{
    int removedCount = 0;
    for (const QVariant &value : sourceIds) {
        if (removeSourceById(value.toString())) {
            ++removedCount;
        }
    }
    return removedCount;
}

int YtDlpImportService::clearFailedProbes()
{
    QVariantList failedSourceIds;
    for (const ImportSource &source : std::as_const(m_importSources)) {
        if (source.status == SourceProbeFailed) {
            failedSourceIds.push_back(source.sourceId);
        }
    }
    return removeSourcesById(failedSourceIds);
}

int YtDlpImportService::clearCompletedImports()
{
    QVariantList completedSourceIds;
    for (const ImportSource &source : std::as_const(m_importSources)) {
        if (source.status == SourceCompleted || source.status == SourceCompletedWithFailures) {
            completedSourceIds.push_back(source.sourceId);
        }
    }
    return removeSourcesById(completedSourceIds);
}

int YtDlpImportService::retryFailedProbes()
{
    QStringList sourceIds;
    for (const ImportSource &source : std::as_const(m_importSources)) {
        if (source.status == SourceProbeFailed && source.retryEligibility == RetryAllowed) {
            sourceIds.push_back(source.sourceId);
        }
    }
    if (sourceIds.isEmpty()) {
        return 0;
    }
    return enqueueProbeSources(sourceIds) ? sourceIds.size() : 0;
}

int YtDlpImportService::retryFailedImports()
{
    if (m_isRunning || m_isProbing || m_activeRunSerial <= 0) {
        return 0;
    }

    int retriedCount = 0;
    ++m_activeRunSerial;
    for (ImportItem &item : m_importItems) {
        const bool isRetryTarget = (item.state == Failed || item.state == Canceled)
            && item.retryEligibility == RetryAllowed;
        if (!isRetryTarget) {
            item.queueMetadata.insert(QStringLiteral("includeInNextRun"), false);
            item.queueMetadata.remove(QStringLiteral("runSerial"));
            continue;
        }

        archiveItemAttempt(item, QStringLiteral("retry-failed-import"));
        item.retryCount += 1;
        item.state = Pending;
        item.progress = 0.0;
        item.statusText = localizedText(QStringLiteral("ytDlpImport.itemStarting"));
        item.errorText.clear();
        item.updatedAtMs = nowMs();
        item.failureType = NoFailure;
        item.retryEligibility = RetryNotApplicable;
        item.queueMetadata.insert(QStringLiteral("includeInNextRun"), true);
        item.queueMetadata.insert(QStringLiteral("runSerial"), m_activeRunSerial);
        item.finalResultState.insert(QStringLiteral("terminalResult"), QStringLiteral("none"));
        item.finalResultState.insert(QStringLiteral("message"), QString());
        syncEntryRuntimeLayers(item);
        ++retriedCount;
    }

    if (retriedCount > 0) {
        setFinalSummary(QVariantMap());
        setStatusText(QString());
        setLastError(QString());
        rebuildPreviewItemsFromSources();
        publishItems(true);
    }
    return retriedCount;
}

int YtDlpImportService::retrySelectedItemsById(const QVariantList &itemIds)
{
    if (m_isRunning || m_isProbing || m_activeRunSerial <= 0 || itemIds.isEmpty()) {
        return 0;
    }

    QSet<QString> requestedIds;
    for (const QVariant &value : itemIds) {
        const QString itemId = value.toString().trimmed();
        if (!itemId.isEmpty()) {
            requestedIds.insert(itemId);
        }
    }
    if (requestedIds.isEmpty()) {
        return 0;
    }

    int retriedCount = 0;
    ++m_activeRunSerial;
    for (ImportItem &item : m_importItems) {
        const bool isRetryTarget = requestedIds.contains(item.entryId)
            && item.retryEligibility == RetryAllowed
            && (item.state == Failed || item.state == Canceled || item.state == Skipped);
        if (!isRetryTarget) {
            item.queueMetadata.insert(QStringLiteral("includeInNextRun"), false);
            item.queueMetadata.remove(QStringLiteral("runSerial"));
            continue;
        }

        archiveItemAttempt(item, QStringLiteral("retry-selected-import"));
        item.retryCount += 1;
        item.state = Pending;
        item.progress = 0.0;
        item.statusText = localizedText(QStringLiteral("ytDlpImport.itemStarting"));
        item.errorText.clear();
        item.updatedAtMs = nowMs();
        item.failureType = NoFailure;
        item.retryEligibility = RetryNotApplicable;
        item.queueMetadata.insert(QStringLiteral("includeInNextRun"), true);
        item.queueMetadata.insert(QStringLiteral("runSerial"), m_activeRunSerial);
        item.finalResultState.insert(QStringLiteral("terminalResult"), QStringLiteral("none"));
        item.finalResultState.insert(QStringLiteral("message"), QString());
        syncEntryRuntimeLayers(item);
        ++retriedCount;
    }

    if (retriedCount > 0) {
        setFinalSummary(QVariantMap());
        setStatusText(QString());
        setLastError(QString());
        rebuildPreviewItemsFromSources();
        publishItems(true);
    }
    return retriedCount;
}

bool YtDlpImportService::moveSourceUp(const QString &sourceId)
{
    if (!canMoveSourceUp(sourceId)) {
        return false;
    }
    const int sourceIndex = indexOfSourceId(sourceId);
    m_importSources.swapItemsAt(sourceIndex, sourceIndex - 1);
    refreshSourceQueuePositions();
    rebuildPreviewItemsFromSources();
    publishSources();
    publishItems(true);
    return true;
}

bool YtDlpImportService::moveSourceDown(const QString &sourceId)
{
    if (!canMoveSourceDown(sourceId)) {
        return false;
    }
    const int sourceIndex = indexOfSourceId(sourceId);
    m_importSources.swapItemsAt(sourceIndex, sourceIndex + 1);
    refreshSourceQueuePositions();
    rebuildPreviewItemsFromSources();
    publishSources();
    publishItems(true);
    return true;
}

bool YtDlpImportService::probeSource()
{
    if (!m_sourceUrl.trimmed().isEmpty()) {
        const QString normalized = normalizedSourceUrl(m_sourceUrl);
        if (!normalized.isEmpty()) {
            for (const ImportSource &source : std::as_const(m_importSources)) {
                const QString sourceUrl =
                    source.immutableSourceInput.value(QStringLiteral("normalizedUrl")).toString();
                if (sourceUrl == normalized) {
                    return probeSourceById(source.sourceId);
                }
            }
        }
    }
    return probeSourceUrl(m_sourceUrl);
}

bool YtDlpImportService::probeSourceUrl(const QString &sourceUrl)
{
    if (m_isProbing) {
        setLastError(localizedText(QStringLiteral("ytDlpImport.probeAlreadyRunning")));
        return false;
    }

    if (m_isRunning) {
        setLastError(localizedText(QStringLiteral("ytDlpImport.importAlreadyRunning")));
        return false;
    }

    const QVariantMap intakeResult = replaceSourceUrl(sourceUrl);
    if (!intakeResult.value(QStringLiteral("ok")).toBool()) {
        return false;
    }
    if (intakeResult.value(QStringLiteral("acceptedCount")).toInt() <= 0 || m_importSources.isEmpty()) {
        setStatusText(QString());
        setLastError(localizedText(QStringLiteral("ytDlpImport.probeInvalidUrl")));
        return false;
    }

    return probeSourceById(m_importSources.constFirst().sourceId);
}

bool YtDlpImportService::probeSourceById(const QString &sourceId)
{
    return enqueueProbeSources({sourceId});
}

bool YtDlpImportService::probeAllSources()
{
    QStringList targetSourceIds;
    for (const ImportSource &source : std::as_const(m_importSources)) {
        if (sourceNeedsProbe(source)) {
            targetSourceIds.push_back(source.sourceId);
        }
    }
    return enqueueProbeSources(targetSourceIds);
}

bool YtDlpImportService::probeFailedOrStaleSources()
{
    QStringList targetSourceIds;
    for (const ImportSource &source : std::as_const(m_importSources)) {
        if (sourceNeedsRetryProbe(source)) {
            targetSourceIds.push_back(source.sourceId);
        }
    }
    return enqueueProbeSources(targetSourceIds);
}

bool YtDlpImportService::startImport()
{
    if (m_isProbing) {
        setLastError(localizedText(QStringLiteral("ytDlpImport.importBlockedWhileProbing")));
        return false;
    }

    if (m_isRunning) {
        setLastError(localizedText(QStringLiteral("ytDlpImport.importAlreadyRunning")));
        return false;
    }

    const bool hasPreparedPreview = std::any_of(m_importSources.cbegin(),
                                                m_importSources.cend(),
                                                [](const ImportSource &source) {
                                                    return source.lastProbedAtMs > 0
                                                        && !source.metadataSnapshot.isEmpty();
                                                });
    if (!hasPreparedPreview || m_importItems.isEmpty()) {
        setLastError(localizedText(QStringLiteral("ytDlpImport.importRequiresProbe")));
        return false;
    }

    setFinalSummary(QVariantMap());

    if (!m_appSettingsManager) {
        const QString message = localizedText(QStringLiteral("ytDlpImport.probeRuntimeUnavailable"));
        setStatusText(message);
        setLastError(message);
        setFinalSummary(buildImmediateFailureSummary(message, QStringLiteral("dependency")));
        return false;
    }

    const QVariantMap runtime = m_appSettingsManager->validateYtDlpImportRuntime(m_selectedFormat);
    if (!runtime.value(QStringLiteral("ok")).toBool()) {
        const QString message = runtime.value(QStringLiteral("message")).toString().trimmed();
        setStatusText(message);
        setLastError(message);
        const QString category = runtime.value(QStringLiteral("errorCategory")).toString().trimmed().isEmpty()
            ? QStringLiteral("dependency")
            : runtime.value(QStringLiteral("errorCategory")).toString().trimmed();
        setFinalSummary(buildImmediateFailureSummary(message, category));
        return false;
    }

    const QString normalizedOutputDirectory = m_outputDirectory.isEmpty()
        ? defaultOutputDirectory()
        : m_outputDirectory;
    QDir outputDir(normalizedOutputDirectory);
    if (!outputDir.exists() && !QDir().mkpath(normalizedOutputDirectory)) {
        const QString message =
            localizedText(QStringLiteral("ytDlpImport.importInvalidOutputDirectory"))
                .arg(normalizedOutputDirectory);
        setStatusText(message);
        setLastError(message);
        setFinalSummary(buildImmediateFailureSummary(message, QStringLiteral("output")));
        return false;
    }
    setOutputDirectory(normalizedOutputDirectory);

    int queuedRunSerial = 0;
    for (const ImportItem &item : std::as_const(m_importItems)) {
        if (!item.queueMetadata.value(QStringLiteral("includeInNextRun"), true).toBool()) {
            continue;
        }
        if (item.state != Pending && item.state != Failed && item.state != Canceled && item.state != Skipped) {
            continue;
        }
        queuedRunSerial = qMax(queuedRunSerial,
                               item.queueMetadata.value(QStringLiteral("runSerial")).toInt());
    }
    if (queuedRunSerial > 0) {
        m_activeRunSerial = queuedRunSerial;
    } else {
        m_activeRunSerial += 1;
    }

    if (!prepareImportQueue()) {
        if (m_finalSummary.isEmpty() && !m_lastError.trimmed().isEmpty()) {
            setFinalSummary(buildImmediateFailureSummary(m_lastError,
                                                         normalizedErrorCategoryKey(m_lastError),
                                                         m_importItems.size()));
        }
        return false;
    }

    setLastError(QString());
    setStatusText(localizedText(QStringLiteral("ytDlpImport.importStarted")));
    setCancelRequested(false);
    if (m_importJob.startedAtMs <= 0) {
        m_importJob.startedAtMs = nowMs();
        m_importJob.finishedAtMs = 0;
        publishImportJob();
    }
    setIsRunning(true);
    updateBatchProgress();
    syncSourceRuntimeFromItems();
    return scheduleImportWorkers();
}

void YtDlpImportService::cancelProbe()
{
    if (!m_probeProcess) {
        return;
    }

    setCancelRequested(true);
    m_probeProcess->kill();
}

void YtDlpImportService::cancelImport()
{
    if (!m_isRunning) {
        return;
    }

    setCancelRequested(true);
    m_cancelPendingItemIds.clear();
    markPendingItemsCanceled(m_activeRunSerial);
    updateBatchProgress();
    syncSourceRuntimeFromItems();
    updateImportRuntimeStatusText();
    bool hasActiveProcesses = false;
    for (auto it = m_activeImportProcesses.cbegin(); it != m_activeImportProcesses.cend(); ++it) {
        if (!it.value()) {
            continue;
        }
        hasActiveProcesses = true;
        m_cancelPendingItemIds.insert(it.key());
        requestImportProcessCancellation(it.key(), it.value());
    }
    if (hasActiveProcesses) {
        return;
    }

    finalizeImportRun(true);
}

void YtDlpImportService::clear()
{
    cancelProbe();
    cancelImport();
    setSourceUrl(QString());
    clearProbeResult();
    clearImportSession();
    setStatusText(QString());
    setLastError(QString());
    persistDraftState();
}

void YtDlpImportService::clearProbeResult()
{
    const bool hadResult = m_hasProbeResult || !m_probeResult.isEmpty() || !m_probeResultData.entries.isEmpty();
    m_probeResultData = ProbeResult();
    m_probeResult = QVariantMap();
    m_hasProbeResult = false;
    if (hadResult) {
        emit probeResultChanged();
    }
}

void YtDlpImportService::clearImportSession()
{
    const bool hadSources = !m_importSources.isEmpty();
    const bool hadJob = !m_importJob.jobId.isEmpty();
    clearQueueState();
    m_importSources.clear();
    m_importJob = ImportJob();
    m_activeRunSerial = 0;
    if (hadSources) {
        emit sourcesChanged();
    }
    if (hadJob) {
        emit importJobChanged();
    }
}

void YtDlpImportService::clearQueueState()
{
    const bool hadItems = !m_importItems.isEmpty();
    for (ImportItem &item : m_importItems) {
        cleanupItemArtifacts(item);
    }
    m_importItems.clear();
    m_queuePrepared = false;
    m_activeImportProcesses.clear();
    m_activeImportItemIndexes.clear();
    m_importStdoutBuffers.clear();
    m_importStderrLineBuffers.clear();
    m_importStderrBuffers.clear();
    m_cancelPendingItemIds.clear();
    setCancelRequested(false);
    setIsRunning(false);
    setBatchProgress(0.0);
    setFinalSummary(QVariantMap());
    m_probeQueueSourceIds.clear();
    m_currentProbeSourceId.clear();
    if (hadItems) {
        emit itemsChanged();
    }
}

void YtDlpImportService::cleanupStagingRootDirectory()
{
    if (m_outputDirectory.trimmed().isEmpty()) {
        return;
    }

    QDir stagingRoot(stagingRootDirectory(m_outputDirectory));
    if (stagingRoot.exists()) {
        stagingRoot.removeRecursively();
    }
}

void YtDlpImportService::finalizeImportProcess(QProcess *process)
{
    unregisterActiveImportProcess(activeImportItemIdForProcess(process));
    if (process) {
        process->deleteLater();
    }
}

void YtDlpImportService::cleanupItemArtifacts(ImportItem &item)
{
    if (!item.stagingOutputFile.isEmpty()) {
        QFile::remove(item.stagingOutputFile);
    }
    if (!item.stagingDirectory.isEmpty()) {
        QDir(item.stagingDirectory).removeRecursively();
    }
}

bool YtDlpImportService::finalizeSuccessfulImport(ImportItem &item)
{
    if (item.stagingOutputFile.isEmpty() || item.plannedOutputFile.isEmpty()) {
        return false;
    }

    if (!QFileInfo::exists(item.stagingOutputFile)) {
        return false;
    }

    if (QFileInfo::exists(item.plannedOutputFile)) {
        return false;
    }

    const QString finalDirectory = QFileInfo(item.plannedOutputFile).absolutePath();
    if (!finalDirectory.isEmpty() && !QDir().mkpath(finalDirectory)) {
        return false;
    }

    QFile stagedFile(item.stagingOutputFile);
    if (!stagedFile.rename(item.plannedOutputFile)) {
        return false;
    }

    item.finalOutputFile = item.plannedOutputFile;
    if (!item.stagingDirectory.isEmpty()) {
        QDir(item.stagingDirectory).removeRecursively();
    }
    item.stagingOutputFile.clear();
    item.stagingDirectory.clear();
    return true;
}

void YtDlpImportService::finishImportWithError(const QString &itemId, const QString &message)
{
    const int itemIndex = activeImportItemIndex(itemId);
    if (itemIndex >= 0 && itemIndex < m_importItems.size()) {
        ImportItem &item = m_importItems[itemIndex];
        item.state = m_cancelRequested ? Canceled : Failed;
        item.progress = m_cancelRequested ? item.progress : 1.0;
        item.errorText = message;
        item.statusText = message;
        item.updatedAtMs = nowMs();
        item.failureType = failureTypeFromCategoryKey(
            m_cancelRequested ? QStringLiteral("canceled") : normalizedErrorCategoryKey(message));
        item.retryEligibility = retryEligibilityForFailureType(item.failureType, item.isPlayable);
        cleanupItemArtifacts(item);
        syncEntryRuntimeLayers(item);
        publishItems();
    }

    setLastError(message);
    setStatusText(message);
    updateBatchProgress();
    syncSourceRuntimeFromItems();
}

void YtDlpImportService::finalizeImportRun(bool wasCanceled)
{
    m_cancelPendingItemIds.clear();
    m_importJob.finishedAtMs = nowMs();
    m_importJob.persistenceEligibility = ReportPersistable;
    publishImportJob();
    if (wasCanceled) {
        markPendingItemsCanceled();
        setStatusText(localizedText(QStringLiteral("ytDlpImport.importCanceled")));
        setLastError(QString());
    } else if (m_importItems.isEmpty()) {
        setStatusText(QString());
    } else if (m_finalSummary.isEmpty()) {
        setStatusText(localizedText(QStringLiteral("ytDlpImport.importFinished")));
    }

    int succeeded = 0;
    int failed = 0;
    int canceled = 0;
    int skipped = 0;
    int notProbed = 0;
    int conflictBlocked = 0;
    QVariantList resultFiles;
    QVariantList problemItems;
    QList<ImportItem> orderedSucceededItems;
    for (const ImportItem &item : std::as_const(m_importItems)) {
        const bool belongsToActiveRun =
            item.queueMetadata.value(QStringLiteral("runSerial")).toInt() == m_activeRunSerial;
        if (!belongsToActiveRun) {
            continue;
        }
        switch (item.state) {
        case Succeeded:
            ++succeeded;
            if (!item.finalOutputFile.isEmpty()) {
                resultFiles.push_back(item.finalOutputFile);
                orderedSucceededItems.push_back(item);
            }
            break;
        case Failed:
            ++failed;
            if (item.conflictResolution.resolutionKey == QStringLiteral("fail-on-conflict")) {
                ++conflictBlocked;
            }
            problemItems.push_back(buildProblemItemSummary(item));
            break;
        case Canceled:
            ++canceled;
            problemItems.push_back(buildProblemItemSummary(item));
            break;
        case Skipped:
            ++skipped;
            if (item.conflictResolution.resolutionKey == QStringLiteral("skip-on-conflict")) {
                ++conflictBlocked;
            }
            problemItems.push_back(buildProblemItemSummary(item));
            break;
        case Running:
        case Pending:
        default:
            break;
        }
    }

    for (const ImportSource &source : std::as_const(m_importSources)) {
        const bool sourceHasPreparedMetadata =
            source.lastProbedAtMs > 0 && !source.metadataSnapshot.isEmpty();
        const bool sourceIsNotProbed = !sourceHasPreparedMetadata
            || source.status == SourceProbeFailed
            || source.runtimeState.value(QStringLiteral("isStale")).toBool();
        if (!sourceIsNotProbed) {
            continue;
        }
        ++notProbed;
        problemItems.push_back(buildProblemSourceSummary(source));
    }

    std::sort(orderedSucceededItems.begin(),
              orderedSucceededItems.end(),
              [](const ImportItem &a, const ImportItem &b) {
                  if (a.resultTrackInsertIndex != b.resultTrackInsertIndex) {
                      return a.resultTrackInsertIndex < b.resultTrackInsertIndex;
                  }
                  if (a.playlistIndex != b.playlistIndex) {
                      return a.playlistIndex < b.playlistIndex;
                  }
                  return a.metadataOrder < b.metadataOrder;
              });

    QStringList orderedResultFiles;
    orderedResultFiles.reserve(orderedSucceededItems.size());
    for (const ImportItem &item : std::as_const(orderedSucceededItems)) {
        if (!item.finalOutputFile.trimmed().isEmpty()) {
            orderedResultFiles.push_back(item.finalOutputFile);
        }
    }

    if (succeeded == 0 && failed == 0 && canceled == 0 && skipped == 0
        && !m_lastError.trimmed().isEmpty()) {
        setFinalSummary(buildImmediateFailureSummary(m_lastError,
                                                     normalizedErrorCategoryKey(m_lastError),
                                                     0));
        setStatusText(m_lastError);
        setIsRunning(false);
        setCancelRequested(false);
        setBatchProgress(m_importItems.isEmpty() ? 0.0 : 1.0);
        cleanupStagingRootDirectory();
        return;
    }

    const QString outcomeKey = summaryOutcomeKey(succeeded, failed, canceled, wasCanceled);
    const QString categoryKey = summaryCategoryKey(problemItems);
    const QString detailText = localizedText(QStringLiteral("ytDlpImport.summaryDetailPattern"))
                                   .arg(succeeded)
                                   .arg(failed)
                                   .arg(canceled)
                                   .arg(skipped);
    const QVariantMap summary = buildTerminalSummary(outcomeKey,
                                                     categoryKey,
                                                     detailText,
                                                     m_lastError,
                                                     succeeded + failed + canceled + skipped + notProbed,
                                                     succeeded,
                                                     failed,
                                                     canceled,
                                                     skipped,
                                                     notProbed,
                                                     conflictBlocked,
                                                     wasCanceled,
                                                     m_outputDirectory,
                                                     m_selectedFormat,
                                                     m_namingPolicy,
                                                     m_conflictPolicy,
                                                     resultFiles,
                                                     orderedResultFiles,
                                                     problemItems);
    QVariantMap enrichedSummary = summary;
    enrichedSummary.insert(QStringLiteral("jobId"), m_importJob.jobId);
    enrichedSummary.insert(QStringLiteral("createdAtMs"), m_importJob.createdAtMs);
    enrichedSummary.insert(QStringLiteral("startedAtMs"), m_importJob.startedAtMs);
    enrichedSummary.insert(QStringLiteral("finishedAtMs"), m_importJob.finishedAtMs);
    setFinalSummary(enrichedSummary);
    setStatusText(detailText);
    setIsRunning(false);
    setCancelRequested(false);
    setBatchProgress(m_importItems.isEmpty() ? 0.0 : 1.0);
    cleanupStagingRootDirectory();
    syncSourceRuntimeFromItems();
    if (!orderedResultFiles.isEmpty()) {
        emit playlistImportReady(orderedResultFiles);
    }
}

bool YtDlpImportService::scheduleImportWorkers()
{
    if (m_cancelRequested) {
        if (m_activeImportProcesses.isEmpty()) {
            markPendingItemsCanceled();
            finalizeImportRun(true);
        }
        return false;
    }

    bool startedWorker = false;
    while (canStartMoreWorkers()) {
        const int nextIndex = nextPendingItemIndexForRun();
        if (nextIndex < 0) {
            break;
        }
        if (!startNextImportItem()) {
            break;
        }
        startedWorker = true;
    }

    if (m_activeImportProcesses.isEmpty() && nextPendingItemIndexForRun() < 0) {
        finalizeImportRun(false);
    }

    updateImportRuntimeStatusText();
    return startedWorker || !m_activeImportProcesses.isEmpty();
}

bool YtDlpImportService::canStartMoreWorkers() const
{
    return !m_cancelRequested
        && m_activeImportProcesses.size() < m_parallelDownloads;
}

int YtDlpImportService::nextPendingItemIndexForRun() const
{
    for (int i = 0; i < m_importItems.size(); ++i) {
        const ImportItem &item = m_importItems.at(i);
        if (item.state == Pending
            && item.isPlayable
            && item.queueMetadata.value(QStringLiteral("runSerial")).toInt() == m_activeRunSerial) {
            return i;
        }
    }
    return -1;
}

bool YtDlpImportService::prepareImportQueue()
{
    if (m_importItems.isEmpty()) {
        const QString message = localizedText(QStringLiteral("ytDlpImport.importRequiresProbe"));
        setLastError(message);
        setStatusText(message);
        return false;
    }

    struct OrderKey {
        int sourceQueuePosition = -1;
        int itemIndex = -1;
        int rank = -1;
        bool hasPlaylistIndex = false;
    };
    QList<OrderKey> playableOrder;
    playableOrder.reserve(m_importItems.size());
    for (int i = 0; i < m_importItems.size(); ++i) {
        const ImportItem &entry = m_importItems.at(i);
        if (!entry.isPlayable || !entry.queueMetadata.value(QStringLiteral("includeInNextRun"), true).toBool()) {
            continue;
        }
        const int sourceQueuePosition =
            sourceById(entry.sourceId)
                .value(QStringLiteral("queueMetadata"))
                .toMap()
                .value(QStringLiteral("queuePosition"))
                .toInt();
        playableOrder.push_back(
            {sourceQueuePosition,
             i,
             entry.playlistIndex >= 0 ? entry.playlistIndex : entry.metadataOrder,
             entry.playlistIndex >= 0});
    }

    std::sort(playableOrder.begin(), playableOrder.end(), [](const OrderKey &a, const OrderKey &b) {
        if (a.sourceQueuePosition != b.sourceQueuePosition) {
            return a.sourceQueuePosition < b.sourceQueuePosition;
        }
        if (a.hasPlaylistIndex != b.hasPlaylistIndex) {
            return a.hasPlaylistIndex && !b.hasPlaylistIndex;
        }
        if (a.rank != b.rank) {
            return a.rank < b.rank;
        }
        return a.itemIndex < b.itemIndex;
    });

    QHash<int, int> resultTrackIndices;
    for (int i = 0; i < playableOrder.size(); ++i) {
        resultTrackIndices.insert(playableOrder.at(i).itemIndex, i);
    }

    const FormatArgumentPlan formatPlan = formatArgumentPlanForSelection(m_selectedFormat);
    const QString rootStagingDirectory = stagingRootDirectory(m_outputDirectory);
    QSet<QString> reservedPaths;
    QSet<QString> existingPaths;
    for (int i = 0; i < m_importItems.size(); ++i) {
        ImportItem &item = m_importItems[i];
        const bool includeInNextRun = item.queueMetadata.value(QStringLiteral("includeInNextRun"), true).toBool();
        if (!includeInNextRun) {
            continue;
        }
        cleanupItemArtifacts(item);
        item.state = Pending;
        item.progress = 0.0;
        item.statusText.clear();
        item.errorText.clear();
        item.finalOutputFile.clear();
        item.failureType = NoFailure;
        item.retryEligibility = item.isPlayable ? RetryNotApplicable : RetryBlocked;
        item.queueMetadata.insert(QStringLiteral("runSerial"), m_activeRunSerial);
        item.resultTrackInsertIndex = resultTrackIndices.value(i, -1);
        ProbeEntry entry;
        entry.sourceUrl = item.sourceUrl;
        entry.extractor = item.extractor;
        entry.title = item.title;
        entry.entryId = item.entrySourceId;
        entry.playlistIndex = item.playlistIndex;
        entry.duration = item.duration;
        entry.thumbnail = item.thumbnail;
        entry.webpageUrl = item.webpageUrl;
        entry.availability = item.availability;
        entry.isPlayable = item.isPlayable;
        entry.metadataOrder = item.metadataOrder;
        const QVariantMap sourceMetadata = sourceById(item.sourceId).value(QStringLiteral("metadataSnapshot")).toMap();
        const bool isPlaylist = sourceMetadata.value(QStringLiteral("isPlaylist")).toBool()
            || sourceMetadata.value(QStringLiteral("entryCount")).toInt() > 1;
        const NamingPlan namingPlan = buildNamingPlanForEntry(
            entry, sourceMetadata, m_namingPolicy, formatPlan.outputExtension, isPlaylist);
        const QString requestedOutputFile =
            QDir(m_outputDirectory).filePath(namingPlan.fileName);
        const bool targetExists = QFileInfo::exists(requestedOutputFile) || existingPaths.contains(requestedOutputFile);
        const bool queueConflict = reservedPaths.contains(requestedOutputFile);
        const bool hasConflict = targetExists || queueConflict;
        item.conflictResolution.requestedOutputFile = requestedOutputFile;
        item.conflictResolution.resolvedOutputFile = requestedOutputFile;
        item.conflictResolution.hadConflict = hasConflict;
        item.conflictResolution.targetExistsOnDisk = targetExists;
        item.conflictResolution.collisionRuleKey = queueConflict
            ? QStringLiteral("queue-conflict")
            : targetExists ? QStringLiteral("existing-target")
                           : QStringLiteral("none");
        QString resolutionKey = QStringLiteral("planned");
        QString finalizationStrategyKey = QStringLiteral("temp-commit");
        bool conflictBlockedSkip = false;
        bool conflictBlockedFail = false;
        if (hasConflict) {
            if (m_conflictPolicy == QStringLiteral("skip-on-conflict")) {
                conflictBlockedSkip = true;
                resolutionKey = QStringLiteral("skip-on-conflict");
                finalizationStrategyKey = QStringLiteral("not-started");
                item.plannedOutputFile = requestedOutputFile;
            } else if (m_conflictPolicy == QStringLiteral("fail-on-conflict")) {
                conflictBlockedFail = true;
                resolutionKey = QStringLiteral("fail-on-conflict");
                finalizationStrategyKey = QStringLiteral("not-started");
                item.plannedOutputFile = requestedOutputFile;
            } else if (m_conflictPolicy == QStringLiteral("overwrite-if-allowed") && !queueConflict) {
                resolutionKey = QStringLiteral("overwrite-existing");
                finalizationStrategyKey = QStringLiteral("temp-replace");
                item.plannedOutputFile = requestedOutputFile;
            } else {
                item.plannedOutputFile = uniqueOutputPath(requestedOutputFile, reservedPaths, existingPaths);
                resolutionKey = item.plannedOutputFile == requestedOutputFile
                    ? QStringLiteral("planned")
                    : QStringLiteral("auto-renamed");
            }
        } else {
            item.plannedOutputFile = requestedOutputFile;
        }
        item.conflictResolution.resolvedOutputFile = item.plannedOutputFile;
        item.conflictResolution.hadConflict = hasConflict;
        item.conflictResolution.resolutionKey = resolutionKey;
        item.conflictResolution.finalizationStrategyKey = finalizationStrategyKey;
        item.stagingDirectory = QDir(rootStagingDirectory).filePath(item.entryId);
        item.stagingOutputFile = QDir(item.stagingDirectory).filePath(
            QStringLiteral("payload.%1").arg(formatPlan.outputExtension));
        item.previewDiagnostics = {
            {QStringLiteral("requestedNamingPolicy"), m_namingPolicy},
            {QStringLiteral("appliedNamingPolicy"), namingPlan.appliedPolicy},
            {QStringLiteral("baseName"), QFileInfo(item.plannedOutputFile).completeBaseName()},
            {QStringLiteral("requestedOutputFile"), requestedOutputFile},
            {QStringLiteral("resolvedOutputFile"), item.plannedOutputFile},
            {QStringLiteral("sourceTitle"), namingPlan.sourceTitle},
            {QStringLiteral("entryTitle"), namingPlan.entryTitle},
            {QStringLiteral("sourceDirectoryPolicy"), QStringLiteral("explicit-output-directory")},
            {QStringLiteral("formatPlan"), formatPlan.toVariantMap()},
            {QStringLiteral("missingMetadataFields"),
             QVariantList(namingPlan.missingMetadataFields.begin(), namingPlan.missingMetadataFields.end())},
            {QStringLiteral("namingFallbackReasonKey"), namingPlan.fallbackReasonKey},
            {QStringLiteral("conflictPolicy"), m_conflictPolicy},
            {QStringLiteral("collisionScope"), queueConflict
                 ? QStringLiteral("in-job")
                 : targetExists ? QStringLiteral("existing-target")
                                : QStringLiteral("none")},
            {QStringLiteral("collisionRuleKey"), item.conflictResolution.collisionRuleKey},
            {QStringLiteral("resolutionKey"), item.conflictResolution.resolutionKey},
            {QStringLiteral("targetExistsOnDisk"), item.conflictResolution.targetExistsOnDisk},
            {QStringLiteral("finalizationStrategyKey"),
             item.conflictResolution.finalizationStrategyKey}
        };

        if (!entry.isPlayable) {
            item.state = Skipped;
            item.progress = 1.0;
            item.errorText = localizedText(QStringLiteral("ytDlpImport.itemSkippedUnavailable"));
            item.statusText = item.errorText;
            item.failureType = ContentFailure;
            item.retryEligibility = RetryBlocked;
        } else if (conflictBlockedSkip) {
            item.state = Skipped;
            item.progress = 1.0;
            item.errorText = localizedText(QStringLiteral("batchAudioConverter.previewCollisionSkipConflict"));
            item.statusText = item.errorText;
            item.failureType = OutputFailure;
            item.retryEligibility = RetryAllowed;
        } else if (conflictBlockedFail) {
            item.state = Failed;
            item.progress = 1.0;
            item.errorText = localizedText(QStringLiteral("batchAudioConverter.previewCollisionFailConflict"));
            item.statusText = item.errorText;
            item.failureType = OutputFailure;
            item.retryEligibility = RetryAllowed;
        } else {
            reservedPaths.insert(item.plannedOutputFile);
            existingPaths.insert(requestedOutputFile);
        }
        item.updatedAtMs = nowMs();
        syncEntryRuntimeLayers(item);
    }

    const bool hasPlannedItems = std::any_of(m_importItems.cbegin(),
                                             m_importItems.cend(),
                                             [this](const ImportItem &item) {
                                                 return item.isPlayable
                                                     && item.queueMetadata.value(QStringLiteral("includeInNextRun"), true).toBool()
                                                     && item.queueMetadata.value(QStringLiteral("runSerial")).toInt() == m_activeRunSerial;
                                             });
    const bool hasRunnableItems = std::any_of(m_importItems.cbegin(),
                                              m_importItems.cend(),
                                              [this](const ImportItem &item) {
                                                  return item.state == Pending
                                                      && item.isPlayable
                                                      && item.queueMetadata.value(QStringLiteral("includeInNextRun"), true).toBool()
                                                      && item.queueMetadata.value(QStringLiteral("runSerial")).toInt() == m_activeRunSerial;
                                              });
    if (!hasPlannedItems) {
        const QString message = localizedText(QStringLiteral("ytDlpImport.importNoPlayableItems"));
        setStatusText(message);
        setLastError(message);
        return false;
    }

    m_queuePrepared = true;
    publishItems(true);
    updateBatchProgress();
    syncSourceRuntimeFromItems();
    return true;
}

bool YtDlpImportService::startNextImportItem()
{
    if (!m_appSettingsManager) {
        const QString message = localizedText(QStringLiteral("ytDlpImport.probeRuntimeUnavailable"));
        setLastError(message);
        setStatusText(message);
        return false;
    }

    const QVariantMap executableInspection = m_appSettingsManager->inspectYtDlpExecutable();
    if (!executableInspection.value(QStringLiteral("ok")).toBool()) {
        const QString message = executableInspection.value(QStringLiteral("message")).toString().trimmed();
        setLastError(message);
        setStatusText(message);
        return false;
    }

    const int nextIndex = nextPendingItemIndexForRun();
    if (nextIndex < 0) {
        return false;
    }

    ImportItem &item = m_importItems[nextIndex];
    const QString itemId = item.entryId;
    item.state = Running;
    item.progress = 0.0;
    item.errorText.clear();
    item.statusText = localizedText(QStringLiteral("ytDlpImport.itemStarting"));
    item.updatedAtMs = nowMs();
    cleanupItemArtifacts(item);
    if (!QDir().mkpath(item.stagingDirectory)) {
        const QString message =
            localizedText(QStringLiteral("ytDlpImport.importInvalidOutputDirectory"))
                .arg(item.stagingDirectory);
        finishImportWithError(itemId, message);
        scheduleImportWorkers();
        return false;
    }
    syncEntryRuntimeLayers(item);
    publishItems();
    updateBatchProgress();
    syncSourceRuntimeFromItems();
    updateImportRuntimeStatusText();

    auto *process = new QProcess(this);
    const FormatArgumentPlan formatPlan = formatArgumentPlanFromVariantMap(
        item.previewDiagnostics.value(QStringLiteral("formatPlan")).toMap(),
        m_selectedFormat);
    const QString stagingOutputTemplate = !item.stagingOutputFile.isEmpty()
        ? outputTemplateForFile(item.stagingOutputFile)
        : stagingOutputTemplateForItem(m_outputDirectory, item.entryId);
    QStringList arguments{QStringLiteral("--extract-audio"),
                          QStringLiteral("--audio-format"),
                          formatPlan.audioFormatArgument};
    arguments.append(formatPlan.extraArguments);
    arguments.append({QStringLiteral("--newline"),
                      QStringLiteral("--no-warnings"),
                      QStringLiteral("--no-overwrites"),
                      QStringLiteral("--output"),
                      stagingOutputTemplate,
                      QStringLiteral("--"),
                      item.sourceUrl});
    process->setProgram(executableInspection.value(QStringLiteral("resolvedPath")).toString());
    process->setArguments(arguments);

    registerActiveImportProcess(itemId, nextIndex, process);

    connect(process, &QProcess::readyReadStandardOutput, this, [this, process, itemId]() {
        processImportOutputChunk(itemId, process->readAllStandardOutput(), false);
    });
    connect(process, &QProcess::readyReadStandardError, this, [this, process, itemId]() {
        const QByteArray stderrChunk = process->readAllStandardError();
        m_importStderrBuffers[itemId].append(stderrChunk);
        processImportOutputChunk(itemId, stderrChunk, true);
    });
    connect(process,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            [this, process, itemId](int exitCode, QProcess::ExitStatus exitStatus) {
                processImportOutputChunk(itemId, process->readAllStandardOutput(), false);
                const QByteArray stderrChunk = process->readAllStandardError();
                m_importStderrBuffers[itemId].append(stderrChunk);
                processImportOutputChunk(itemId, stderrChunk, true);

                const QString stderrText = QString::fromLocal8Bit(m_importStderrBuffers.value(itemId));
                finalizeImportProcess(process);
                const int currentItemIndex = indexOfItemId(itemId);

                if (m_cancelRequested) {
                    if (currentItemIndex >= 0 && currentItemIndex < m_importItems.size()) {
                        ImportItem &currentItem = m_importItems[currentItemIndex];
                        currentItem.state = Canceled;
                        currentItem.progress = 1.0;
                        currentItem.statusText =
                            localizedText(QStringLiteral("ytDlpImport.itemCanceled"));
                        currentItem.errorText.clear();
                        currentItem.updatedAtMs = nowMs();
                        currentItem.failureType = CanceledFailure;
                        currentItem.retryEligibility = RetryAllowed;
                        cleanupItemArtifacts(currentItem);
                        syncEntryRuntimeLayers(currentItem);
                        publishItems();
                    }
                    m_cancelPendingItemIds.remove(itemId);
                    if (!m_cancelPendingItemIds.isEmpty()) {
                        updateBatchProgress();
                        syncSourceRuntimeFromItems();
                        updateImportRuntimeStatusText();
                        return;
                    }
                    scheduleImportWorkers();
                    return;
                }

                if (currentItemIndex < 0 || currentItemIndex >= m_importItems.size()) {
                    scheduleImportWorkers();
                    return;
                }

                ImportItem &currentItem = m_importItems[currentItemIndex];
                const QString expectedOutput = currentItem.stagingOutputFile;
                const bool normalSuccess = exitStatus == QProcess::NormalExit && exitCode == 0;
                const bool outputExists = !expectedOutput.isEmpty() && QFileInfo::exists(expectedOutput);
                if (!normalSuccess || !outputExists) {
                    const QString details = bestYtDlpDiagnosticLine(stderrText);
                    if (!stderrText.trimmed().isEmpty()) {
                        qWarning().noquote() << "yt-dlp import stderr:" << stderrText.trimmed();
                    }
                    const QString message = normalSuccess
                        ? localizedText(QStringLiteral("ytDlpImport.itemMissingOutput"))
                              .arg(expectedOutput)
                        : localizedText(QStringLiteral("ytDlpImport.importProcessFailed"))
                              .arg(details.isEmpty()
                                       ? QStringLiteral("yt-dlp exited with an error.")
                                       : details);
                    currentItem.state = Failed;
                    currentItem.progress = 1.0;
                    currentItem.errorText = message;
                    currentItem.statusText = message;
                    currentItem.updatedAtMs = nowMs();
                    currentItem.failureType =
                        failureTypeFromCategoryKey(normalizedErrorCategoryKey(message));
                    currentItem.retryEligibility =
                        retryEligibilityForFailureType(currentItem.failureType, currentItem.isPlayable);
                    cleanupItemArtifacts(currentItem);
                    syncEntryRuntimeLayers(currentItem);
                    publishItems();
                    setLastError(message);
                    updateBatchProgress();
                    syncSourceRuntimeFromItems();
                    scheduleImportWorkers();
                    return;
                }

                if (!finalizeSuccessfulImport(currentItem)) {
                    const QString message = localizedText(QStringLiteral("ytDlpImport.itemMissingOutput"))
                                                .arg(currentItem.plannedOutputFile);
                    currentItem.state = Failed;
                    currentItem.progress = 1.0;
                    currentItem.errorText = message;
                    currentItem.statusText = message;
                    currentItem.updatedAtMs = nowMs();
                    currentItem.failureType = OutputFailure;
                    currentItem.retryEligibility = RetryAllowed;
                    cleanupItemArtifacts(currentItem);
                    syncEntryRuntimeLayers(currentItem);
                    publishItems();
                    setLastError(message);
                    updateBatchProgress();
                    syncSourceRuntimeFromItems();
                    scheduleImportWorkers();
                    return;
                }

                currentItem.state = Succeeded;
                currentItem.progress = 1.0;
                currentItem.statusText = localizedText(QStringLiteral("ytDlpImport.itemFinished"));
                currentItem.errorText.clear();
                currentItem.updatedAtMs = nowMs();
                currentItem.failureType = NoFailure;
                currentItem.retryEligibility = RetryNotApplicable;
                currentItem.persistenceEligibility = ReportPersistable;
                syncEntryRuntimeLayers(currentItem);
                publishItems();
                updateBatchProgress();
                syncSourceRuntimeFromItems();
                scheduleImportWorkers();
            });

    process->start();
    if (!process->waitForStarted(kImportStartTimeoutMs)) {
        finalizeImportProcess(process);
        const QString message = localizedText(QStringLiteral("ytDlpImport.importFailedStart"))
                                    .arg(process->errorString().trimmed());
        finishImportWithError(itemId, message);
        scheduleImportWorkers();
        return false;
    }

    return true;
}

void YtDlpImportService::requestImportProcessCancellation(const QString &itemId, QProcess *process)
{
    if (itemId.trimmed().isEmpty() || !process) {
        return;
    }

    if (process->state() == QProcess::NotRunning) {
        return;
    }

    process->terminate();
    const QPointer<QProcess> guardedProcess(process);
    QTimer::singleShot(kImportCancelTerminateTimeoutMs, this, [this, itemId, guardedProcess]() {
        if (!guardedProcess || itemId.trimmed().isEmpty()) {
            return;
        }
        if (!m_cancelPendingItemIds.contains(itemId)) {
            return;
        }
        if (m_activeImportProcesses.value(itemId) != guardedProcess) {
            return;
        }
        if (guardedProcess->state() == QProcess::NotRunning) {
            return;
        }
        guardedProcess->kill();
    });
}

void YtDlpImportService::markPendingItemsCanceled(int runSerial)
{
    bool changed = false;
    const int targetRunSerial = runSerial > 0 ? runSerial : m_activeRunSerial;
    for (ImportItem &item : m_importItems) {
        const bool belongsToTargetRun =
            item.queueMetadata.value(QStringLiteral("runSerial")).toInt() == targetRunSerial;
        if (item.state != Pending || !belongsToTargetRun) {
            continue;
        }

        item.state = Canceled;
        item.progress = 1.0;
        item.statusText = localizedText(QStringLiteral("ytDlpImport.itemCanceled"));
        item.updatedAtMs = nowMs();
        item.failureType = CanceledFailure;
        item.retryEligibility = RetryAllowed;
        syncEntryRuntimeLayers(item);
        changed = true;
    }

    if (changed) {
        publishItems();
        syncSourceRuntimeFromItems();
    }
}

void YtDlpImportService::updateBatchProgress()
{
    bool hasActiveRunItems = false;
    double progressUnits = 0.0;
    int denominator = 0;
    for (const ImportItem &item : std::as_const(m_importItems)) {
        if (item.queueMetadata.value(QStringLiteral("runSerial")).toInt() != m_activeRunSerial) {
            continue;
        }
        hasActiveRunItems = true;
        ++denominator;
        if (item.state == Running) {
            progressUnits += qBound(0.0, item.progress, 1.0);
            continue;
        }

        if (isTerminalState(item.state)) {
            progressUnits += 1.0;
        }
    }

    if (!hasActiveRunItems || denominator <= 0) {
        setBatchProgress(0.0);
        return;
    }
    setBatchProgress(progressUnits / static_cast<double>(denominator));
}

void YtDlpImportService::updateImportRuntimeStatusText()
{
    if (!m_isRunning) {
        return;
    }

    QStringList activeItemIds;
    activeItemIds.reserve(m_activeImportProcesses.size());
    for (auto it = m_activeImportProcesses.cbegin(); it != m_activeImportProcesses.cend(); ++it) {
        if (it.value()) {
            activeItemIds.push_back(it.key());
        }
    }

    if (activeItemIds.isEmpty()) {
        if (m_cancelRequested) {
            return;
        }
        setStatusText(localizedText(QStringLiteral("ytDlpImport.importStarted")));
        return;
    }

    if (activeItemIds.size() == 1) {
        const QVariantMap itemMap = itemById(activeItemIds.constFirst());
        const QString itemStatus = itemMap.value(QStringLiteral("statusText")).toString().trimmed();
        setStatusText(itemStatus.isEmpty()
                          ? localizedText(QStringLiteral("ytDlpImport.itemStarting"))
                          : itemStatus);
        return;
    }

    setStatusText(localizedText(QStringLiteral("ytDlpImport.importRunningActiveCount"))
                      .arg(activeItemIds.size()));
}

void YtDlpImportService::setBatchProgress(double batchProgress)
{
    const double normalized = qBound(0.0, batchProgress, 1.0);
    if (qFuzzyCompare(m_batchProgress, normalized)) {
        return;
    }

    m_batchProgress = normalized;
    emit batchProgressChanged();
}

void YtDlpImportService::setCancelRequested(bool cancelRequested)
{
    if (m_cancelRequested == cancelRequested) {
        return;
    }

    m_cancelRequested = cancelRequested;
    emit cancelRequestedChanged();
}

void YtDlpImportService::setIsRunning(bool isRunning)
{
    if (m_isRunning == isRunning) {
        return;
    }

    m_isRunning = isRunning;
    emit isRunningChanged();
}

void YtDlpImportService::setFinalSummary(const QVariantMap &summary)
{
    if (m_finalSummary == summary) {
        return;
    }

    m_finalSummary = summary;
    if (!summary.isEmpty()) {
        archiveCompletedReport(summary);
    }
    emit finalSummaryChanged();
    persistDraftState();
}

void YtDlpImportService::processImportOutputChunk(const QString &itemId,
                                                  const QByteArray &chunk,
                                                  bool isStdErr)
{
    const int itemIndex = activeImportItemIndex(itemId);
    if (itemIndex < 0 || itemIndex >= m_importItems.size()) {
        return;
    }

    QByteArray &lineBuffer = isStdErr
        ? m_importStderrLineBuffers[itemId]
        : m_importStdoutBuffers[itemId];
    const QStringList lines = readLinesFromBuffer(&lineBuffer, chunk);
    static const QRegularExpression percentPattern(QStringLiteral(R"((\d+(?:\.\d+)?)%)"));
    bool changed = false;
    for (const QString &line : lines) {
        if (line.isEmpty()) {
            continue;
        }

        ImportItem &item = m_importItems[itemIndex];
        item.statusText = line;

        const QRegularExpressionMatch match = percentPattern.match(line);
        if (match.hasMatch()) {
            bool ok = false;
            const double percent = match.captured(1).toDouble(&ok);
            if (ok) {
                item.progress = qBound(0.0, percent / 100.0, 1.0);
                item.updatedAtMs = nowMs();
                syncEntryRuntimeLayers(item);
                updateBatchProgress();
            }
        }
        changed = true;
    }

    if (changed) {
        publishItems();
        syncSourceRuntimeFromItems();
        updateImportRuntimeStatusText();
    }
}

void YtDlpImportService::setStatusText(const QString &statusText)
{
    if (m_statusText == statusText) {
        return;
    }

    m_statusText = statusText;
    emit statusTextChanged();
}

void YtDlpImportService::setLastError(const QString &lastError)
{
    if (m_lastError == lastError) {
        return;
    }

    m_lastError = lastError;
    emit lastErrorChanged();
}

void YtDlpImportService::finishProbeWithError(const QString &message)
{
    clearProbeResult();
    const int sourceIndex = activeProbeSourceIndex();
    if (sourceIndex >= 0) {
        completeProbeFailure(sourceIndex, message);
    }
    setStatusText(message);
    setLastError(message);
}

void YtDlpImportService::finalizeProcess(QProcess *process)
{
    if (m_probeProcess == process) {
        m_probeProcess = nullptr;
    }
    if (m_isProbing) {
        m_isProbing = false;
        emit isProbingChanged();
    }
    if (process) {
        process->deleteLater();
    }
}

void YtDlpImportService::resetJobLifecycle()
{
    m_importJob = ImportJob();
    m_importJob.jobId = newIdentity();
    m_importJob.createdAtMs = nowMs();
    m_importJob.persistenceEligibility = DraftPersistable;
    m_importJob.defaultsSnapshot = {
        {QStringLiteral("outputDirectory"), m_outputDirectory},
        {QStringLiteral("selectedFormat"), m_selectedFormat},
        {QStringLiteral("namingPolicy"), m_namingPolicy},
        {QStringLiteral("conflictPolicy"), m_conflictPolicy},
        {QStringLiteral("parallelDownloads"), m_parallelDownloads}
    };
}

void YtDlpImportService::refreshSourceQueuePositions()
{
    for (int i = 0; i < m_importSources.size(); ++i) {
        QVariantMap queueMetadata = m_importSources[i].queueMetadata;
        queueMetadata.insert(QStringLiteral("queuePosition"), i);
        m_importSources[i].queueMetadata = queueMetadata;
    }
}

void YtDlpImportService::invalidateSingleSourceProbeView()
{
    clearProbeResult();
    if (!m_isRunning && m_importJob.startedAtMs <= 0 && m_finalSummary.isEmpty()) {
        rebuildPreviewItemsFromSources();
    }
}

bool YtDlpImportService::enqueueProbeSources(const QStringList &sourceIds)
{
    if (m_isProbing) {
        setLastError(localizedText(QStringLiteral("ytDlpImport.probeAlreadyRunning")));
        return false;
    }

    if (m_isRunning) {
        setLastError(localizedText(QStringLiteral("ytDlpImport.importAlreadyRunning")));
        return false;
    }

    if (m_importJob.startedAtMs > 0 || !m_finalSummary.isEmpty()) {
        setLastError(localizedText(QStringLiteral("ytDlpImport.importAlreadyRunning")));
        return false;
    }

    if (!m_appSettingsManager) {
        setLastError(localizedText(QStringLiteral("ytDlpImport.probeRuntimeUnavailable")));
        return false;
    }

    const QVariantMap executableInspection = m_appSettingsManager->inspectYtDlpExecutable();
    if (!executableInspection.value(QStringLiteral("ok")).toBool()) {
        setLastError(executableInspection.value(QStringLiteral("message")).toString());
        return false;
    }

    QStringList queue;
    QSet<QString> seen;
    for (const QString &sourceId : sourceIds) {
        const int index = indexOfSourceId(sourceId);
        if (index < 0 || seen.contains(sourceId)) {
            continue;
        }
        queue.push_back(sourceId);
        seen.insert(sourceId);
    }

    if (queue.isEmpty()) {
        setLastError(localizedText(QStringLiteral("ytDlpImport.importRequiresProbe")));
        return false;
    }

    m_probeQueueSourceIds = queue;
    setCancelRequested(false);
    setLastError(QString());
    return startNextProbeSource();
}

bool YtDlpImportService::startNextProbeSource()
{
    if (m_probeQueueSourceIds.isEmpty()) {
        setStatusText(QString());
        return true;
    }

    const QString nextSourceId = m_probeQueueSourceIds.takeFirst();
    const int sourceIndex = indexOfSourceId(nextSourceId);
    if (sourceIndex < 0) {
        return startNextProbeSource();
    }
    return startProbeProcessForSource(sourceIndex);
}

bool YtDlpImportService::startProbeProcessForSource(int sourceIndex)
{
    if (sourceIndex < 0 || sourceIndex >= m_importSources.size()) {
        return false;
    }

    ImportSource &source = m_importSources[sourceIndex];
    const QString normalizedUrl =
        source.immutableSourceInput.value(QStringLiteral("normalizedUrl")).toString().trimmed();
    if (normalizedUrl.isEmpty()) {
        completeProbeFailure(sourceIndex, localizedText(QStringLiteral("ytDlpImport.probeInvalidUrl")));
        return startNextProbeSource();
    }

    source.status = SourceProbing;
    source.runtimeState.insert(QStringLiteral("isStale"), false);
    source.runtimeState.insert(QStringLiteral("probeFormatMismatch"), false);
    m_currentProbeSourceId = source.sourceId;
    setSourceUrl(normalizedUrl);
    publishSources();
    setStatusText(localizedText(QStringLiteral("ytDlpImport.probeStarted")));

    const QVariantMap executableInspection = m_appSettingsManager->inspectYtDlpExecutable();
    auto *process = new QProcess(this);
    process->setProgram(executableInspection.value(QStringLiteral("resolvedPath")).toString());
    process->setArguments(probeArgumentsForSourceUrl(normalizedUrl));

    m_probeStderrBuffer.clear();
    m_probeStderrLogBuffer.clear();
    m_probeProcess = process;
    m_isProbing = true;
    emit isProbingChanged();

    connect(process, &QProcess::readyReadStandardError, this, [this, process]() {
        const QByteArray stderrChunk = process->readAllStandardError();
        m_probeStderrLogBuffer.append(stderrChunk);
        const QStringList lines =
            readLinesFromBuffer(&m_probeStderrBuffer, stderrChunk);
        for (const QString &line : lines) {
            if (!line.isEmpty()) {
                setStatusText(line);
            }
        }
    });
    connect(process,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            [this, process, normalizedUrl](int exitCode, QProcess::ExitStatus exitStatus) {
                const QByteArray stdoutBytes = process->readAllStandardOutput();
                const QByteArray stderrChunk = process->readAllStandardError();
                m_probeStderrBuffer.append(stderrChunk);
                m_probeStderrLogBuffer.append(stderrChunk);
                const QString stderrText = QString::fromLocal8Bit(m_probeStderrLogBuffer);
                const int sourceIndex = activeProbeSourceIndex();

                finalizeProcess(process);

                if (m_cancelRequested && !m_isRunning) {
                    setCancelRequested(false);
                    setStatusText(localizedText(QStringLiteral("ytDlpImport.probeCanceled")));
                    setLastError(QString());
                    if (sourceIndex >= 0 && sourceIndex < m_importSources.size()) {
                        ImportSource &source = m_importSources[sourceIndex];
                        if (source.lastProbedAtMs > 0) {
                            source.status =
                                source.metadataSnapshot.value(QStringLiteral("hasUnavailableEntries")).toBool()
                                    ? SourceReadyWithIssues
                                    : SourceReady;
                        } else {
                            source.status = SourcePendingProbe;
                        }
                        publishSources();
                    }
                    m_probeQueueSourceIds.clear();
                    m_currentProbeSourceId.clear();
                    return;
                }

                if (sourceIndex < 0 || sourceIndex >= m_importSources.size()) {
                    m_currentProbeSourceId.clear();
                    startNextProbeSource();
                    return;
                }

                if (exitStatus != QProcess::NormalExit || exitCode != 0) {
                    const QString details = bestYtDlpDiagnosticLine(stderrText);
                    if (!stderrText.trimmed().isEmpty()) {
                        qWarning().noquote() << "yt-dlp probe stderr:" << stderrText.trimmed();
                    }
                    completeProbeFailure(
                        sourceIndex,
                        localizedText(QStringLiteral("ytDlpImport.probeFailedProcess"))
                            .arg(details.isEmpty() ? QStringLiteral("yt-dlp exited with an error.")
                                                   : details));
                    m_currentProbeSourceId.clear();
                    startNextProbeSource();
                    return;
                }

                ProbeResult result;
                QString parseError;
                if (!parseProbeJson(stdoutBytes, normalizedUrl, &result, &parseError)) {
                    completeProbeFailure(sourceIndex, parseError);
                    m_currentProbeSourceId.clear();
                    startNextProbeSource();
                    return;
                }

                completeProbeSuccess(sourceIndex, result);
                m_currentProbeSourceId.clear();
                startNextProbeSource();
            });

    process->start();
    if (!process->waitForStarted(kProbeStartTimeoutMs)) {
        finalizeProcess(process);
        completeProbeFailure(
            sourceIndex,
            localizedText(QStringLiteral("ytDlpImport.probeFailedStart"))
                .arg(process->errorString().trimmed()));
        m_currentProbeSourceId.clear();
        return startNextProbeSource();
    }

    return true;
}

void YtDlpImportService::completeProbeSuccess(int sourceIndex, const ProbeResult &result)
{
    if (sourceIndex < 0 || sourceIndex >= m_importSources.size()) {
        return;
    }

    const qint64 materializedAtMs = nowMs();
    ImportSource &source = m_importSources[sourceIndex];
    source.lastProbedAtMs = materializedAtMs;
    source.status = result.hasUnavailableEntries ? SourceReadyWithIssues : SourceReady;
    source.failureType = NoFailure;
    source.retryEligibility = RetryNotApplicable;
    source.persistenceEligibility = DraftPersistable;
    source.metadataSnapshot = probeResultToVariantMap(result);
    source.runtimeState = {
        {QStringLiteral("entryCount"), result.entries.size()},
        {QStringLiteral("availableEntryCount"),
         source.metadataSnapshot.value(QStringLiteral("availableEntryCount")).toInt()},
        {QStringLiteral("unavailableEntryCount"),
         source.metadataSnapshot.value(QStringLiteral("unavailableEntryCount")).toInt()},
        {QStringLiteral("isStale"), false},
        {QStringLiteral("probeFormatSnapshot"), m_selectedFormat},
        {QStringLiteral("probeFormatMismatch"), false},
        {QStringLiteral("metadataTimestampMs"), materializedAtMs},
        {QStringLiteral("resolvedCanonicalUrl"), result.resolvedSourceUrl}
    };
    source.finalResultState = {
        {QStringLiteral("message"), QString()},
        {QStringLiteral("lastFailureAtMs"), static_cast<qint64>(0)}
    };

    m_probeResultData = result;
    m_probeResult = probeResultToVariantMap(result);
    m_hasProbeResult = true;
    rebuildPreviewItemsFromSources();
    persistRecentCanonicalSourceUrl(result.resolvedSourceUrl);
    publishSources();
    emit probeResultChanged();
    setLastError(QString());
    setStatusText(result.isPlaylist
                      ? localizedText(QStringLiteral("ytDlpImport.probeReadyPlaylist"))
                            .arg(result.entries.size())
                      : localizedText(QStringLiteral("ytDlpImport.probeReadySingle")));
}

void YtDlpImportService::completeProbeFailure(int sourceIndex, const QString &message)
{
    if (sourceIndex < 0 || sourceIndex >= m_importSources.size()) {
        return;
    }

    ImportSource &source = m_importSources[sourceIndex];
    source.lastProbedAtMs = nowMs();
    source.status = SourceProbeFailed;
    source.failureType = failureTypeFromCategoryKey(normalizedErrorCategoryKey(message));
    source.retryEligibility = retryEligibilityForFailureType(source.failureType);
    source.persistenceEligibility = DraftPersistable;
    source.runtimeState.insert(QStringLiteral("isStale"), false);
    source.runtimeState.insert(QStringLiteral("probeFormatSnapshot"), m_selectedFormat);
    source.runtimeState.insert(QStringLiteral("metadataTimestampMs"), source.lastProbedAtMs);
    source.finalResultState = {
        {QStringLiteral("message"), message},
        {QStringLiteral("lastFailureAtMs"), source.lastProbedAtMs}
    };
    rebuildPreviewItemsFromSources();
    publishSources();
    setStatusText(message);
    setLastError(message);
}

bool YtDlpImportService::sourceNeedsProbe(const ImportSource &source) const
{
    return source.status == SourcePendingProbe || source.status == SourceProbeFailed
        || source.runtimeState.value(QStringLiteral("isStale")).toBool();
}

bool YtDlpImportService::sourceNeedsRetryProbe(const ImportSource &source) const
{
    return source.status == SourceProbeFailed || source.runtimeState.value(QStringLiteral("isStale")).toBool();
}

int YtDlpImportService::activeProbeSourceIndex() const
{
    return indexOfSourceId(m_currentProbeSourceId);
}

void YtDlpImportService::rebuildPreviewItemsFromSources()
{
    if (m_isRunning || m_importJob.startedAtMs > 0 || !m_finalSummary.isEmpty()) {
        return;
    }

    QList<ImportItem> previewItems;
    for (const ImportSource &source : std::as_const(m_importSources)) {
        const QVariantList serializedEntries =
            source.metadataSnapshot.value(QStringLiteral("entries")).toList();
        for (const QVariant &value : serializedEntries) {
            const QVariantMap entryMap = value.toMap();
            ImportItem item;
            item.entryId = newIdentity();
            item.sourceId = source.sourceId;
            item.sourceUrl = entryMap.value(QStringLiteral("sourceUrl")).toString();
            item.extractor = entryMap.value(QStringLiteral("extractor")).toString();
            item.title = entryMap.value(QStringLiteral("title")).toString();
            item.entrySourceId = entryMap.value(QStringLiteral("entryId")).toString();
            item.duration = entryMap.value(QStringLiteral("duration")).toLongLong();
            item.thumbnail = entryMap.value(QStringLiteral("thumbnail")).toString();
            item.webpageUrl = entryMap.value(QStringLiteral("webpageUrl")).toString();
            item.availability = entryMap.value(QStringLiteral("availability")).toString();
            item.playlistIndex = entryMap.contains(QStringLiteral("playlistIndex"))
                ? entryMap.value(QStringLiteral("playlistIndex")).toInt()
                : -1;
            item.metadataOrder = entryMap.contains(QStringLiteral("metadataOrder"))
                ? entryMap.value(QStringLiteral("metadataOrder")).toInt()
                : -1;
            item.isPlayable = entryMap.value(QStringLiteral("isPlayable")).toBool();
            item.createdAtMs = source.lastProbedAtMs > 0 ? source.lastProbedAtMs : source.createdAtMs;
            item.updatedAtMs = item.createdAtMs;
            item.failureType = item.isPlayable ? NoFailure : ContentFailure;
            item.retryEligibility = item.isPlayable ? RetryNotApplicable : RetryBlocked;
            item.persistenceEligibility = DraftPersistable;
            item.immutableSourceInput = {
                {QStringLiteral("sourceUrl"), item.sourceUrl},
                {QStringLiteral("sourceId"), source.sourceId}
            };
            item.metadataSnapshot = entryMap;
            item.queueMetadata = {
                {QStringLiteral("includeInNextRun"), item.isPlayable},
                {QStringLiteral("queuePosition"), item.metadataOrder},
                {QStringLiteral("sourceQueuePosition"),
                 source.queueMetadata.value(QStringLiteral("queuePosition")).toInt()}
            };
            item.runtimeState = {
                {QStringLiteral("state"), itemStateKey(Pending)},
                {QStringLiteral("progress"), 0.0}
            };
            item.finalResultState = {
                {QStringLiteral("terminalResult"), QStringLiteral("none")},
                {QStringLiteral("message"), QString()},
                {QStringLiteral("failureType"), failureTypeKey(item.failureType)}
            };
            if (!item.isPlayable) {
                item.state = Skipped;
                item.progress = 1.0;
                item.errorText =
                    localizedText(QStringLiteral("ytDlpImport.itemSkippedUnavailable"));
                item.statusText = item.errorText;
                syncEntryRuntimeLayers(item);
            }
            previewItems.push_back(item);
        }
    }

    m_importItems = previewItems;
    m_queuePrepared = false;
    refreshPreviewOutputPlan();
    publishItems(true);
    updateBatchProgress();
}

QList<int> YtDlpImportService::itemIndexesForSource(const QString &sourceId) const
{
    QList<int> indexes;
    for (int i = 0; i < m_importItems.size(); ++i) {
        if (m_importItems.at(i).sourceId == sourceId) {
            indexes.push_back(i);
        }
    }
    return indexes;
}

bool YtDlpImportService::sourceHasRunningEntry(const QString &sourceId) const
{
    for (const ImportItem &item : m_importItems) {
        if (item.sourceId == sourceId && item.state == Running) {
            return true;
        }
    }
    return false;
}

bool YtDlpImportService::sourceHasOnlyPendingTailEntries(const QString &sourceId) const
{
    if (sourceHasRunningEntry(sourceId)) {
        return false;
    }

    const QList<int> indexes = itemIndexesForSource(sourceId);
    if (indexes.isEmpty()) {
        return true;
    }

    const int furthestActiveIndex = furthestActiveImportItemIndex();
    for (const int index : indexes) {
        if (furthestActiveIndex >= 0 && index <= furthestActiveIndex) {
            return false;
        }
        const ItemState state = m_importItems.at(index).state;
        if (!(state == Pending || state == Skipped)) {
            return false;
        }
    }
    return true;
}

QString YtDlpImportService::activeImportItemIdForProcess(const QProcess *process) const
{
    if (!process) {
        return {};
    }

    for (auto it = m_activeImportProcesses.cbegin(); it != m_activeImportProcesses.cend(); ++it) {
        if (it.value() == process) {
            return it.key();
        }
    }
    return {};
}

int YtDlpImportService::activeImportItemIndex(const QString &itemId) const
{
    if (itemId.trimmed().isEmpty()) {
        return -1;
    }

    const auto it = m_activeImportItemIndexes.constFind(itemId);
    if (it != m_activeImportItemIndexes.cend()) {
        return it.value();
    }
    return indexOfItemId(itemId);
}

int YtDlpImportService::furthestActiveImportItemIndex() const
{
    int furthestIndex = -1;
    for (auto it = m_activeImportItemIndexes.cbegin(); it != m_activeImportItemIndexes.cend(); ++it) {
        furthestIndex = qMax(furthestIndex, it.value());
    }
    return furthestIndex;
}

void YtDlpImportService::registerActiveImportProcess(const QString &itemId,
                                                     int itemIndex,
                                                     QProcess *process)
{
    if (itemId.trimmed().isEmpty()) {
        return;
    }

    m_activeImportProcesses.insert(itemId, process);
    m_activeImportItemIndexes.insert(itemId, itemIndex);
    m_importStdoutBuffers.insert(itemId, QByteArray());
    m_importStderrLineBuffers.insert(itemId, QByteArray());
    m_importStderrBuffers.insert(itemId, QByteArray());
}

void YtDlpImportService::unregisterActiveImportProcess(const QString &itemId)
{
    if (itemId.trimmed().isEmpty()) {
        return;
    }

    m_activeImportProcesses.remove(itemId);
    m_activeImportItemIndexes.remove(itemId);
    m_importStdoutBuffers.remove(itemId);
    m_importStderrLineBuffers.remove(itemId);
    m_importStderrBuffers.remove(itemId);
}

void YtDlpImportService::archiveItemAttempt(ImportItem &item, const QString &reasonKey)
{
    QVariantList attempts = item.finalResultState.value(QStringLiteral("attemptHistory")).toList();
    QVariantMap attempt;
    attempt.insert(QStringLiteral("reasonKey"), reasonKey);
    attempt.insert(QStringLiteral("state"), itemStateKey(item.state));
    attempt.insert(QStringLiteral("failureType"), failureTypeKey(item.failureType));
    attempt.insert(QStringLiteral("message"),
                   !item.errorText.trimmed().isEmpty() ? item.errorText : item.statusText);
    attempt.insert(QStringLiteral("finalOutputFile"), item.finalOutputFile);
    attempt.insert(QStringLiteral("plannedOutputFile"), item.plannedOutputFile);
    attempt.insert(QStringLiteral("retryCount"), item.retryCount);
    attempt.insert(QStringLiteral("archivedAtMs"), nowMs());
    attempts.push_back(attempt);
    item.finalResultState.insert(QStringLiteral("attemptHistory"), attempts);
}

QVariantMap YtDlpImportService::applySourceIntake(const QStringList &rawInputs,
                                                  bool replaceExistingSources)
{
    QVariantMap result;
    result.insert(QStringLiteral("ok"), true);
    result.insert(QStringLiteral("operationKey"),
                  replaceExistingSources ? QStringLiteral("replace") : QStringLiteral("append"));
    result.insert(QStringLiteral("dedupPolicy"), QStringLiteral("drop-duplicate-sources"));
    result.insert(QStringLiteral("canonicalDedupPolicy"),
                  QStringLiteral("drop-if-known-canonical-source-already-exists"));
    result.insert(QStringLiteral("overlappingEntryPolicy"),
                  QStringLiteral("allow-distinct-sources; no-entry-level-dedup-at-intake"));

    if (m_isProbing) {
        const QString message = localizedText(QStringLiteral("ytDlpImport.probeAlreadyRunning"));
        setLastError(message);
        result.insert(QStringLiteral("ok"), false);
        result.insert(QStringLiteral("message"), message);
        return result;
    }

    if (m_isRunning) {
        const QString message = localizedText(QStringLiteral("ytDlpImport.importAlreadyRunning"));
        setLastError(message);
        result.insert(QStringLiteral("ok"), false);
        result.insert(QStringLiteral("message"), message);
        return result;
    }

    QVariantList acceptedSources;
    QVariantList duplicateSources;
    QVariantList invalidSources;
    int ignoredEmptyCount = 0;
    bool queueChanged = false;
    bool createdJob = false;

    QList<ImportSource> updatedSources;
    if (!replaceExistingSources) {
        updatedSources = m_importSources;
    }

    QSet<QString> knownNormalizedUrls;
    QSet<QString> knownCanonicalUrls;
    for (const ImportSource &source : std::as_const(updatedSources)) {
        const QString normalized =
            source.immutableSourceInput.value(QStringLiteral("normalizedUrl")).toString().trimmed();
        if (!normalized.isEmpty()) {
            knownNormalizedUrls.insert(normalized);
        }

        const QVariantMap metadataSnapshot = source.metadataSnapshot;
        const QString canonical =
            metadataSnapshot.value(QStringLiteral("resolvedSourceUrl")).toString().trimmed();
        if (!canonical.isEmpty()) {
            knownCanonicalUrls.insert(canonical);
        }
        const QString sourceMetadataUrl =
            metadataSnapshot.value(QStringLiteral("sourceUrl")).toString().trimmed();
        if (!sourceMetadataUrl.isEmpty()) {
            knownCanonicalUrls.insert(sourceMetadataUrl);
        }
    }

    QSet<QString> batchNormalizedUrls;
    QString lastAcceptedUrl;
    for (const QString &rawInput : rawInputs) {
        const QString trimmed = rawInput.trimmed();
        if (trimmed.isEmpty()) {
            ++ignoredEmptyCount;
            continue;
        }

        const QString normalized = normalizedSourceUrl(trimmed);
        if (normalized.isEmpty()) {
            QVariantMap invalid;
            invalid.insert(QStringLiteral("rawInput"), rawInput);
            const QString unsupportedScheme = unsupportedSchemeForInput(trimmed);
            invalid.insert(QStringLiteral("issueKey"),
                           unsupportedScheme.isEmpty() ? QStringLiteral("invalid-url")
                                                       : QStringLiteral("unsupported-scheme"));
            invalid.insert(QStringLiteral("unsupportedScheme"), unsupportedScheme);
            invalidSources.push_back(invalid);
            continue;
        }

        QString duplicateReason;
        if (batchNormalizedUrls.contains(normalized) || knownNormalizedUrls.contains(normalized)) {
            duplicateReason = QStringLiteral("same-normalized-url");
        } else if (knownCanonicalUrls.contains(normalized)) {
            duplicateReason = QStringLiteral("known-canonical-source");
        }

        if (!duplicateReason.isEmpty()) {
            QVariantMap duplicate;
            duplicate.insert(QStringLiteral("rawInput"), rawInput);
            duplicate.insert(QStringLiteral("normalizedUrl"), normalized);
            duplicate.insert(QStringLiteral("duplicateReason"), duplicateReason);
            duplicateSources.push_back(duplicate);
            continue;
        }

        if (updatedSources.isEmpty() && m_importJob.jobId.trimmed().isEmpty()) {
            resetJobLifecycle();
            createdJob = true;
        } else if (replaceExistingSources && acceptedSources.isEmpty()) {
            resetJobLifecycle();
            createdJob = true;
        }

        ImportSource source;
        source.sourceId = newIdentity();
        source.createdAtMs = nowMs();
        source.status = SourcePendingProbe;
        source.failureType = NoFailure;
        source.retryEligibility = RetryNotApplicable;
        source.persistenceEligibility = DraftPersistable;
        source.immutableSourceInput = {
            {QStringLiteral("originalUrl"), trimmed},
            {QStringLiteral("normalizedUrl"), normalized}
        };
        source.queueMetadata = {
            {QStringLiteral("queuePosition"), updatedSources.size()},
            {QStringLiteral("includeInNextRun"), true}
        };
        source.runtimeState = {
            {QStringLiteral("entryCount"), 0},
            {QStringLiteral("availableEntryCount"), 0},
            {QStringLiteral("unavailableEntryCount"), 0},
            {QStringLiteral("isStale"), false}
        };
        source.finalResultState = {
            {QStringLiteral("message"), QString()},
            {QStringLiteral("lastFailureAtMs"), static_cast<qint64>(0)}
        };

        updatedSources.push_back(source);
        acceptedSources.push_back(importSourceToVariantMap(source));
        knownNormalizedUrls.insert(normalized);
        batchNormalizedUrls.insert(normalized);
        lastAcceptedUrl = normalized;
        persistRecentSourceUrl(normalized);
        queueChanged = true;
    }

    if (replaceExistingSources) {
        clearImportSession();
        m_importSources = updatedSources;
        if (!m_importSources.isEmpty() && m_importJob.jobId.trimmed().isEmpty()) {
            resetJobLifecycle();
            createdJob = true;
        }
        queueChanged = true;
    } else if (queueChanged) {
        m_importSources = updatedSources;
    }

    if (queueChanged) {
        refreshSourceQueuePositions();
        invalidateSingleSourceProbeView();
        if (!lastAcceptedUrl.isEmpty()) {
            setSourceUrl(lastAcceptedUrl);
        } else if (replaceExistingSources) {
            setSourceUrl(QString());
        }
        setLastError(QString());
        if (createdJob || !m_importSources.isEmpty()) {
            publishImportJob();
        }
        publishSources();
    }

    result.insert(QStringLiteral("acceptedSources"), acceptedSources);
    result.insert(QStringLiteral("duplicateSources"), duplicateSources);
    result.insert(QStringLiteral("invalidSources"), invalidSources);
    result.insert(QStringLiteral("acceptedCount"), acceptedSources.size());
    result.insert(QStringLiteral("duplicateCount"), duplicateSources.size());
    result.insert(QStringLiteral("invalidCount"), invalidSources.size());
    result.insert(QStringLiteral("ignoredEmptyCount"), ignoredEmptyCount);
    result.insert(QStringLiteral("queueChanged"), queueChanged);
    result.insert(QStringLiteral("createdJob"), createdJob);
    result.insert(QStringLiteral("sourceCount"), m_importSources.size());
    result.insert(QStringLiteral("hasPendingProbeSources"), !m_importSources.isEmpty());
    return result;
}

void YtDlpImportService::publishSources()
{
    emit sourcesChanged();
    persistDraftState();
}

void YtDlpImportService::publishImportJob()
{
    emit importJobChanged();
    persistDraftState();
}

void YtDlpImportService::syncEntryRuntimeLayers(ImportItem &item)
{
    item.runtimeState = {
        {QStringLiteral("state"), itemStateKey(item.state)},
        {QStringLiteral("progress"), item.progress},
        {QStringLiteral("statusText"), item.statusText}
    };
    item.finalResultState.insert(QStringLiteral("terminalResult"),
                                 isTerminalState(item.state) ? itemStateKey(item.state)
                                                             : QStringLiteral("none"));
    item.finalResultState.insert(QStringLiteral("message"),
                                 !item.errorText.trimmed().isEmpty() ? item.errorText
                                                                     : item.statusText);
    item.finalResultState.insert(QStringLiteral("failureType"), failureTypeKey(item.failureType));
    item.finalResultState.insert(QStringLiteral("finalOutputFile"), item.finalOutputFile);
    item.finalResultState.insert(QStringLiteral("plannedOutputFile"), item.plannedOutputFile);
}

void YtDlpImportService::syncSourceRuntimeFromItems()
{
    for (ImportSource &source : m_importSources) {
        int totalCount = 0;
        int availableCount = 0;
        int succeededCount = 0;
        int problemCount = 0;
        int runningCount = 0;
        int canceledCount = 0;
        for (const ImportItem &item : std::as_const(m_importItems)) {
            if (item.sourceId != source.sourceId) {
                continue;
            }
            ++totalCount;
            if (item.isPlayable) {
                ++availableCount;
            }
            if (item.state == Running) {
                ++runningCount;
            } else if (item.state == Succeeded) {
                ++succeededCount;
            } else if (item.state == Failed || item.state == Skipped) {
                ++problemCount;
            } else if (item.state == Canceled) {
                ++canceledCount;
            }
        }

        QVariantMap runtimeState = source.runtimeState;
        runtimeState.insert(QStringLiteral("entryCount"), totalCount);
        runtimeState.insert(QStringLiteral("availableEntryCount"), availableCount);
        runtimeState.insert(QStringLiteral("unavailableEntryCount"), qMax(0, totalCount - availableCount));
        if (!runtimeState.contains(QStringLiteral("isStale"))) {
            runtimeState.insert(QStringLiteral("isStale"), false);
        }
        source.runtimeState = runtimeState;

        if (runningCount > 0) {
            source.status = SourceImporting;
        } else if (canceledCount > 0 && succeededCount == 0 && problemCount == 0) {
            source.status = SourceCanceled;
        } else if (succeededCount > 0 || problemCount > 0 || canceledCount > 0) {
            source.status = (problemCount > 0 || canceledCount > 0)
                ? SourceCompletedWithFailures
                : SourceCompleted;
        } else if (source.lastProbedAtMs > 0) {
            source.status = source.metadataSnapshot.value(QStringLiteral("hasUnavailableEntries")).toBool()
                ? SourceReadyWithIssues
                : SourceReady;
        }

        if (problemCount > 0 || canceledCount > 0) {
            source.failureType = GenericFailure;
            source.retryEligibility = RetryAllowed;
            source.persistenceEligibility = ReportPersistable;
        } else if (succeededCount > 0) {
            source.failureType = NoFailure;
            source.retryEligibility = RetryNotApplicable;
            source.persistenceEligibility = ReportPersistable;
        }
    }

    if (!m_importSources.isEmpty()) {
        publishSources();
    }
}

void YtDlpImportService::publishItems(bool force)
{
    Q_UNUSED(force);
    emit itemsChanged();
    persistDraftState();
}

void YtDlpImportService::reloadPersistedHistory()
{
    if (!m_appSettingsManager) {
        return;
    }

    const QVariantList sourceUrls = m_appSettingsManager->ytDlpImportRecentSources();
    const QVariantList canonicalUrls = m_appSettingsManager->ytDlpImportRecentCanonicalSources();
    const QVariantList outputDirectories = m_appSettingsManager->ytDlpImportRecentOutputDirectories();

    if (m_recentSourceUrls != sourceUrls) {
        m_recentSourceUrls = sourceUrls;
        emit recentSourceUrlsChanged();
    }
    if (m_recentCanonicalSourceUrls != canonicalUrls) {
        m_recentCanonicalSourceUrls = canonicalUrls;
        emit recentCanonicalSourceUrlsChanged();
    }
    if (m_recentOutputDirectories != outputDirectories) {
        m_recentOutputDirectories = outputDirectories;
        emit recentOutputDirectoriesChanged();
    }
}

void YtDlpImportService::persistSettingsSnapshot()
{
    if (!m_appSettingsManager) {
        return;
    }
    m_appSettingsManager->setYtDlpImportLastSettings(currentSettingsPreset());
}

void YtDlpImportService::persistDraftState()
{
    if (!m_appSettingsManager || m_restoringPersistedDraft) {
        return;
    }

    m_appSettingsManager->setYtDlpImportDraft(buildDraftStateForPersistence());
}

QVariantMap YtDlpImportService::buildDraftStateForPersistence() const
{
    if (m_importSources.isEmpty() || m_importJob.jobId.trimmed().isEmpty()) {
        return {};
    }

    if (!m_finalSummary.isEmpty() && !m_isRunning && !m_isProbing) {
        return {};
    }

    QVariantMap draft = exportCurrentJobState();
    if (draft.isEmpty()) {
        return {};
    }

    QVariantMap jobMetadata = draft.value(QStringLiteral("jobMetadata")).toMap();
    jobMetadata.insert(QStringLiteral("startedAtMs"), static_cast<qint64>(0));
    jobMetadata.insert(QStringLiteral("finishedAtMs"), static_cast<qint64>(0));
    jobMetadata.insert(QStringLiteral("isRunning"), false);
    jobMetadata.insert(QStringLiteral("persistenceEligibility"), QStringLiteral("draft"));
    draft.insert(QStringLiteral("jobMetadata"), jobMetadata);

    QVariantList serializedSources = draft.value(QStringLiteral("sources")).toList();
    for (QVariant &value : serializedSources) {
        QVariantMap sourceMap = value.toMap();
        sourceMap.insert(QStringLiteral("sourceStatus"), sourceStatusKey(restoredSourceStatus(sourceMap)));
        sourceMap.insert(QStringLiteral("persistenceEligibility"), QStringLiteral("draft"));
        value = sourceMap;
    }
    draft.insert(QStringLiteral("sources"), serializedSources);

    QVariantList serializedItems = draft.value(QStringLiteral("items")).toList();
    for (QVariant &value : serializedItems) {
        QVariantMap itemMap = value.toMap();
        const ItemState storedState = itemStateFromKey(itemMap.value(QStringLiteral("state")).toString());
        if (storedState == Running) {
            itemMap.insert(QStringLiteral("state"), QStringLiteral("pending"));
            itemMap.insert(QStringLiteral("progress"), 0.0);
            itemMap.insert(QStringLiteral("statusText"), QString());
            itemMap.insert(QStringLiteral("errorText"), QString());
        }
        itemMap.insert(QStringLiteral("stagingOutputFile"), QString());
        itemMap.insert(QStringLiteral("stagingDirectory"), QString());
        itemMap.insert(QStringLiteral("persistenceEligibility"), QStringLiteral("draft"));
        QVariantMap runtimeState = itemMap.value(QStringLiteral("runtimeState")).toMap();
        if (storedState == Running) {
            runtimeState.insert(QStringLiteral("state"), QStringLiteral("pending"));
            runtimeState.insert(QStringLiteral("progress"), 0.0);
            runtimeState.insert(QStringLiteral("statusText"), QString());
            itemMap.insert(QStringLiteral("runtimeState"), runtimeState);
        }
        value = itemMap;
    }
    draft.insert(QStringLiteral("items"), serializedItems);
    return draft;
}

bool YtDlpImportService::restorePersistedDraft(const QVariantMap &draft)
{
    if (draft.isEmpty() || m_isRunning || m_isProbing) {
        return false;
    }

    const QVariantMap settings = draft.value(QStringLiteral("settings")).toMap();
    const QVariantMap jobMetadata = draft.value(QStringLiteral("jobMetadata")).toMap();
    const QVariantList sourceList = draft.value(QStringLiteral("sources")).toList();
    const QVariantList itemList = draft.value(QStringLiteral("items")).toList();
    if (jobMetadata.value(QStringLiteral("jobId")).toString().trimmed().isEmpty()
        || settings.isEmpty()
        || sourceList.isEmpty()) {
        return false;
    }

    m_restoringPersistedDraft = true;
    setOutputDirectory(settings.value(QStringLiteral("outputDirectory")).toString());
    setSelectedFormat(settings.value(QStringLiteral("selectedFormat")).toString());
    setNamingPolicy(settings.value(QStringLiteral("namingPolicy")).toString());
    setConflictPolicy(settings.value(QStringLiteral("conflictPolicy")).toString());
    setParallelDownloads(
        settings.value(QStringLiteral("parallelDownloads"), kDefaultParallelDownloads).toInt());
    m_restoringPersistedDraft = false;

    clearProbeResult();
    clearImportSession();
    setFinalSummary(QVariantMap());
    setStatusText(QString());
    setLastError(QString());

    m_importJob = ImportJob();
    m_importJob.jobId = jobMetadata.value(QStringLiteral("jobId")).toString().trimmed();
    m_importJob.createdAtMs = qMax<qint64>(0, jobMetadata.value(QStringLiteral("createdAtMs")).toLongLong());
    m_importJob.persistenceEligibility = DraftPersistable;
    m_importJob.defaultsSnapshot = jobMetadata.value(QStringLiteral("defaultsSnapshot")).toMap();
    if (m_importJob.defaultsSnapshot.isEmpty()) {
        m_importJob.defaultsSnapshot = currentSettingsPreset();
    } else if (!m_importJob.defaultsSnapshot.contains(QStringLiteral("parallelDownloads"))) {
        m_importJob.defaultsSnapshot.insert(QStringLiteral("parallelDownloads"), m_parallelDownloads);
    }

    const qint64 persistedAtMs = qMax<qint64>(0, draft.value(QStringLiteral("persistedAtMs")).toLongLong());
    QHash<QString, bool> sourcePreviewAllowed;

    m_importSources.clear();
    m_importSources.reserve(sourceList.size());
    for (const QVariant &value : sourceList) {
        const QVariantMap sourceMap = value.toMap();
        const QString sourceId = sourceMap.value(QStringLiteral("sourceId")).toString().trimmed();
        if (sourceId.isEmpty()) {
            continue;
        }

        ImportSource source;
        source.sourceId = sourceId;
        source.createdAtMs = qMax<qint64>(0, sourceMap.value(QStringLiteral("createdAtMs")).toLongLong());
        source.lastProbedAtMs = qMax<qint64>(0, sourceMap.value(QStringLiteral("lastProbedAtMs")).toLongLong());
        source.failureType = failureTypeFromCategoryKey(sourceMap.value(QStringLiteral("failureType")).toString());
        source.retryEligibility =
            retryEligibilityFromKey(sourceMap.value(QStringLiteral("retryEligibility")).toString());
        source.persistenceEligibility = DraftPersistable;
        source.immutableSourceInput = sourceMap.value(QStringLiteral("immutableSourceInput")).toMap();
        source.metadataSnapshot = sourceMap.value(QStringLiteral("metadataSnapshot")).toMap();
        source.queueMetadata = sourceMap.value(QStringLiteral("queueMetadata")).toMap();
        source.runtimeState = sourceMap.value(QStringLiteral("runtimeState")).toMap();
        source.finalResultState = sourceMap.value(QStringLiteral("finalResultState")).toMap();

        const qint64 metadataTimestampMs =
            qMax(source.runtimeState.value(QStringLiteral("metadataTimestampMs")).toLongLong(),
                 source.lastProbedAtMs);
        const QString probeFormatSnapshot =
            source.runtimeState.value(QStringLiteral("probeFormatSnapshot")).toString().trimmed();
        const bool hasMetadata = !source.metadataSnapshot.isEmpty();
        const bool formatMismatch =
            hasMetadata && !probeFormatSnapshot.isEmpty() && probeFormatSnapshot != m_selectedFormat;
        const bool metadataFresh = hasMetadata && metadataTimestampMs > 0
            && (persistedAtMs <= 0 || persistedAtMs - metadataTimestampMs <= kProbeSnapshotFreshnessMs);
        source.runtimeState.insert(QStringLiteral("probeFormatMismatch"), formatMismatch);
        source.runtimeState.insert(QStringLiteral("isStale"), hasMetadata && (!metadataFresh || formatMismatch));
        source.status = restoredSourceStatus(sourceMap);
        if (!hasMetadata && source.status != SourceProbeFailed) {
            source.status = SourcePendingProbe;
        }
        sourcePreviewAllowed.insert(sourceId, hasMetadata);
        m_importSources.push_back(source);
    }

    refreshSourceQueuePositions();
    m_importItems.clear();
    m_importItems.reserve(itemList.size());
    for (const QVariant &value : itemList) {
        const QVariantMap itemMap = value.toMap();
        const QString sourceId = itemMap.value(QStringLiteral("sourceId")).toString().trimmed();
        if (sourceId.isEmpty() || !sourcePreviewAllowed.value(sourceId, false)) {
            continue;
        }

        ImportItem item;
        item.entryId = itemMap.value(QStringLiteral("itemId")).toString().trimmed();
        if (item.entryId.isEmpty()) {
            item.entryId = itemMap.value(QStringLiteral("entryId")).toString().trimmed();
        }
        if (item.entryId.isEmpty()) {
            continue;
        }
        item.sourceId = sourceId;
        item.sourceUrl = itemMap.value(QStringLiteral("sourceUrl")).toString();
        item.extractor = itemMap.value(QStringLiteral("extractor")).toString();
        item.title = itemMap.value(QStringLiteral("title")).toString();
        item.entrySourceId = itemMap.value(QStringLiteral("entrySourceId")).toString();
        item.duration = itemMap.value(QStringLiteral("duration")).toLongLong();
        item.thumbnail = itemMap.value(QStringLiteral("thumbnail")).toString();
        item.webpageUrl = itemMap.value(QStringLiteral("webpageUrl")).toString();
        item.availability = itemMap.value(QStringLiteral("availability")).toString();
        item.playlistIndex = itemMap.contains(QStringLiteral("playlistIndex"))
            ? itemMap.value(QStringLiteral("playlistIndex")).toInt()
            : -1;
        item.metadataOrder = itemMap.contains(QStringLiteral("metadataOrder"))
            ? itemMap.value(QStringLiteral("metadataOrder")).toInt()
            : -1;
        item.isPlayable = itemMap.value(QStringLiteral("isPlayable")).toBool();
        item.plannedOutputFile = itemMap.value(QStringLiteral("plannedOutputFile")).toString();
        item.finalOutputFile = itemMap.value(QStringLiteral("finalOutputFile")).toString();
        item.resultTrackInsertIndex = itemMap.contains(QStringLiteral("resultTrackInsertIndex"))
            ? itemMap.value(QStringLiteral("resultTrackInsertIndex")).toInt()
            : -1;
        item.retryCount = itemMap.value(QStringLiteral("retryCount")).toInt();
        item.createdAtMs = qMax<qint64>(0, itemMap.value(QStringLiteral("createdAtMs")).toLongLong());
        item.updatedAtMs = qMax<qint64>(item.createdAtMs, itemMap.value(QStringLiteral("updatedAtMs")).toLongLong());
        item.failureType = failureTypeFromCategoryKey(itemMap.value(QStringLiteral("failureType")).toString());
        item.retryEligibility = retryEligibilityFromKey(itemMap.value(QStringLiteral("retryEligibility")).toString());
        item.persistenceEligibility = DraftPersistable;
        item.immutableSourceInput = itemMap.value(QStringLiteral("immutableSourceInput")).toMap();
        item.metadataSnapshot = itemMap.value(QStringLiteral("metadataSnapshot")).toMap();
        item.queueMetadata = itemMap.value(QStringLiteral("queueMetadata")).toMap();
        item.runtimeState = itemMap.value(QStringLiteral("runtimeState")).toMap();
        item.finalResultState = itemMap.value(QStringLiteral("finalResultState")).toMap();
        item.previewDiagnostics = itemMap.value(QStringLiteral("previewDiagnostics")).toMap();
        const QVariantMap conflictMap = itemMap.value(QStringLiteral("conflictResolution")).toMap();
        item.conflictResolution.requestedOutputFile =
            conflictMap.value(QStringLiteral("requestedOutputFile")).toString();
        item.conflictResolution.resolvedOutputFile =
            conflictMap.value(QStringLiteral("resolvedOutputFile")).toString();
        item.conflictResolution.resolutionKey =
            conflictMap.value(QStringLiteral("resolutionKey")).toString();
        item.conflictResolution.collisionRuleKey =
            conflictMap.value(QStringLiteral("collisionRuleKey")).toString();
        item.conflictResolution.hadConflict =
            conflictMap.value(QStringLiteral("hadConflict")).toBool();
        item.conflictResolution.targetExistsOnDisk =
            conflictMap.value(QStringLiteral("targetExistsOnDisk")).toBool();
        item.conflictResolution.finalizationStrategyKey =
            conflictMap.value(QStringLiteral("finalizationStrategyKey")).toString();

        item.state = itemStateFromKey(itemMap.value(QStringLiteral("state")).toString());
        if (item.state == Running) {
            item.state = Pending;
            item.progress = 0.0;
            item.statusText.clear();
            item.errorText.clear();
        } else {
            item.progress = qBound(0.0, itemMap.value(QStringLiteral("progress")).toDouble(), 1.0);
            item.statusText = itemMap.value(QStringLiteral("statusText")).toString();
            item.errorText = itemMap.value(QStringLiteral("errorText")).toString();
        }

        if (item.state == Succeeded && !item.finalOutputFile.trimmed().isEmpty()
            && !QFileInfo::exists(item.finalOutputFile)) {
            item.state = Pending;
            item.progress = 0.0;
            item.statusText.clear();
            item.errorText.clear();
            item.finalOutputFile.clear();
            item.failureType = NoFailure;
            item.retryEligibility = item.isPlayable ? RetryNotApplicable : RetryBlocked;
        }

        item.stagingDirectory.clear();
        item.stagingOutputFile.clear();
        syncEntryRuntimeLayers(item);
        m_importItems.push_back(item);
    }

    publishImportJob();
    publishSources();
    publishItems(true);
    updateBatchProgress();
    refreshPreviewOutputPlan();

    const QString normalizedUrl =
        !m_importSources.isEmpty()
        ? m_importSources.constLast().immutableSourceInput.value(QStringLiteral("normalizedUrl")).toString().trimmed()
        : QString();
    if (!normalizedUrl.isEmpty()) {
        setSourceUrl(normalizedUrl);
    }
    return true;
}

void YtDlpImportService::persistRecentSourceUrl(const QString &url)
{
    if (!m_appSettingsManager || url.trimmed().isEmpty()) {
        return;
    }

    QVariantList next;
    next.reserve(m_recentSourceUrls.size() + 1);
    next.prepend(url);
    for (const QVariant &value : std::as_const(m_recentSourceUrls)) {
        if (value.toString() != url) {
            next.push_back(value);
        }
    }
    m_appSettingsManager->setYtDlpImportRecentSources(next);
    reloadPersistedHistory();
}

void YtDlpImportService::persistRecentCanonicalSourceUrl(const QString &url)
{
    if (!m_appSettingsManager || url.trimmed().isEmpty()) {
        return;
    }

    QVariantList next;
    next.reserve(m_recentCanonicalSourceUrls.size() + 1);
    next.prepend(url);
    for (const QVariant &value : std::as_const(m_recentCanonicalSourceUrls)) {
        if (value.toString() != url) {
            next.push_back(value);
        }
    }
    m_appSettingsManager->setYtDlpImportRecentCanonicalSources(next);
    reloadPersistedHistory();
}

void YtDlpImportService::persistRecentOutputDirectory(const QString &directory)
{
    if (!m_appSettingsManager || directory.trimmed().isEmpty()) {
        return;
    }

    QVariantList next;
    next.reserve(m_recentOutputDirectories.size() + 1);
    next.prepend(directory);
    for (const QVariant &value : std::as_const(m_recentOutputDirectories)) {
        if (value.toString() != directory) {
            next.push_back(value);
        }
    }
    m_appSettingsManager->setYtDlpImportRecentOutputDirectories(next);
    reloadPersistedHistory();
}

void YtDlpImportService::clearUnstartedOutputPlan()
{
    if (m_isRunning || m_isProbing || m_importJob.startedAtMs > 0 || !m_finalSummary.isEmpty()) {
        return;
    }

    bool changed = false;
    for (ImportItem &item : m_importItems) {
        if (!item.plannedOutputFile.isEmpty()
            || !item.stagingOutputFile.isEmpty()
            || !item.stagingDirectory.isEmpty()) {
            item.plannedOutputFile.clear();
            item.stagingOutputFile.clear();
            item.stagingDirectory.clear();
            syncEntryRuntimeLayers(item);
            changed = true;
        }
    }
    if (changed) {
        m_queuePrepared = false;
        publishItems(true);
    }
}

void YtDlpImportService::refreshPreviewOutputPlan()
{
    if (m_isRunning || m_isProbing || m_importJob.startedAtMs > 0 || !m_finalSummary.isEmpty()) {
        return;
    }
    if (m_importItems.isEmpty() || m_outputDirectory.trimmed().isEmpty()) {
        return;
    }

    struct OrderKey {
        int sourceQueuePosition = -1;
        int itemIndex = -1;
        int rank = -1;
        bool hasPlaylistIndex = false;
    };

    QList<OrderKey> planningOrder;
    planningOrder.reserve(m_importItems.size());
    for (int i = 0; i < m_importItems.size(); ++i) {
        const ImportItem &item = m_importItems.at(i);
        if (!item.isPlayable) {
            continue;
        }
        const QVariantMap sourceMetadata = sourceById(item.sourceId).value(QStringLiteral("metadataSnapshot")).toMap();
        planningOrder.push_back(
            {sourceById(item.sourceId).value(QStringLiteral("queueMetadata")).toMap().value(QStringLiteral("queuePosition")).toInt(),
             i,
             item.playlistIndex >= 0 ? item.playlistIndex : item.metadataOrder,
             item.playlistIndex >= 0});
    }

    std::sort(planningOrder.begin(), planningOrder.end(), [](const OrderKey &a, const OrderKey &b) {
        if (a.sourceQueuePosition != b.sourceQueuePosition) {
            return a.sourceQueuePosition < b.sourceQueuePosition;
        }
        if (a.hasPlaylistIndex != b.hasPlaylistIndex) {
            return a.hasPlaylistIndex && !b.hasPlaylistIndex;
        }
        if (a.rank != b.rank) {
            return a.rank < b.rank;
        }
        return a.itemIndex < b.itemIndex;
    });

    const FormatArgumentPlan formatPlan = formatArgumentPlanForSelection(m_selectedFormat);
    QSet<QString> reservedPaths;
    QSet<QString> existingPaths;
    bool changed = false;

    for (const OrderKey &order : std::as_const(planningOrder)) {
        ImportItem &item = m_importItems[order.itemIndex];
        const QVariantMap sourceMetadata = sourceById(item.sourceId).value(QStringLiteral("metadataSnapshot")).toMap();
        ProbeEntry entry;
        entry.sourceUrl = item.sourceUrl;
        entry.extractor = item.extractor;
        entry.title = item.title;
        entry.entryId = item.entrySourceId;
        entry.playlistIndex = item.playlistIndex;
        entry.duration = item.duration;
        entry.thumbnail = item.thumbnail;
        entry.webpageUrl = item.webpageUrl;
        entry.availability = item.availability;
        entry.isPlayable = item.isPlayable;
        entry.metadataOrder = item.metadataOrder;

        const bool isPlaylist = sourceMetadata.value(QStringLiteral("isPlaylist")).toBool()
            || sourceMetadata.value(QStringLiteral("entryCount")).toInt() > 1;
        const NamingPlan namingPlan = buildNamingPlanForEntry(
            entry, sourceMetadata, m_namingPolicy, formatPlan.outputExtension, isPlaylist);
        const QString requestedOutputFile =
            QDir(m_outputDirectory).filePath(namingPlan.fileName);
        const bool targetExists =
            QFileInfo::exists(requestedOutputFile) || existingPaths.contains(requestedOutputFile);
        const bool queueConflict = reservedPaths.contains(requestedOutputFile);
        const bool hasConflict = targetExists || queueConflict;

        QString plannedOutputFile = requestedOutputFile;
        QString resolutionKey = QStringLiteral("planned");
        QString finalizationStrategyKey = QStringLiteral("temp-commit");
        if (hasConflict) {
            if (m_conflictPolicy == QStringLiteral("skip-on-conflict")) {
                resolutionKey = QStringLiteral("skip-on-conflict");
                finalizationStrategyKey = QStringLiteral("not-started");
            } else if (m_conflictPolicy == QStringLiteral("fail-on-conflict")) {
                resolutionKey = QStringLiteral("fail-on-conflict");
                finalizationStrategyKey = QStringLiteral("not-started");
            } else if (m_conflictPolicy == QStringLiteral("overwrite-if-allowed") && !queueConflict) {
                resolutionKey = QStringLiteral("overwrite-existing");
                finalizationStrategyKey = QStringLiteral("temp-replace");
            } else {
                plannedOutputFile = uniqueOutputPath(requestedOutputFile, reservedPaths, existingPaths);
                resolutionKey = plannedOutputFile == requestedOutputFile
                    ? QStringLiteral("planned")
                    : QStringLiteral("auto-renamed");
            }
        }

        QVariantMap previewDiagnostics = {
            {QStringLiteral("requestedNamingPolicy"), m_namingPolicy},
            {QStringLiteral("appliedNamingPolicy"), namingPlan.appliedPolicy},
            {QStringLiteral("requestedOutputFile"), requestedOutputFile},
            {QStringLiteral("resolvedOutputFile"), plannedOutputFile},
            {QStringLiteral("sourceTitle"), namingPlan.sourceTitle},
            {QStringLiteral("entryTitle"), namingPlan.entryTitle},
            {QStringLiteral("baseName"), QFileInfo(plannedOutputFile).completeBaseName()},
            {QStringLiteral("sourceDirectoryPolicy"), QStringLiteral("explicit-output-directory")},
            {QStringLiteral("formatPlan"), formatPlan.toVariantMap()},
            {QStringLiteral("missingMetadataFields"), QVariantList(namingPlan.missingMetadataFields.begin(),
                                                                   namingPlan.missingMetadataFields.end())},
            {QStringLiteral("namingFallbackReasonKey"), namingPlan.fallbackReasonKey},
            {QStringLiteral("conflictPolicy"), m_conflictPolicy},
            {QStringLiteral("collisionScope"), queueConflict
                 ? QStringLiteral("in-job")
                 : targetExists ? QStringLiteral("existing-target")
                                : QStringLiteral("none")},
            {QStringLiteral("collisionRuleKey"), queueConflict
                 ? QStringLiteral("queue-conflict")
                 : targetExists ? QStringLiteral("existing-target")
                                : QStringLiteral("none")},
            {QStringLiteral("resolutionKey"), resolutionKey},
            {QStringLiteral("targetExistsOnDisk"), targetExists},
            {QStringLiteral("finalizationStrategyKey"), finalizationStrategyKey}
        };

        ImportItem::ConflictResolutionInfo conflictResolution;
        conflictResolution.requestedOutputFile = requestedOutputFile;
        conflictResolution.resolvedOutputFile = plannedOutputFile;
        conflictResolution.hadConflict = hasConflict;
        conflictResolution.targetExistsOnDisk = targetExists;
        conflictResolution.collisionRuleKey = queueConflict
            ? QStringLiteral("queue-conflict")
            : targetExists ? QStringLiteral("existing-target")
                           : QStringLiteral("none");
        conflictResolution.resolutionKey = resolutionKey;
        conflictResolution.finalizationStrategyKey = finalizationStrategyKey;

        if (item.plannedOutputFile != plannedOutputFile
            || item.previewDiagnostics != previewDiagnostics
            || item.conflictResolution.requestedOutputFile != conflictResolution.requestedOutputFile
            || item.conflictResolution.resolvedOutputFile != conflictResolution.resolvedOutputFile
            || item.conflictResolution.resolutionKey != conflictResolution.resolutionKey
            || item.conflictResolution.collisionRuleKey != conflictResolution.collisionRuleKey
            || item.conflictResolution.hadConflict != conflictResolution.hadConflict
            || item.conflictResolution.targetExistsOnDisk != conflictResolution.targetExistsOnDisk
            || item.conflictResolution.finalizationStrategyKey != conflictResolution.finalizationStrategyKey) {
            item.plannedOutputFile = plannedOutputFile;
            item.previewDiagnostics = previewDiagnostics;
            item.conflictResolution = conflictResolution;
            changed = true;
        }

        if (!(hasConflict && (m_conflictPolicy == QStringLiteral("skip-on-conflict")
                              || m_conflictPolicy == QStringLiteral("fail-on-conflict")))) {
            reservedPaths.insert(plannedOutputFile);
            existingPaths.insert(requestedOutputFile);
        }
    }

    if (changed) {
        publishItems(true);
    }
}

void YtDlpImportService::archiveCompletedReport(const QVariantMap &summary)
{
    const QString jobId = summary.value(QStringLiteral("jobId")).toString().trimmed();
    QVariantList updatedReports;
    updatedReports.reserve(m_completedReports.size() + 1);
    updatedReports.push_back(summary);
    for (const QVariant &value : std::as_const(m_completedReports)) {
        const QVariantMap existing = value.toMap();
        if (!jobId.isEmpty()
            && existing.value(QStringLiteral("jobId")).toString().trimmed() == jobId) {
            continue;
        }
        if (existing == summary) {
            continue;
        }
        updatedReports.push_back(existing);
        if (updatedReports.size() >= kCompletedReportRetention) {
            break;
        }
    }

    if (m_completedReports == updatedReports) {
        return;
    }
    m_completedReports = updatedReports;
    emit completedReportsChanged();
}

QString YtDlpImportService::reportTextForSummary(const QVariantMap &summary) const
{
    if (summary.isEmpty()) {
        return {};
    }

    QStringList lines;
    const QString headline = summary.value(QStringLiteral("headlineText")).toString().trimmed();
    const QString detail = summary.value(QStringLiteral("detailText")).toString().trimmed();
    if (!headline.isEmpty()) {
        lines.push_back(headline);
    }
    if (!detail.isEmpty()) {
        lines.push_back(detail);
    }

    lines.push_back(QStringLiteral("Imported: %1").arg(summary.value(QStringLiteral("importedCount")).toInt()));
    lines.push_back(QStringLiteral("Succeeded: %1").arg(summary.value(QStringLiteral("succeededCount")).toInt()));
    lines.push_back(QStringLiteral("Failed: %1").arg(summary.value(QStringLiteral("failedCount")).toInt()));
    lines.push_back(QStringLiteral("Skipped: %1").arg(summary.value(QStringLiteral("skippedCount")).toInt()));
    lines.push_back(QStringLiteral("Canceled: %1").arg(summary.value(QStringLiteral("canceledCount")).toInt()));
    lines.push_back(QStringLiteral("Not probed: %1").arg(summary.value(QStringLiteral("notProbedCount")).toInt()));
    lines.push_back(QStringLiteral("Conflict blocked: %1").arg(summary.value(QStringLiteral("conflictBlockedCount")).toInt()));

    const QStringList outputs = summary.value(QStringLiteral("orderedResultFiles")).toStringList();
    if (!outputs.isEmpty()) {
        lines.push_back(QStringLiteral(""));
        lines.push_back(QStringLiteral("Successful outputs:"));
        for (const QString &path : outputs) {
            lines.push_back(QStringLiteral("- %1").arg(path));
        }
    }

    const QVariantList problems = summary.value(QStringLiteral("problemItems")).toList();
    if (!problems.isEmpty()) {
        lines.push_back(QStringLiteral(""));
        lines.push_back(QStringLiteral("Problems:"));
        for (const QVariant &value : problems) {
            const QVariantMap item = value.toMap();
            const QString title = item.value(QStringLiteral("title")).toString().trimmed();
            const QString state = item.value(QStringLiteral("state")).toString().trimmed();
            const QString message = item.value(QStringLiteral("message")).toString().trimmed();
            lines.push_back(QStringLiteral("- %1 [%2] %3").arg(title, state, message));
        }
    }

    return lines.join(QLatin1Char('\n'));
}

QVariantMap YtDlpImportService::exportCurrentJobState() const
{
    if (m_importJob.jobId.trimmed().isEmpty() || m_importSources.isEmpty()) {
        return {};
    }

    QVariantMap result;
    result.insert(QStringLiteral("schema"), QString::fromLatin1(kImportDraftSchema));
    result.insert(QStringLiteral("persistedAtMs"), nowMs());
    result.insert(QStringLiteral("jobMetadata"), importJob());
    result.insert(QStringLiteral("settings"),
                  QVariantMap{{QStringLiteral("outputDirectory"), m_outputDirectory},
                              {QStringLiteral("selectedFormat"), m_selectedFormat},
                              {QStringLiteral("namingPolicy"), m_namingPolicy},
                              {QStringLiteral("conflictPolicy"), m_conflictPolicy},
                              {QStringLiteral("parallelDownloads"), m_parallelDownloads}});
    result.insert(QStringLiteral("sources"), sources());

    QVariantList serializedItems;
    serializedItems.reserve(m_importItems.size());
    for (const ImportItem &item : m_importItems) {
        QVariantMap serialized = importItemToVariantMap(item);
        if (item.state == Running) {
            serialized.insert(QStringLiteral("state"), QStringLiteral("pending"));
            serialized.insert(QStringLiteral("progress"), 0.0);
            QVariantMap runtimeState = serialized.value(QStringLiteral("runtimeState")).toMap();
            runtimeState.insert(QStringLiteral("state"), QStringLiteral("pending"));
            runtimeState.insert(QStringLiteral("progress"), 0.0);
            serialized.insert(QStringLiteral("runtimeState"), runtimeState);
        }
        serializedItems.push_back(serialized);
    }
    result.insert(QStringLiteral("items"), serializedItems);
    return result;
}

QVariantMap YtDlpImportService::currentSettingsPreset() const
{
    return QVariantMap{
        {QStringLiteral("outputDirectory"), m_outputDirectory},
        {QStringLiteral("selectedFormat"), m_selectedFormat},
        {QStringLiteral("namingPolicy"), m_namingPolicy},
        {QStringLiteral("conflictPolicy"), m_conflictPolicy},
        {QStringLiteral("parallelDownloads"), m_parallelDownloads}
    };
}

bool YtDlpImportService::applySettingsPreset(const QVariantMap &preset)
{
    if (m_isRunning || m_isProbing) {
        return false;
    }

    setOutputDirectory(preset.value(QStringLiteral("outputDirectory")).toString());
    setSelectedFormat(preset.value(QStringLiteral("selectedFormat")).toString());
    setNamingPolicy(preset.value(QStringLiteral("namingPolicy")).toString());
    setConflictPolicy(preset.value(QStringLiteral("conflictPolicy")).toString());
    setParallelDownloads(
        preset.value(QStringLiteral("parallelDownloads"), kDefaultParallelDownloads).toInt());
    persistSettingsSnapshot();
    return true;
}

QVariantMap YtDlpImportService::latestCompletedReport() const
{
    return m_completedReports.isEmpty() ? QVariantMap() : m_completedReports.constFirst().toMap();
}

bool YtDlpImportService::reopenCompletedReport(const QString &jobId)
{
    if (m_isRunning || m_isProbing) {
        return false;
    }

    const QString normalizedJobId = jobId.trimmed();
    if (normalizedJobId.isEmpty()) {
        return false;
    }

    for (const QVariant &value : m_completedReports) {
        const QVariantMap report = value.toMap();
        if (report.value(QStringLiteral("jobId")).toString().trimmed() != normalizedJobId) {
            continue;
        }
        setFinalSummary(report);
        setStatusText(report.value(QStringLiteral("detailText")).toString());
        setLastError(report.value(QStringLiteral("lastError")).toString());
        return true;
    }
    return false;
}

QString YtDlpImportService::currentReportText() const
{
    return reportTextForSummary(m_finalSummary);
}

QString YtDlpImportService::sourceIdAt(int index) const
{
    return index >= 0 && index < m_importSources.size() ? m_importSources.at(index).sourceId : QString();
}

int YtDlpImportService::indexOfSourceId(const QString &sourceId) const
{
    const QString normalized = sourceId.trimmed();
    for (int i = 0; i < m_importSources.size(); ++i) {
        if (m_importSources.at(i).sourceId == normalized) {
            return i;
        }
    }
    return -1;
}

QVariantMap YtDlpImportService::sourceById(const QString &sourceId) const
{
    const int index = indexOfSourceId(sourceId);
    return index >= 0 ? importSourceToVariantMap(m_importSources.at(index)) : QVariantMap();
}

QString YtDlpImportService::itemIdAt(int index) const
{
    return index >= 0 && index < m_importItems.size() ? m_importItems.at(index).entryId : QString();
}

int YtDlpImportService::indexOfItemId(const QString &itemId) const
{
    const QString normalized = itemId.trimmed();
    for (int i = 0; i < m_importItems.size(); ++i) {
        if (m_importItems.at(i).entryId == normalized) {
            return i;
        }
    }
    return -1;
}

QVariantMap YtDlpImportService::itemById(const QString &itemId) const
{
    const int index = indexOfItemId(itemId);
    return index >= 0 ? importItemToVariantMap(m_importItems.at(index)) : QVariantMap();
}
