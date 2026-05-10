#include "BatchAudioConverterPresetManager.h"
#include "AppSettingsManager.h"

#include <QDateTime>
#include <QRegularExpression>

namespace {
const QRegularExpression kUserIdPattern(QStringLiteral("^batch:(\\d+)$"));

QString localizedBatchPresetText(const QString &key)
{
    return AppSettingsManager::translateForCurrentLanguage(key);
}

QString normalizeNamingPolicy(const QString &value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("artist-title")
        || normalized == QStringLiteral("album-track-title")) {
        return normalized;
    }
    return QStringLiteral("basename");
}

QString normalizeFormat(const QString &value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("flac")
        || normalized == QStringLiteral("wav")
        || normalized == QStringLiteral("opus")) {
        return normalized;
    }
    return QStringLiteral("mp3");
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

QString normalizePlaylistAddMode(const QString &value, bool addResultsToPlaylistFallback = true)
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

QString normalizeChannelMode(const QString &value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("mono")) {
        return normalized;
    }
    return QStringLiteral("stereo");
}

int normalizeBitrate(int value)
{
    return qMax(0, value);
}

int normalizeSampleRate(int value)
{
    return qMax(0, value);
}

double normalizePlaybackRate(double value)
{
    if (!qIsFinite(value)) {
        return 1.0;
    }
    return qBound(0.25, value, 4.0);
}

int normalizePitchSemitones(int value)
{
    return qBound(-24, value, 24);
}
} // namespace

BatchAudioConverterPresetManager::BatchAudioConverterPresetManager(QObject *parent)
    : QObject(parent)
{
}

QVariantList BatchAudioConverterPresetManager::userPresets() const
{
    QVariantList result;
    result.reserve(m_userPresets.size());
    for (const Preset &preset : m_userPresets) {
        result.push_back(presetToVariantMap(preset));
    }
    return result;
}

QVariantList BatchAudioConverterPresetManager::listUserPresets() const
{
    return userPresets();
}

QVariantMap BatchAudioConverterPresetManager::getPreset(const QString &presetId) const
{
    const int index = findUserPresetIndex(presetId);
    if (index < 0) {
        return {};
    }
    return presetToVariantMap(m_userPresets.at(index));
}

QString BatchAudioConverterPresetManager::createUserPreset(const QString &name,
                                                           const QVariantMap &settings)
{
    const QString uniqueName = makeUniqueUserPresetName(name);
    if (uniqueName.isEmpty()) {
        setLastError(localizedBatchPresetText(QStringLiteral("error.presetNameEmpty")));
        return {};
    }

    Preset preset;
    preset.id = makeNextUserPresetId();
    preset.name = uniqueName;
    preset.settings = normalizePresetSettings(settings);
    preset.updatedAtMs = QDateTime::currentMSecsSinceEpoch();

    m_userPresets.push_back(preset);
    setLastError(QString());
    bumpRevisionAndNotify();
    return preset.id;
}

bool BatchAudioConverterPresetManager::updateUserPreset(const QString &presetId,
                                                        const QString &name,
                                                        const QVariantMap &settings)
{
    const int index = findUserPresetIndex(presetId);
    if (index < 0) {
        setLastError(localizedBatchPresetText(QStringLiteral("error.userPresetNotFound")));
        return false;
    }

    const QString uniqueName = makeUniqueUserPresetName(name, presetId);
    if (uniqueName.isEmpty()) {
        setLastError(localizedBatchPresetText(QStringLiteral("error.presetNameEmpty")));
        return false;
    }

    Preset &preset = m_userPresets[index];
    const QVariantMap normalizedSettings = normalizePresetSettings(settings);
    if (preset.name == uniqueName && preset.settings == normalizedSettings) {
        setLastError(QString());
        return true;
    }

    preset.name = uniqueName;
    preset.settings = normalizedSettings;
    preset.updatedAtMs = QDateTime::currentMSecsSinceEpoch();
    setLastError(QString());
    bumpRevisionAndNotify();
    return true;
}

bool BatchAudioConverterPresetManager::renameUserPreset(const QString &presetId, const QString &name)
{
    const int index = findUserPresetIndex(presetId);
    if (index < 0) {
        setLastError(localizedBatchPresetText(QStringLiteral("error.userPresetNotFound")));
        return false;
    }

    const QString uniqueName = makeUniqueUserPresetName(name, presetId);
    if (uniqueName.isEmpty()) {
        setLastError(localizedBatchPresetText(QStringLiteral("error.presetNameEmpty")));
        return false;
    }

    Preset &preset = m_userPresets[index];
    if (preset.name == uniqueName) {
        setLastError(QString());
        return true;
    }

    preset.name = uniqueName;
    preset.updatedAtMs = QDateTime::currentMSecsSinceEpoch();
    setLastError(QString());
    bumpRevisionAndNotify();
    return true;
}

bool BatchAudioConverterPresetManager::deleteUserPreset(const QString &presetId)
{
    const int index = findUserPresetIndex(presetId);
    if (index < 0) {
        setLastError(localizedBatchPresetText(QStringLiteral("error.userPresetNotFound")));
        return false;
    }

    m_userPresets.removeAt(index);
    setLastError(QString());
    bumpRevisionAndNotify();
    return true;
}

QVariantList BatchAudioConverterPresetManager::exportUserPresetsSnapshot() const
{
    return userPresets();
}

bool BatchAudioConverterPresetManager::replaceUserPresets(const QVariantList &serializedPresets)
{
    QList<Preset> parsed;
    parsed.reserve(serializedPresets.size());

    int nextUserId = 1;
    for (const QVariant &value : serializedPresets) {
        const QVariantMap serialized = value.toMap();
        Preset preset;
        QString error;
        if (!parseSerializedPreset(serialized, &preset, &error)) {
            setLastError(error);
            return false;
        }
        parsed.push_back(preset);

        const QRegularExpressionMatch match = kUserIdPattern.match(preset.id);
        if (match.hasMatch()) {
            nextUserId = qMax(nextUserId, match.captured(1).toInt() + 1);
        }
    }

    m_userPresets = parsed;
    m_nextUserId = nextUserId;
    setLastError(QString());
    bumpRevisionAndNotify();
    return true;
}

QVariantMap BatchAudioConverterPresetManager::normalizePresetSettings(const QVariantMap &settings)
{
    QVariantMap normalized;
    normalized.insert(QStringLiteral("outputDirectory"),
                      settings.value(QStringLiteral("outputDirectory")).toString().trimmed());
    normalized.insert(QStringLiteral("namingPolicy"),
                      normalizeNamingPolicy(settings.value(QStringLiteral("namingPolicy")).toString()));
    normalized.insert(QStringLiteral("format"),
                      normalizeFormat(settings.value(QStringLiteral("format")).toString()));
    normalized.insert(QStringLiteral("conflictPolicy"),
                      normalizeConflictPolicy(settings.value(QStringLiteral("conflictPolicy")).toString()));
    normalized.insert(QStringLiteral("playlistAddMode"),
                      normalizePlaylistAddMode(
                          settings.value(QStringLiteral("playlistAddMode")).toString(),
                          settings.value(QStringLiteral("addResultsToPlaylist"), true).toBool()));
    normalized.insert(QStringLiteral("bitrate"),
                      normalizeBitrate(settings.value(QStringLiteral("bitrate")).toInt()));
    normalized.insert(QStringLiteral("sampleRate"),
                      normalizeSampleRate(settings.value(QStringLiteral("sampleRate")).toInt()));
    normalized.insert(QStringLiteral("channelMode"),
                      normalizeChannelMode(settings.value(QStringLiteral("channelMode")).toString()));
    normalized.insert(QStringLiteral("playbackRate"),
                      normalizePlaybackRate(settings.value(QStringLiteral("playbackRate")).toDouble()));
    normalized.insert(QStringLiteral("pitchSemitones"),
                      normalizePitchSemitones(settings.value(QStringLiteral("pitchSemitones")).toInt()));
    normalized.insert(QStringLiteral("addResultsToPlaylist"),
                      normalized.value(QStringLiteral("playlistAddMode")).toString()
                          != QStringLiteral("disabled"));
    return normalized;
}

QVariantMap BatchAudioConverterPresetManager::presetToVariantMap(const Preset &preset)
{
    QVariantMap map;
    map.insert(QStringLiteral("id"), preset.id);
    map.insert(QStringLiteral("name"), preset.name);
    map.insert(QStringLiteral("settings"), preset.settings);
    map.insert(QStringLiteral("updatedAtMs"), preset.updatedAtMs);
    return map;
}

bool BatchAudioConverterPresetManager::parseSerializedPreset(const QVariantMap &serialized,
                                                             Preset *presetOut,
                                                             QString *errorOut)
{
    if (!presetOut) {
        if (errorOut) {
            *errorOut = QStringLiteral("Output preset is null");
        }
        return false;
    }

    const QString id = serialized.value(QStringLiteral("id")).toString().trimmed();
    const QString name = sanitizeName(serialized.value(QStringLiteral("name")).toString());
    if (id.isEmpty() || name.isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Preset id and name are required");
        }
        return false;
    }

    presetOut->id = id;
    presetOut->name = name;
    presetOut->settings = normalizePresetSettings(serialized.value(QStringLiteral("settings")).toMap());
    presetOut->updatedAtMs = serialized.value(QStringLiteral("updatedAtMs")).toLongLong();
    return true;
}

int BatchAudioConverterPresetManager::findUserPresetIndex(const QString &presetId) const
{
    const QString trimmed = presetId.trimmed();
    if (trimmed.isEmpty()) {
        return -1;
    }
    for (int i = 0; i < m_userPresets.size(); ++i) {
        if (m_userPresets.at(i).id == trimmed) {
            return i;
        }
    }
    return -1;
}

bool BatchAudioConverterPresetManager::isPresetNameUsedCaseInsensitive(
    const QString &candidateName,
    const QString &ignorePresetId) const
{
    const QString normalized = candidateName.trimmed();
    for (const Preset &preset : m_userPresets) {
        if (!ignorePresetId.isEmpty() && preset.id == ignorePresetId) {
            continue;
        }
        if (preset.name.compare(normalized, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

QString BatchAudioConverterPresetManager::makeNextUserPresetId()
{
    return QStringLiteral("batch:%1").arg(m_nextUserId++);
}

QString BatchAudioConverterPresetManager::makeUniqueUserPresetName(const QString &requestedName,
                                                                   const QString &ignorePresetId) const
{
    const QString baseName = sanitizeName(requestedName);
    if (baseName.isEmpty()) {
        return {};
    }
    if (!isPresetNameUsedCaseInsensitive(baseName, ignorePresetId)) {
        return baseName;
    }

    int suffix = 2;
    while (true) {
        const QString candidate = QStringLiteral("%1 (%2)").arg(baseName).arg(suffix);
        if (!isPresetNameUsedCaseInsensitive(candidate, ignorePresetId)) {
            return candidate;
        }
        ++suffix;
    }
}

QString BatchAudioConverterPresetManager::sanitizeName(const QString &name)
{
    return name.simplified().trimmed();
}

void BatchAudioConverterPresetManager::setLastError(const QString &error)
{
    if (m_lastError == error) {
        return;
    }
    m_lastError = error;
    emit lastErrorChanged();
}

void BatchAudioConverterPresetManager::bumpRevisionAndNotify()
{
    ++m_revision;
    emit presetsChanged();
}
