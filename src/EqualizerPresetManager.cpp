#include "EqualizerPresetManager.h"

#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTimeZone>
#include <QtGlobal>
#include <algorithm>
#include <cmath>

namespace {
const QRegularExpression kUserIdPattern(QStringLiteral("^user:(\\d+)$"));
const QString kBundleSchema = QStringLiteral("waveflux.eq.presets.v1");
const QString kMergePolicyKeepBoth = QStringLiteral("keep_both");
const QString kMergePolicyReplaceExisting = QStringLiteral("replace_existing");
}

QVariantMap EqualizerPresetManager::ImportResult::toVariantMap() const
{
    QVariantMap map;
    map.insert(QStringLiteral("success"), success);
    map.insert(QStringLiteral("importedCount"), importedCount);
    map.insert(QStringLiteral("replacedCount"), replacedCount);
    map.insert(QStringLiteral("skippedCount"), skippedCount);
    map.insert(QStringLiteral("errors"), errors);
    map.insert(QStringLiteral("mergePolicy"), mergePolicy);
    return map;
}

QVariantMap EqualizerPresetManager::ExportResult::toVariantMap() const
{
    QVariantMap map;
    map.insert(QStringLiteral("success"), success);
    map.insert(QStringLiteral("exportedCount"), exportedCount);
    map.insert(QStringLiteral("error"), error);
    map.insert(QStringLiteral("json"), jsonPayload);
    map.insert(QStringLiteral("schema"), schema);
    return map;
}

EqualizerPresetManager::EqualizerPresetManager(QObject *parent)
    : QObject(parent)
{
    m_builtInPresets = {
        makeBuiltInPreset(QStringLiteral("builtin:flat"),
                          QStringLiteral("Flat"),
                          {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}),
        makeBuiltInPreset(QStringLiteral("builtin:bass_boost"),
                          QStringLiteral("Bass Boost"),
                          {6.0, 5.0, 4.0, 2.0, 1.0, 0.0, -1.0, -2.0, -2.0, -2.0}),
        makeBuiltInPreset(QStringLiteral("builtin:vocal"),
                          QStringLiteral("Vocal"),
                          {-2.0, -1.0, 0.0, 2.0, 4.0, 4.0, 3.0, 1.0, 0.0, -1.0}),
        makeBuiltInPreset(QStringLiteral("builtin:high_boost"),
                          QStringLiteral("High Boost"),
                          {-3.0, -2.0, -1.0, 0.0, 1.0, 2.0, 4.0, 5.0, 6.0, 6.0}),
        makeBuiltInPreset(QStringLiteral("builtin:rock"),
                          QStringLiteral("Rock"),
                          {4.0, 3.0, 2.0, 0.0, -1.0, 0.0, 2.0, 3.0, 4.0, 4.0}),
        makeBuiltInPreset(QStringLiteral("builtin:pop"),
                          QStringLiteral("Pop"),
                          {-1.0, 1.0, 3.0, 4.0, 2.0, 0.0, -1.0, -1.0, 1.0, 2.0}),
        makeBuiltInPreset(QStringLiteral("builtin:jazz"),
                          QStringLiteral("Jazz"),
                          {2.0, 1.0, 0.0, 1.0, 2.0, 2.0, 1.0, 1.0, 2.0, 3.0}),
        makeBuiltInPreset(QStringLiteral("builtin:electronic"),
                          QStringLiteral("Electronic"),
                          {5.0, 4.0, 2.0, 0.0, -1.0, 1.0, 3.0, 4.0, 5.0, 4.0}),
        makeBuiltInPreset(QStringLiteral("builtin:classical"),
                          QStringLiteral("Classical"),
                          {1.0, 0.0, -1.0, -2.0, 0.0, 1.0, 2.0, 3.0, 4.0, 5.0})
    };
}

QVariantList EqualizerPresetManager::presets() const
{
    QVariantList result;
    result.reserve(m_builtInPresets.size() + m_userPresets.size());
    for (const Preset &preset : m_builtInPresets) {
        result.push_back(presetToVariantMap(preset));
    }
    for (const Preset &preset : m_userPresets) {
        result.push_back(presetToVariantMap(preset));
    }
    return result;
}

QVariantList EqualizerPresetManager::builtInPresets() const
{
    QVariantList result;
    result.reserve(m_builtInPresets.size());
    for (const Preset &preset : m_builtInPresets) {
        result.push_back(presetToVariantMap(preset));
    }
    return result;
}

QVariantList EqualizerPresetManager::userPresets() const
{
    QVariantList result;
    result.reserve(m_userPresets.size());
    for (const Preset &preset : m_userPresets) {
        result.push_back(presetToVariantMap(preset));
    }
    return result;
}

QVariantList EqualizerPresetManager::listPresets() const
{
    return presets();
}

QVariantList EqualizerPresetManager::listBuiltInPresets() const
{
    return builtInPresets();
}

QVariantList EqualizerPresetManager::listUserPresets() const
{
    return userPresets();
}

QVariantMap EqualizerPresetManager::getPreset(const QString &presetId) const
{
    const int builtInIndex = findBuiltInPresetIndex(presetId);
    if (builtInIndex >= 0) {
        return presetToVariantMap(m_builtInPresets.at(builtInIndex));
    }

    const int userIndex = findUserPresetIndex(presetId);
    if (userIndex >= 0) {
        return presetToVariantMap(m_userPresets.at(userIndex));
    }

    return {};
}

bool EqualizerPresetManager::hasPreset(const QString &presetId) const
{
    return findBuiltInPresetIndex(presetId) >= 0 || findUserPresetIndex(presetId) >= 0;
}

QString EqualizerPresetManager::createUserPreset(const QString &name, const QVariantList &gainsDb)
{
    const QString uniqueName = makeUniqueUserPresetName(name);
    if (uniqueName.isEmpty()) {
        setLastError(QStringLiteral("Preset name is empty"));
        return {};
    }

    Preset preset;
    preset.id = makeNextUserPresetId();
    preset.name = uniqueName;
    preset.gainsDb = normalizeGains(gainsDb);
    preset.builtIn = false;
    preset.updatedAtMs = QDateTime::currentMSecsSinceEpoch();

    m_userPresets.push_back(preset);
    setLastError(QString());
    bumpRevisionAndNotify();
    return preset.id;
}

bool EqualizerPresetManager::updateUserPreset(const QString &presetId,
                                              const QString &name,
                                              const QVariantList &gainsDb)
{
    const int index = findUserPresetIndex(presetId);
    if (index < 0) {
        setLastError(QStringLiteral("User preset not found"));
        return false;
    }

    const QString uniqueName = makeUniqueUserPresetName(name, presetId);
    if (uniqueName.isEmpty()) {
        setLastError(QStringLiteral("Preset name is empty"));
        return false;
    }

    Preset &preset = m_userPresets[index];
    const QVariantList normalizedGains = normalizeGains(gainsDb);
    const bool sameName = preset.name == uniqueName;
    const bool sameGains = preset.gainsDb == normalizedGains;
    if (sameName && sameGains) {
        setLastError(QString());
        return true;
    }

    preset.name = uniqueName;
    preset.gainsDb = normalizedGains;
    preset.updatedAtMs = QDateTime::currentMSecsSinceEpoch();
    setLastError(QString());
    bumpRevisionAndNotify();
    return true;
}

bool EqualizerPresetManager::renameUserPreset(const QString &presetId, const QString &name)
{
    const int index = findUserPresetIndex(presetId);
    if (index < 0) {
        setLastError(QStringLiteral("User preset not found"));
        return false;
    }

    const QString uniqueName = makeUniqueUserPresetName(name, presetId);
    if (uniqueName.isEmpty()) {
        setLastError(QStringLiteral("Preset name is empty"));
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

bool EqualizerPresetManager::deleteUserPreset(const QString &presetId)
{
    const int index = findUserPresetIndex(presetId);
    if (index < 0) {
        setLastError(QStringLiteral("User preset not found"));
        return false;
    }

    m_userPresets.removeAt(index);
    setLastError(QString());
    bumpRevisionAndNotify();
    return true;
}

void EqualizerPresetManager::clearUserPresets()
{
    if (m_userPresets.isEmpty()) {
        setLastError(QString());
        return;
    }

    m_userPresets.clear();
    m_nextUserId = 1;
    setLastError(QString());
    bumpRevisionAndNotify();
}

QVariantList EqualizerPresetManager::exportUserPresetsSnapshot() const
{
    return userPresets();
}

bool EqualizerPresetManager::replaceUserPresets(const QVariantList &serializedPresets)
{
    QList<Preset> parsed;
    parsed.reserve(serializedPresets.size());

    int nextUserNumericId = 1;
    for (int i = 0; i < serializedPresets.size(); ++i) {
        const QVariant value = serializedPresets.at(i);
        if (!value.canConvert<QVariantMap>()) {
            setLastError(QStringLiteral("Invalid user preset at index %1").arg(i));
            return false;
        }

        Preset preset;
        QString parseError;
        if (!parseSerializedPreset(value.toMap(), &preset, &parseError)) {
            setLastError(QStringLiteral("Invalid user preset at index %1: %2")
                             .arg(i)
                             .arg(parseError));
            return false;
        }

        if (preset.id.startsWith(QStringLiteral("builtin:"))) {
            setLastError(QStringLiteral("Built-in preset id cannot be used for user preset"));
            return false;
        }

        const QRegularExpressionMatch match = kUserIdPattern.match(preset.id);
        if (match.hasMatch()) {
            const int numericId = match.captured(1).toInt();
            nextUserNumericId = std::max(nextUserNumericId, numericId + 1);
        }

        parsed.push_back(preset);
    }

    m_userPresets = parsed;
    m_nextUserId = nextUserNumericId;
    setLastError(QString());
    bumpRevisionAndNotify();
    return true;
}

QVariantMap EqualizerPresetManager::exportPresetToJson(const QString &presetId) const
{
    QList<Preset> list;
    const int builtInIndex = findBuiltInPresetIndex(presetId);
    if (builtInIndex >= 0) {
        list.push_back(m_builtInPresets.at(builtInIndex));
        return exportPresetsAsBundleV1(list).toVariantMap();
    }

    const int userIndex = findUserPresetIndex(presetId);
    if (userIndex >= 0) {
        list.push_back(m_userPresets.at(userIndex));
        return exportPresetsAsBundleV1(list).toVariantMap();
    }

    ExportResult result;
    result.success = false;
    result.error = QStringLiteral("Preset not found");
    result.schema = kBundleSchema;
    return result.toVariantMap();
}

QVariantMap EqualizerPresetManager::exportUserPresetsToJson() const
{
    return exportPresetsAsBundleV1(m_userPresets).toVariantMap();
}

QVariantMap EqualizerPresetManager::exportBundleV1ToJson() const
{
    QList<Preset> bundle;
    bundle.reserve(m_builtInPresets.size() + m_userPresets.size());
    for (const Preset &preset : m_builtInPresets) {
        bundle.push_back(preset);
    }
    for (const Preset &preset : m_userPresets) {
        bundle.push_back(preset);
    }
    return exportPresetsAsBundleV1(bundle).toVariantMap();
}

QVariantMap EqualizerPresetManager::exportPresetToJsonFile(const QString &presetId,
                                                           const QString &filePath) const
{
    QVariantMap result = exportPresetToJson(presetId);
    if (!result.value(QStringLiteral("success")).toBool()) {
        return result;
    }

    QString error;
    if (!writeTextFileAtomically(filePath, result.value(QStringLiteral("json")).toString(), &error)) {
        result.insert(QStringLiteral("success"), false);
        result.insert(QStringLiteral("error"), error);
        return result;
    }

    result.insert(QStringLiteral("outputPath"), filePath);
    return result;
}

QVariantMap EqualizerPresetManager::exportUserPresetsToJsonFile(const QString &filePath) const
{
    QVariantMap result = exportUserPresetsToJson();
    if (!result.value(QStringLiteral("success")).toBool()) {
        return result;
    }

    QString error;
    if (!writeTextFileAtomically(filePath, result.value(QStringLiteral("json")).toString(), &error)) {
        result.insert(QStringLiteral("success"), false);
        result.insert(QStringLiteral("error"), error);
        return result;
    }

    result.insert(QStringLiteral("outputPath"), filePath);
    return result;
}

QVariantMap EqualizerPresetManager::exportBundleV1ToJsonFile(const QString &filePath) const
{
    QVariantMap result = exportBundleV1ToJson();
    if (!result.value(QStringLiteral("success")).toBool()) {
        return result;
    }

    QString error;
    if (!writeTextFileAtomically(filePath, result.value(QStringLiteral("json")).toString(), &error)) {
        result.insert(QStringLiteral("success"), false);
        result.insert(QStringLiteral("error"), error);
        return result;
    }

    result.insert(QStringLiteral("outputPath"), filePath);
    return result;
}

QVariantMap EqualizerPresetManager::importPresetsFromJson(const QString &jsonPayload,
                                                          const QString &mergePolicy)
{
    ImportResult result;
    result.mergePolicy = mergePolicy.trimmed().toLower();
    if (result.mergePolicy.isEmpty()) {
        result.mergePolicy = kMergePolicyKeepBoth;
    }

    if (!isValidMergePolicy(result.mergePolicy)) {
        result.errors.push_back(QStringLiteral("Unsupported merge policy: %1").arg(result.mergePolicy));
        result.success = false;
        setLastError(result.errors.constLast());
        return result.toVariantMap();
    }

    if (jsonPayload.trimmed().isEmpty()) {
        result.errors.push_back(QStringLiteral("JSON payload is empty"));
        result.success = false;
        setLastError(result.errors.constLast());
        return result.toVariantMap();
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(jsonPayload.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        result.errors.push_back(QStringLiteral("JSON parse error: %1 at offset %2")
                                .arg(parseError.errorString())
                                .arg(parseError.offset));
        result.success = false;
        setLastError(result.errors.constLast());
        return result.toVariantMap();
    }

    if (!document.isObject()) {
        result.errors.push_back(QStringLiteral("Invalid JSON root: object expected"));
        result.success = false;
        setLastError(result.errors.constLast());
        return result.toVariantMap();
    }

    const QJsonObject root = document.object();
    const QString schema = root.value(QStringLiteral("schema")).toString().trimmed();
    if (schema != kBundleSchema) {
        result.errors.push_back(QStringLiteral("Unsupported schema: %1").arg(schema));
        result.success = false;
        setLastError(result.errors.constLast());
        return result.toVariantMap();
    }

    if (root.contains(QStringLiteral("bandCount"))) {
        const int bandCount = root.value(QStringLiteral("bandCount")).toInt(-1);
        if (bandCount != kBandCount) {
            result.errors.push_back(QStringLiteral("Unsupported bandCount: %1").arg(bandCount));
            result.success = false;
            setLastError(result.errors.constLast());
            return result.toVariantMap();
        }
    }

    const QJsonValue presetsValue = root.value(QStringLiteral("presets"));
    if (!presetsValue.isArray()) {
        result.errors.push_back(QStringLiteral("Invalid JSON: 'presets' array is required"));
        result.success = false;
        setLastError(result.errors.constLast());
        return result.toVariantMap();
    }

    QList<Preset> mergedUserPresets = m_userPresets;
    int nextUserId = m_nextUserId;
    const QJsonArray presets = presetsValue.toArray();
    for (int i = 0; i < presets.size(); ++i) {
        const QJsonValue presetValue = presets.at(i);
        if (!presetValue.isObject()) {
            ++result.skippedCount;
            result.errors.push_back(QStringLiteral("Preset at index %1 is not an object").arg(i));
            continue;
        }

        Preset importedPreset;
        QString importError;
        if (!parsePresetFromJsonObject(presetValue.toObject(), &importedPreset, &importError)) {
            ++result.skippedCount;
            result.errors.push_back(QStringLiteral("Preset at index %1 rejected: %2")
                                    .arg(i)
                                    .arg(importError));
            continue;
        }

        importedPreset.builtIn = false;
        importedPreset.updatedAtMs = QDateTime::currentMSecsSinceEpoch();

        if (result.mergePolicy == kMergePolicyReplaceExisting) {
            int replaceIndex = -1;
            if (!importedPreset.id.trimmed().isEmpty()) {
                for (int j = 0; j < mergedUserPresets.size(); ++j) {
                    if (mergedUserPresets.at(j).id == importedPreset.id) {
                        replaceIndex = j;
                        break;
                    }
                }
            }
            if (replaceIndex < 0) {
                replaceIndex = findUserPresetIndexByNameCaseInsensitive(mergedUserPresets, importedPreset.name);
            }

            if (replaceIndex >= 0) {
                Preset &existing = mergedUserPresets[replaceIndex];
                existing.name = importedPreset.name;
                existing.gainsDb = importedPreset.gainsDb;
                existing.updatedAtMs = importedPreset.updatedAtMs;
                ++result.replacedCount;
                continue;
            }
        }

        importedPreset.name = makeUniqueUserPresetNameInList(mergedUserPresets, importedPreset.name);
        importedPreset.id = makeNextUserPresetIdInList(mergedUserPresets, &nextUserId);
        mergedUserPresets.push_back(importedPreset);
        ++result.importedCount;
    }

    const bool changed = (mergedUserPresets != m_userPresets);
    if (changed) {
        m_userPresets = mergedUserPresets;
        m_nextUserId = nextUserId;
        bumpRevisionAndNotify();
    }

    result.success = (result.errors.isEmpty()
                      || (result.importedCount + result.replacedCount) > 0);
    setLastError(result.errors.isEmpty() ? QString() : result.errors.constLast());
    return result.toVariantMap();
}

QVariantMap EqualizerPresetManager::importPresetsFromJsonFile(const QString &filePath,
                                                              const QString &mergePolicy)
{
    QString payload;
    QString error;
    if (!readTextFile(filePath, &payload, &error)) {
        ImportResult result;
        result.success = false;
        result.mergePolicy = mergePolicy.trimmed().toLower();
        result.errors.push_back(error);
        setLastError(error);
        QVariantMap map = result.toVariantMap();
        map.insert(QStringLiteral("inputPath"), filePath);
        return map;
    }

    QVariantMap map = importPresetsFromJson(payload, mergePolicy);
    map.insert(QStringLiteral("inputPath"), filePath);
    return map;
}

EqualizerPresetManager::Preset EqualizerPresetManager::makeBuiltInPreset(const QString &id,
                                                                          const QString &name,
                                                                          const QVariantList &gainsDb)
{
    Preset preset;
    preset.id = id;
    preset.name = name;
    preset.gainsDb = normalizeGains(gainsDb);
    preset.builtIn = true;
    preset.updatedAtMs = 0;
    return preset;
}

QVariantList EqualizerPresetManager::normalizeGains(const QVariantList &gainsDb)
{
    QVariantList normalized;
    normalized.reserve(kBandCount);
    for (int i = 0; i < kBandCount; ++i) {
        const double source = (i < gainsDb.size()) ? gainsDb.at(i).toDouble() : 0.0;
        const double clamped = qBound(kMinGainDb, source, kMaxGainDb);
        normalized.push_back(std::round(clamped * 10.0) / 10.0);
    }
    return normalized;
}

QVariantMap EqualizerPresetManager::presetToVariantMap(const Preset &preset)
{
    QVariantMap map;
    map.insert(QStringLiteral("id"), preset.id);
    map.insert(QStringLiteral("name"), preset.name);
    map.insert(QStringLiteral("gains"), preset.gainsDb);
    map.insert(QStringLiteral("builtIn"), preset.builtIn);
    map.insert(QStringLiteral("updatedAtMs"), preset.updatedAtMs);
    return map;
}

bool EqualizerPresetManager::parseSerializedPreset(const QVariantMap &serialized,
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
    const QVariantList gains = serialized.value(QStringLiteral("gains")).toList();

    if (id.isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Missing id");
        }
        return false;
    }

    if (name.isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Missing name");
        }
        return false;
    }

    presetOut->id = id;
    presetOut->name = name;
    presetOut->gainsDb = normalizeGains(gains);
    presetOut->builtIn = false;
    presetOut->updatedAtMs = serialized.value(QStringLiteral("updatedAtMs")).toLongLong();
    return true;
}

int EqualizerPresetManager::findBuiltInPresetIndex(const QString &presetId) const
{
    const QString trimmed = presetId.trimmed();
    for (int i = 0; i < m_builtInPresets.size(); ++i) {
        if (m_builtInPresets.at(i).id == trimmed) {
            return i;
        }
    }
    return -1;
}

int EqualizerPresetManager::findUserPresetIndex(const QString &presetId) const
{
    const QString trimmed = presetId.trimmed();
    for (int i = 0; i < m_userPresets.size(); ++i) {
        if (m_userPresets.at(i).id == trimmed) {
            return i;
        }
    }
    return -1;
}

int EqualizerPresetManager::findUserPresetIndexByNameCaseInsensitive(const QList<Preset> &presets,
                                                                      const QString &name) const
{
    const QString normalized = sanitizeName(name);
    if (normalized.isEmpty()) {
        return -1;
    }

    for (int i = 0; i < presets.size(); ++i) {
        if (presets.at(i).name.compare(normalized, Qt::CaseInsensitive) == 0) {
            return i;
        }
    }
    return -1;
}

bool EqualizerPresetManager::isPresetNameUsedCaseInsensitive(const QList<Preset> &presets,
                                                             const QString &candidateName,
                                                             const QString &ignorePresetId) const
{
    const QString normalized = sanitizeName(candidateName);
    if (normalized.isEmpty()) {
        return false;
    }

    for (const Preset &preset : presets) {
        if (!ignorePresetId.isEmpty() && preset.id == ignorePresetId) {
            continue;
        }
        if (preset.name.compare(normalized, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

QString EqualizerPresetManager::makeNextUserPresetId()
{
    const QString id = QStringLiteral("user:%1").arg(m_nextUserId);
    ++m_nextUserId;
    return id;
}

QString EqualizerPresetManager::makeNextUserPresetIdInList(const QList<Preset> &presets, int *nextId) const
{
    int localNextId = (nextId && *nextId > 0) ? *nextId : 1;
    while (true) {
        const QString candidate = QStringLiteral("user:%1").arg(localNextId++);
        bool exists = false;
        for (const Preset &preset : presets) {
            if (preset.id == candidate) {
                exists = true;
                break;
            }
        }
        if (exists || findBuiltInPresetIndex(candidate) >= 0) {
            continue;
        }
        if (nextId) {
            *nextId = localNextId;
        }
        return candidate;
    }
}

QString EqualizerPresetManager::makeUniqueUserPresetName(const QString &requestedName,
                                                         const QString &ignorePresetId) const
{
    const QString baseName = sanitizeName(requestedName);
    if (baseName.isEmpty()) {
        return {};
    }

    auto nameExists = [this, &ignorePresetId](const QString &candidate) {
        for (const Preset &preset : m_userPresets) {
            if (!ignorePresetId.isEmpty() && preset.id == ignorePresetId) {
                continue;
            }
            if (preset.name.compare(candidate, Qt::CaseInsensitive) == 0) {
                return true;
            }
        }
        return false;
    };

    if (!nameExists(baseName)) {
        return baseName;
    }

    int suffix = 2;
    while (true) {
        const QString candidate = QStringLiteral("%1 (%2)").arg(baseName).arg(suffix);
        if (!nameExists(candidate)) {
            return candidate;
        }
        ++suffix;
    }
}

QString EqualizerPresetManager::makeUniqueUserPresetNameInList(const QList<Preset> &presets,
                                                               const QString &requestedName,
                                                               const QString &ignorePresetId) const
{
    const QString baseName = sanitizeName(requestedName);
    if (baseName.isEmpty()) {
        return {};
    }

    if (!isPresetNameUsedCaseInsensitive(presets, baseName, ignorePresetId)) {
        return baseName;
    }

    QString importedBase = baseName + QStringLiteral(" (imported)");
    if (!isPresetNameUsedCaseInsensitive(presets, importedBase, ignorePresetId)) {
        return importedBase;
    }

    int suffix = 2;
    while (true) {
        const QString candidate = QStringLiteral("%1 (%2)").arg(importedBase).arg(suffix);
        if (!isPresetNameUsedCaseInsensitive(presets, candidate, ignorePresetId)) {
            return candidate;
        }
        ++suffix;
    }
}

QJsonObject EqualizerPresetManager::presetToJsonObject(const Preset &preset)
{
    QJsonObject object;
    object.insert(QStringLiteral("id"), preset.id);
    object.insert(QStringLiteral("name"), preset.name);
    object.insert(QStringLiteral("builtIn"), preset.builtIn);
    object.insert(QStringLiteral("updatedAt"), QDateTime::fromMSecsSinceEpoch(preset.updatedAtMs,
                                                                               QTimeZone::UTC)
                      .toString(Qt::ISODate));

    QJsonArray gainsArray;
    for (const QVariant &gain : preset.gainsDb) {
        gainsArray.push_back(gain.toDouble());
    }
    object.insert(QStringLiteral("gains"), gainsArray);
    return object;
}

bool EqualizerPresetManager::parsePresetFromJsonObject(const QJsonObject &object,
                                                       Preset *presetOut,
                                                       QString *errorOut)
{
    if (!presetOut) {
        if (errorOut) {
            *errorOut = QStringLiteral("Output preset is null");
        }
        return false;
    }

    const QString name = sanitizeName(object.value(QStringLiteral("name")).toString());
    if (name.isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Missing preset name");
        }
        return false;
    }

    const QJsonValue gainsValue = object.value(QStringLiteral("gains"));
    if (!gainsValue.isArray()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Missing gains array");
        }
        return false;
    }

    QVariantList gains;
    const QJsonArray gainsArray = gainsValue.toArray();
    gains.reserve(gainsArray.size());
    for (const QJsonValue &gainValue : gainsArray) {
        gains.push_back(gainValue.toDouble());
    }

    presetOut->id = object.value(QStringLiteral("id")).toString().trimmed();
    presetOut->name = name;
    presetOut->gainsDb = normalizeGains(gains);
    presetOut->builtIn = false;
    presetOut->updatedAtMs = QDateTime::currentMSecsSinceEpoch();
    return true;
}

bool EqualizerPresetManager::writeTextFileAtomically(const QString &filePath,
                                                     const QString &utf8Text,
                                                     QString *errorOut)
{
    const QString normalizedPath = filePath.trimmed();
    if (normalizedPath.isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Output file path is empty");
        }
        return false;
    }

    QSaveFile file(normalizedPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Failed to open output file: %1").arg(file.errorString());
        }
        return false;
    }

    const QByteArray bytes = utf8Text.toUtf8();
    if (file.write(bytes) != bytes.size()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Failed to write output file");
        }
        file.cancelWriting();
        return false;
    }

    if (!file.commit()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Failed to commit output file: %1").arg(file.errorString());
        }
        return false;
    }

    return true;
}

bool EqualizerPresetManager::readTextFile(const QString &filePath,
                                          QString *utf8TextOut,
                                          QString *errorOut)
{
    if (!utf8TextOut) {
        if (errorOut) {
            *errorOut = QStringLiteral("Output buffer is null");
        }
        return false;
    }

    const QString normalizedPath = filePath.trimmed();
    if (normalizedPath.isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Input file path is empty");
        }
        return false;
    }

    QFile file(normalizedPath);
    if (!file.exists()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Input file does not exist");
        }
        return false;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Failed to open input file: %1").arg(file.errorString());
        }
        return false;
    }

    *utf8TextOut = QString::fromUtf8(file.readAll());
    return true;
}

EqualizerPresetManager::ExportResult EqualizerPresetManager::exportPresetsAsBundleV1(
    const QList<Preset> &presets) const
{
    ExportResult result;
    result.schema = kBundleSchema;

    QJsonObject root;
    root.insert(QStringLiteral("schema"), kBundleSchema);
    root.insert(QStringLiteral("app"), QStringLiteral("WaveFlux"));
    root.insert(QStringLiteral("version"), QStringLiteral("1.1.0"));
    root.insert(QStringLiteral("exportedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    root.insert(QStringLiteral("bandCount"), kBandCount);

    QJsonObject gainRange;
    gainRange.insert(QStringLiteral("min"), kMinGainDb);
    gainRange.insert(QStringLiteral("max"), kMaxGainDb);
    root.insert(QStringLiteral("gainRange"), gainRange);

    QJsonArray presetsArray;
    for (const Preset &preset : presets) {
        presetsArray.push_back(presetToJsonObject(preset));
    }
    root.insert(QStringLiteral("presets"), presetsArray);

    result.exportedCount = presets.size();
    result.success = true;
    result.jsonPayload = QString::fromUtf8(
        QJsonDocument(root).toJson(QJsonDocument::Indented));
    return result;
}

bool EqualizerPresetManager::isValidMergePolicy(const QString &mergePolicy)
{
    const QString normalized = mergePolicy.trimmed().toLower();
    return normalized == kMergePolicyKeepBoth
            || normalized == kMergePolicyReplaceExisting;
}

QString EqualizerPresetManager::sanitizeName(const QString &name)
{
    return name.trimmed();
}

void EqualizerPresetManager::setLastError(const QString &error)
{
    if (m_lastError == error) {
        return;
    }

    m_lastError = error;
    emit lastErrorChanged();
}

void EqualizerPresetManager::bumpRevisionAndNotify()
{
    ++m_revision;
    emit presetsChanged();
}
