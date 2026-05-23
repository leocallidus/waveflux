#include "ShortcutManager.h"

#include <QKeySequence>
#include <QSet>

namespace {
constexpr int kShortcutsSettingsVersion = 1;

QString trimmedId(const QString &id)
{
    return id.trimmed();
}
}

ShortcutManager::ShortcutManager(QObject *parent)
    : QObject(parent)
    , m_settings(QStringLiteral("WaveFlux"), QStringLiteral("WaveFlux"))
    , m_definitions(ShortcutRegistry::definitions())
{
    for (const ShortcutDefinition &definition : std::as_const(m_definitions)) {
        m_definitionById.insert(definition.id, definition);
    }
    loadOverrides();
}

QVariantList ShortcutManager::shortcutDefinitions() const
{
    QVariantList rows;
    rows.reserve(m_definitions.size());
    for (const ShortcutDefinition &definition : m_definitions) {
        rows.push_back(definitionToVariantMap(definition));
    }
    return rows;
}

QVariantList ShortcutManager::shortcutRows() const
{
    QVariantList rows;
    rows.reserve(m_definitions.size());
    for (const ShortcutDefinition &definition : m_definitions) {
        QVariantMap row = definitionToVariantMap(definition);
        const QString custom = customSequence(definition.id);
        const QString effective = effectiveSequenceForDefinition(definition);
        row.insert(QStringLiteral("customSequence"), custom);
        row.insert(QStringLiteral("effectiveSequence"), effective);
        row.insert(QStringLiteral("displaySequence"), displaySequenceText(effective));
        row.insert(QStringLiteral("defaultDisplaySequence"), displaySequenceText(definition.defaultSequence));
        row.insert(QStringLiteral("hasCustom"), m_overrides.contains(definition.id));
        row.insert(QStringLiteral("enabled"), !effective.isEmpty());
        rows.push_back(row);
    }
    return rows;
}

bool ShortcutManager::hasShortcut(const QString &id) const
{
    return definitionForId(id) != nullptr;
}

QString ShortcutManager::defaultSequence(const QString &id) const
{
    const ShortcutDefinition *definition = definitionForId(id);
    return definition ? definition->defaultSequence : QString();
}

QString ShortcutManager::customSequence(const QString &id) const
{
    const QString normalizedId = trimmedId(id);
    return m_overrides.value(normalizedId);
}

QString ShortcutManager::effectiveSequence(const QString &id) const
{
    const ShortcutDefinition *definition = definitionForId(id);
    return definition ? effectiveSequenceForDefinition(*definition) : QString();
}

QString ShortcutManager::displaySequence(const QString &id) const
{
    return displaySequenceText(effectiveSequence(id));
}

QString ShortcutManager::displayDefaultSequence(const QString &id) const
{
    return displaySequenceText(defaultSequence(id));
}

bool ShortcutManager::shortcutEnabled(const QString &id) const
{
    return !effectiveSequence(id).isEmpty();
}

QVariantMap ShortcutManager::validateSequence(const QString &id, const QString &sequence) const
{
    QVariantMap result;
    result.insert(QStringLiteral("ok"), false);
    result.insert(QStringLiteral("id"), trimmedId(id));

    const ShortcutDefinition *definition = definitionForId(id);
    if (!definition) {
        result.insert(QStringLiteral("reason"), QStringLiteral("unknown-id"));
        return result;
    }

    result.insert(QStringLiteral("userAssignable"), definition->userAssignable);
    result.insert(QStringLiteral("allowEmpty"), definition->allowEmpty);

    if (!definition->userAssignable) {
        result.insert(QStringLiteral("reason"), QStringLiteral("not-assignable"));
        return result;
    }

    const QString normalized = normalizeSequence(sequence);
    result.insert(QStringLiteral("normalizedSequence"), normalized);
    result.insert(QStringLiteral("displaySequence"), displaySequenceText(normalized));

    if (sequence.trimmed().isEmpty()) {
        if (!definition->allowEmpty) {
            result.insert(QStringLiteral("reason"), QStringLiteral("empty-not-allowed"));
            return result;
        }
        result.insert(QStringLiteral("ok"), true);
        result.insert(QStringLiteral("reason"), QStringLiteral("ok"));
        result.insert(QStringLiteral("conflicts"), QVariantList());
        return result;
    }

    if (normalized.isEmpty()) {
        result.insert(QStringLiteral("reason"), QStringLiteral("invalid-sequence"));
        return result;
    }

    if (definition->id == QStringLiteral("playback.spaceTapHold")
        && sequenceKeyCount(normalized) != 1) {
        result.insert(QStringLiteral("reason"), QStringLiteral("invalid-sequence"));
        return result;
    }

    if (definition->reservedSequences.contains(normalized, Qt::CaseInsensitive)) {
        result.insert(QStringLiteral("reason"), QStringLiteral("reserved-sequence"));
        return result;
    }

    const QVariantList conflicts = conflictsForSequence(definition->id, normalized);
    result.insert(QStringLiteral("conflicts"), conflicts);
    result.insert(QStringLiteral("hasConflicts"), !conflicts.isEmpty());
    result.insert(QStringLiteral("ok"), true);
    result.insert(QStringLiteral("reason"), QStringLiteral("ok"));
    return result;
}

QVariantMap ShortcutManager::conflictReportForSequence(const QString &id, const QString &sequence) const
{
    QVariantMap report;
    report.insert(QStringLiteral("ok"), false);
    report.insert(QStringLiteral("id"), trimmedId(id));

    const ShortcutDefinition *definition = definitionForId(id);
    if (!definition) {
        report.insert(QStringLiteral("reason"), QStringLiteral("unknown-id"));
        report.insert(QStringLiteral("conflicts"), QVariantList());
        report.insert(QStringLiteral("hasConflicts"), false);
        report.insert(QStringLiteral("canReplaceAll"), false);
        return report;
    }

    const QVariantMap validation = validateSequence(definition->id, sequence);
    report.insert(QStringLiteral("validation"), validation);
    if (!validation.value(QStringLiteral("ok")).toBool()) {
        report.insert(QStringLiteral("reason"), validation.value(QStringLiteral("reason")).toString());
        report.insert(QStringLiteral("conflicts"), QVariantList());
        report.insert(QStringLiteral("hasConflicts"), false);
        report.insert(QStringLiteral("canReplaceAll"), false);
        return report;
    }

    const QVariantList conflicts = validation.value(QStringLiteral("conflicts")).toList();
    bool canReplaceAll = true;
    for (const QVariant &conflictVariant : conflicts) {
        const QVariantMap conflict = conflictVariant.toMap();
        if (!conflict.value(QStringLiteral("canReplace")).toBool()) {
            canReplaceAll = false;
            break;
        }
    }

    report.insert(QStringLiteral("ok"), true);
    report.insert(QStringLiteral("reason"), QStringLiteral("ok"));
    report.insert(QStringLiteral("target"), definitionToVariantMap(*definition));
    report.insert(QStringLiteral("normalizedSequence"),
                  validation.value(QStringLiteral("normalizedSequence")).toString());
    report.insert(QStringLiteral("displaySequence"),
                  validation.value(QStringLiteral("displaySequence")).toString());
    report.insert(QStringLiteral("conflicts"), conflicts);
    report.insert(QStringLiteral("hasConflicts"), !conflicts.isEmpty());
    report.insert(QStringLiteral("canReplaceAll"), canReplaceAll);
    return report;
}

QVariantList ShortcutManager::conflictsForSequence(const QString &id, const QString &sequence) const
{
    QVariantList conflicts;
    const ShortcutDefinition *definition = definitionForId(id);
    if (!definition) {
        return conflicts;
    }

    const QString normalized = normalizeSequence(sequence);
    if (normalized.isEmpty()) {
        return conflicts;
    }

    for (const ShortcutDefinition &candidate : m_definitions) {
        if (candidate.id == definition->id) {
            continue;
        }
        if (!contextsConflict(definition->context, candidate.context)) {
            continue;
        }

        const QString candidateSequence = normalizeSequence(effectiveSequenceForDefinition(candidate));
        if (candidateSequence.compare(normalized, Qt::CaseInsensitive) == 0) {
            conflicts.push_back(conflictToVariantMap(*definition, candidate));
        }
    }

    return conflicts;
}

QVariantMap ShortcutManager::setCustomSequenceResolvingConflicts(const QString &id,
                                                                 const QString &sequence,
                                                                 bool replaceConflicts)
{
    QVariantMap result;
    result.insert(QStringLiteral("ok"), false);
    result.insert(QStringLiteral("id"), trimmedId(id));

    const QVariantMap report = conflictReportForSequence(id, sequence);
    result.insert(QStringLiteral("conflictReport"), report);
    if (!report.value(QStringLiteral("ok")).toBool()) {
        result.insert(QStringLiteral("reason"), report.value(QStringLiteral("reason")).toString());
        setLastError(result.value(QStringLiteral("reason")).toString());
        return result;
    }

    const bool hasConflicts = report.value(QStringLiteral("hasConflicts")).toBool();
    if (hasConflicts && !replaceConflicts) {
        result.insert(QStringLiteral("reason"), QStringLiteral("conflict"));
        setLastError(QStringLiteral("conflict"));
        return result;
    }

    if (hasConflicts && !report.value(QStringLiteral("canReplaceAll")).toBool()) {
        result.insert(QStringLiteral("reason"), QStringLiteral("non-replaceable-conflict"));
        setLastError(QStringLiteral("non-replaceable-conflict"));
        return result;
    }

    QVariantList replaced;
    if (hasConflicts) {
        const QVariantList conflicts = report.value(QStringLiteral("conflicts")).toList();
        for (const QVariant &conflictVariant : conflicts) {
            const QString conflictId = conflictVariant.toMap().value(QStringLiteral("id")).toString();
            const ShortcutDefinition *conflictDefinition = definitionForId(conflictId);
            if (!conflictDefinition || !clearConflictForReplacement(*conflictDefinition)) {
                result.insert(QStringLiteral("reason"), QStringLiteral("replace-failed"));
                setLastError(QStringLiteral("replace-failed"));
                return result;
            }
            replaced.push_back(conflictToVariantMap(*definitionForId(trimmedId(id)), *conflictDefinition));
        }
    }

    if (!setCustomSequence(id, sequence)) {
        result.insert(QStringLiteral("reason"), lastError());
        return result;
    }

    result.insert(QStringLiteral("ok"), true);
    result.insert(QStringLiteral("reason"), QStringLiteral("ok"));
    result.insert(QStringLiteral("replacedConflicts"), replaced);
    result.insert(QStringLiteral("replacedCount"), replaced.size());
    return result;
}

bool ShortcutManager::setCustomSequence(const QString &id, const QString &sequence)
{
    const QString normalizedId = trimmedId(id);
    const QVariantMap validation = validateSequence(normalizedId, sequence);
    if (!validation.value(QStringLiteral("ok")).toBool()) {
        setLastError(validation.value(QStringLiteral("reason")).toString());
        return false;
    }

    const QString normalized = validation.value(QStringLiteral("normalizedSequence")).toString();
    const QString current = m_overrides.value(normalizedId);
    if (m_overrides.contains(normalizedId) && current == normalized) {
        setLastError(QString());
        return true;
    }

    m_overrides.insert(normalizedId, normalized);
    saveOverrides();
    setLastError(QString());
    markChanged();
    return true;
}

bool ShortcutManager::clearCustomSequence(const QString &id)
{
    return setCustomSequence(id, QString());
}

bool ShortcutManager::resetShortcut(const QString &id)
{
    const QString normalizedId = trimmedId(id);
    if (!definitionForId(normalizedId)) {
        setLastError(QStringLiteral("unknown-id"));
        return false;
    }

    if (!m_overrides.remove(normalizedId)) {
        setLastError(QString());
        return true;
    }

    saveOverrides();
    setLastError(QString());
    markChanged();
    return true;
}

bool ShortcutManager::resetGroup(const QString &group)
{
    const QString normalizedGroup = group.trimmed();
    bool knownGroup = false;
    bool changed = false;
    for (const ShortcutDefinition &definition : m_definitions) {
        if (definition.group != normalizedGroup) {
            continue;
        }
        knownGroup = true;
        changed = m_overrides.remove(definition.id) > 0 || changed;
    }

    if (!knownGroup) {
        setLastError(QStringLiteral("unknown-group"));
        return false;
    }

    if (changed) {
        saveOverrides();
        markChanged();
    }
    setLastError(QString());
    return true;
}

bool ShortcutManager::resetAll()
{
    if (m_overrides.isEmpty()) {
        setLastError(QString());
        return true;
    }

    m_overrides.clear();
    saveOverrides();
    setLastError(QString());
    markChanged();
    return true;
}

QString ShortcutManager::normalizeSequence(const QString &sequence)
{
    const QString trimmed = sequence.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }

    QKeySequence keySequence = QKeySequence::fromString(trimmed, QKeySequence::PortableText);
    if (keySequence.isEmpty()) {
        keySequence = QKeySequence::fromString(trimmed, QKeySequence::NativeText);
    }
    if (keySequence.isEmpty()) {
        return QString();
    }

    return keySequence.toString(QKeySequence::PortableText);
}

int ShortcutManager::sequenceKeyCount(const QString &sequence)
{
    const QString trimmed = sequence.trimmed();
    if (trimmed.isEmpty()) {
        return 0;
    }

    QKeySequence keySequence = QKeySequence::fromString(trimmed, QKeySequence::PortableText);
    if (keySequence.isEmpty()) {
        keySequence = QKeySequence::fromString(trimmed, QKeySequence::NativeText);
    }
    return keySequence.count();
}

QString ShortcutManager::displaySequenceText(const QString &sequence)
{
    const QString normalized = normalizeSequence(sequence);
    if (normalized.isEmpty()) {
        return QString();
    }

    const QKeySequence keySequence = QKeySequence::fromString(normalized, QKeySequence::PortableText);
    return keySequence.toString(QKeySequence::NativeText);
}

QVariantMap ShortcutManager::definitionToVariantMap(const ShortcutDefinition &definition)
{
    QVariantMap row;
    row.insert(QStringLiteral("id"), definition.id);
    row.insert(QStringLiteral("translationKey"), definition.translationKey);
    row.insert(QStringLiteral("group"), definition.group);
    row.insert(QStringLiteral("context"), definition.context);
    row.insert(QStringLiteral("defaultSequence"), definition.defaultSequence);
    row.insert(QStringLiteral("userAssignable"), definition.userAssignable);
    row.insert(QStringLiteral("allowEmpty"), definition.allowEmpty);
    row.insert(QStringLiteral("reservedSequences"), definition.reservedSequences);
    row.insert(QStringLiteral("source"), definition.source);
    row.insert(QStringLiteral("notes"), definition.notes);
    return row;
}

bool ShortcutManager::contextsConflict(const QString &left, const QString &right)
{
    return !contextConflictReason(left, right).isEmpty();
}

QString ShortcutManager::contextConflictReason(const QString &left, const QString &right)
{
    if (left == right) {
        return QStringLiteral("same-context");
    }

    if (left == QStringLiteral("dialog") || right == QStringLiteral("dialog")) {
        return QString();
    }

    if (left == QStringLiteral("application") || right == QStringLiteral("application")) {
        return QStringLiteral("application-global");
    }

    if ((left == QStringLiteral("normal-skin") && right == QStringLiteral("compact-skin"))
        || (left == QStringLiteral("compact-skin") && right == QStringLiteral("normal-skin"))) {
        return QString();
    }

    const QSet<QString> windowContexts{QStringLiteral("window"),
                                       QStringLiteral("playlist"),
                                       QStringLiteral("normal-skin"),
                                       QStringLiteral("compact-skin")};

    if (windowContexts.contains(left) && windowContexts.contains(right)) {
        if (left == QStringLiteral("playlist") || right == QStringLiteral("playlist")) {
            return QStringLiteral("playlist-focus-overlap");
        }
        return QStringLiteral("window-overlap");
    }

    return QString();
}

const ShortcutDefinition *ShortcutManager::definitionForId(const QString &id) const
{
    const auto it = m_definitionById.constFind(trimmedId(id));
    if (it == m_definitionById.constEnd()) {
        return nullptr;
    }
    return &it.value();
}

QString ShortcutManager::effectiveSequenceForDefinition(const ShortcutDefinition &definition) const
{
    if (m_overrides.contains(definition.id)) {
        return m_overrides.value(definition.id);
    }
    return definition.defaultSequence;
}

QVariantMap ShortcutManager::conflictToVariantMap(const ShortcutDefinition &target,
                                                  const ShortcutDefinition &definition) const
{
    QVariantMap row = definitionToVariantMap(definition);
    const QString effective = effectiveSequenceForDefinition(definition);
    row.insert(QStringLiteral("effectiveSequence"), effective);
    row.insert(QStringLiteral("displaySequence"), displaySequenceText(effective));
    row.insert(QStringLiteral("targetId"), target.id);
    row.insert(QStringLiteral("contextConflictReason"),
               contextConflictReason(target.context, definition.context));
    row.insert(QStringLiteral("canReplace"), definition.userAssignable);
    row.insert(QStringLiteral("replacementAction"),
               definition.allowEmpty ? QStringLiteral("clear") : QStringLiteral("reset"));
    row.insert(QStringLiteral("blockingReason"),
               definition.userAssignable ? QString() : QStringLiteral("not-assignable"));
    return row;
}

bool ShortcutManager::clearConflictForReplacement(const ShortcutDefinition &definition)
{
    if (!definition.userAssignable) {
        return false;
    }

    if (definition.allowEmpty) {
        m_overrides.insert(definition.id, QString());
    } else {
        m_overrides.remove(definition.id);
    }
    return true;
}

void ShortcutManager::loadOverrides()
{
    m_overrides.clear();

    m_settings.beginGroup(QStringLiteral("App"));
    const bool hasVersion = m_settings.contains(QStringLiteral("shortcuts.version"));
    const int version = m_settings.value(QStringLiteral("shortcuts.version"), 0).toInt();
    const QVariantMap stored = m_settings.value(QStringLiteral("shortcuts.overrides"), QVariantMap()).toMap();
    m_settings.endGroup();

    if (!hasVersion) {
        return;
    }

    if (version > kShortcutsSettingsVersion) {
        return;
    }

    for (auto it = stored.constBegin(); it != stored.constEnd(); ++it) {
        const QString id = trimmedId(it.key());
        const ShortcutDefinition *definition = definitionForId(id);
        if (!definition || !definition->userAssignable) {
            continue;
        }

        const QString rawSequence = it.value().toString();
        if (rawSequence.trimmed().isEmpty()) {
            if (definition->allowEmpty) {
                m_overrides.insert(id, QString());
            }
            continue;
        }

        const QString normalized = normalizeSequence(rawSequence);
        if (!normalized.isEmpty()
            && (definition->id != QStringLiteral("playback.spaceTapHold")
                || sequenceKeyCount(normalized) == 1)
            && !definition->reservedSequences.contains(normalized, Qt::CaseInsensitive)) {
            m_overrides.insert(id, normalized);
        }
    }
}

void ShortcutManager::saveOverrides()
{
    QVariantMap stored;
    for (auto it = m_overrides.constBegin(); it != m_overrides.constEnd(); ++it) {
        stored.insert(it.key(), it.value());
    }

    m_settings.beginGroup(QStringLiteral("App"));
    m_settings.setValue(QStringLiteral("shortcuts.version"), kShortcutsSettingsVersion);
    m_settings.setValue(QStringLiteral("shortcuts.overrides"), stored);
    m_settings.endGroup();
    m_settings.sync();
}

void ShortcutManager::setLastError(const QString &error)
{
    if (m_lastError == error) {
        return;
    }
    m_lastError = error;
    emit lastErrorChanged();
}

void ShortcutManager::markChanged()
{
    ++m_revision;
    emit shortcutsChanged();
}
