#include "library/SmartCollectionsEngine.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QPair>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>

namespace {
enum class FieldKind {
    Text,
    Number,
    Boolean
};

struct FieldSpec {
    QString expression;
    FieldKind kind = FieldKind::Text;
    bool sortable = true;
    bool usesNowMs = false;
};

QString normalizePath(const QString &path)
{
    return QDir::cleanPath(QDir::fromNativeSeparators(path.trimmed()));
}

FieldSpec fieldSpecByName(const QString &fieldName)
{
    const QString field = fieldName.trimmed().toLower();
    if (field == QStringLiteral("title")) {
        return {QStringLiteral("COALESCE(t.title, '')"), FieldKind::Text, true, false};
    }
    if (field == QStringLiteral("artist")) {
        return {QStringLiteral("COALESCE(t.artist, '')"), FieldKind::Text, true, false};
    }
    if (field == QStringLiteral("album")) {
        return {QStringLiteral("COALESCE(t.album, '')"), FieldKind::Text, true, false};
    }
    if (field == QStringLiteral("path")) {
        return {QStringLiteral("COALESCE(t.canonical_path, '')"), FieldKind::Text, true, false};
    }
    if (field == QStringLiteral("format")) {
        return {QStringLiteral("COALESCE(t.format, '')"), FieldKind::Text, true, false};
    }
    if (field == QStringLiteral("bit_depth")) {
        return {QStringLiteral("COALESCE(t.bit_depth, 0)"), FieldKind::Number, true, false};
    }
    if (field == QStringLiteral("sample_rate")) {
        return {QStringLiteral("COALESCE(t.sample_rate, 0)"), FieldKind::Number, true, false};
    }
    if (field == QStringLiteral("duration_ms")) {
        return {QStringLiteral("COALESCE(t.duration_ms, 0)"), FieldKind::Number, true, false};
    }
    if (field == QStringLiteral("added_at_ms")) {
        return {QStringLiteral("COALESCE(t.added_at_ms, 0)"), FieldKind::Number, true, false};
    }
    if (field == QStringLiteral("added_days_ago")) {
        return {QStringLiteral("CAST((:now_ms - COALESCE(t.added_at_ms, 0)) / 86400000 AS INTEGER)"),
                FieldKind::Number,
                true,
                true};
    }
    if (field == QStringLiteral("play_count")) {
        return {QStringLiteral("COALESCE(s.play_count, 0)"), FieldKind::Number, true, false};
    }
    if (field == QStringLiteral("skip_count")) {
        return {QStringLiteral("COALESCE(s.skip_count, 0)"), FieldKind::Number, true, false};
    }
    if (field == QStringLiteral("completion_count")) {
        return {QStringLiteral("COALESCE(s.completion_count, 0)"), FieldKind::Number, true, false};
    }
    if (field == QStringLiteral("total_listen_ms")) {
        return {QStringLiteral("COALESCE(s.total_listen_ms, 0)"), FieldKind::Number, true, false};
    }
    if (field == QStringLiteral("last_played_at_ms")) {
        return {QStringLiteral("COALESCE(s.last_played_at_ms, 0)"), FieldKind::Number, true, false};
    }
    if (field == QStringLiteral("last_played_days_ago")) {
        return {QStringLiteral(
                    "CASE WHEN s.last_played_at_ms IS NULL THEN 1000000000 "
                    "ELSE CAST((:now_ms - s.last_played_at_ms) / 86400000 AS INTEGER) END"),
                FieldKind::Number,
                true,
                true};
    }
    if (field == QStringLiteral("favorite")) {
        return {QStringLiteral("COALESCE(s.favorite, 0)"), FieldKind::Boolean, true, false};
    }
    if (field == QStringLiteral("rating")) {
        return {QStringLiteral("COALESCE(s.rating, 0)"), FieldKind::Number, true, false};
    }

    return {};
}

QString sortableExpressionByField(const QString &fieldName)
{
    const QString field = fieldName.trimmed().toLower();
    if (field == QStringLiteral("title")) {
        return QStringLiteral("LOWER(COALESCE(t.title, ''))");
    }
    if (field == QStringLiteral("artist")) {
        return QStringLiteral("LOWER(COALESCE(t.artist, ''))");
    }
    if (field == QStringLiteral("album")) {
        return QStringLiteral("LOWER(COALESCE(t.album, ''))");
    }
    if (field == QStringLiteral("path")) {
        return QStringLiteral("LOWER(COALESCE(t.canonical_path, ''))");
    }
    if (field == QStringLiteral("format")) {
        return QStringLiteral("LOWER(COALESCE(t.format, ''))");
    }
    if (field == QStringLiteral("bit_depth")) {
        return QStringLiteral("COALESCE(t.bit_depth, 0)");
    }
    if (field == QStringLiteral("sample_rate")) {
        return QStringLiteral("COALESCE(t.sample_rate, 0)");
    }
    if (field == QStringLiteral("duration_ms")) {
        return QStringLiteral("COALESCE(t.duration_ms, 0)");
    }
    if (field == QStringLiteral("added_at_ms")) {
        return QStringLiteral("COALESCE(t.added_at_ms, 0)");
    }
    if (field == QStringLiteral("added_days_ago")) {
        return QStringLiteral("CAST((strftime('%s', 'now') * 1000 - COALESCE(t.added_at_ms, 0)) / 86400000 AS INTEGER)");
    }
    if (field == QStringLiteral("play_count")) {
        return QStringLiteral("COALESCE(s.play_count, 0)");
    }
    if (field == QStringLiteral("skip_count")) {
        return QStringLiteral("COALESCE(s.skip_count, 0)");
    }
    if (field == QStringLiteral("completion_count")) {
        return QStringLiteral("COALESCE(s.completion_count, 0)");
    }
    if (field == QStringLiteral("total_listen_ms")) {
        return QStringLiteral("COALESCE(s.total_listen_ms, 0)");
    }
    if (field == QStringLiteral("last_played_at_ms")) {
        return QStringLiteral("COALESCE(s.last_played_at_ms, 0)");
    }
    if (field == QStringLiteral("last_played_days_ago")) {
        return QStringLiteral(
            "CASE WHEN s.last_played_at_ms IS NULL THEN 1000000000 "
            "ELSE CAST((strftime('%s', 'now') * 1000 - s.last_played_at_ms) / 86400000 AS INTEGER) END");
    }
    if (field == QStringLiteral("favorite")) {
        return QStringLiteral("COALESCE(s.favorite, 0)");
    }
    if (field == QStringLiteral("rating")) {
        return QStringLiteral("COALESCE(s.rating, 0)");
    }
    return {};
}

constexpr qint64 kContextProgressMaxAgeMs = 180LL * 24LL * 60LL * 60LL * 1000LL;
constexpr int kContextProgressMaxRows = 4000;
} // namespace

SmartCollectionsEngine::SmartCollectionsEngine(QObject *parent)
    : QObject(parent)
{
}

SmartCollectionsEngine::~SmartCollectionsEngine()
{
    closeConnection();
}

void SmartCollectionsEngine::configure(bool enabled, const QString &databasePath)
{
    const bool previousEnabled = m_enabled;
    const QString normalizedPath = normalizePath(databasePath);
    const bool pathChanged = normalizedPath != m_databasePath;
    const bool enabledChangedState = enabled != previousEnabled;

    if (pathChanged || (!enabled && previousEnabled)) {
        closeConnection();
        m_defaultsEnsured = false;
    }

    m_enabled = enabled;
    m_databasePath = normalizedPath;

    if (enabledChangedState) {
        emit enabledChanged();
    }

    if (!m_enabled) {
        setLastError(QString());
        return;
    }

    if (!ensureConnection()) {
        return;
    }

    if (!ensureDefaultCollections()) {
        return;
    }

    setLastError(QString());
}

QVariantList SmartCollectionsEngine::listCollections() const
{
    QVariantList result;
    if (!m_enabled) {
        return result;
    }
    if (!ensureConnection()) {
        return result;
    }

    QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
    QSqlQuery query(db);
    if (!query.exec(QStringLiteral(
            "SELECT id, name, definition_json, sort_json, limit_count, enabled, pinned, "
            "created_at_ms, updated_at_ms "
            "FROM smart_collections "
            "ORDER BY pinned DESC, name COLLATE NOCASE ASC, id ASC"))) {
        setLastError(query.lastError().text());
        return result;
    }

    while (query.next()) {
        result.push_back(collectionRecordFromQuery(query));
    }
    setLastError(QString());
    return result;
}

QVariantMap SmartCollectionsEngine::getCollection(int id) const
{
    QVariantMap result;
    if (!m_enabled || id <= 0) {
        return result;
    }
    if (!ensureConnection()) {
        return result;
    }

    QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
    QSqlQuery query(db);
    if (!query.prepare(QStringLiteral(
            "SELECT id, name, definition_json, sort_json, limit_count, enabled, pinned, "
            "created_at_ms, updated_at_ms "
            "FROM smart_collections WHERE id = :id LIMIT 1"))) {
        setLastError(query.lastError().text());
        return result;
    }
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return result;
    }

    if (query.next()) {
        result = collectionRecordFromQuery(query);
        setLastError(QString());
    } else {
        setLastError(QStringLiteral("Smart collection not found"));
    }
    return result;
}

int SmartCollectionsEngine::createCollection(const QString &name,
                                             const QString &definitionJson,
                                             const QString &sortJson,
                                             int limitCount,
                                             bool enabled,
                                             bool pinned)
{
    if (!m_enabled) {
        setLastError(QStringLiteral("Smart collections engine is disabled"));
        return -1;
    }
    if (!ensureConnection()) {
        return -1;
    }

    const QString trimmedName = name.trimmed();
    if (trimmedName.isEmpty()) {
        setLastError(QStringLiteral("Collection name is empty"));
        return -1;
    }

    QString error;
    if (!validateDefinitionJson(definitionJson, &error)) {
        setLastError(error);
        return -1;
    }
    const QString compiledSort = compileSort(sortJson, &error);
    if (compiledSort.isEmpty() && !sortJson.trimmed().isEmpty()) {
        setLastError(error);
        return -1;
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
    QSqlQuery query(db);
    if (!query.prepare(QStringLiteral(
            "INSERT INTO smart_collections ("
            "name, definition_json, sort_json, limit_count, enabled, pinned, created_at_ms, updated_at_ms"
            ") VALUES ("
            ":name, :definition_json, :sort_json, :limit_count, :enabled, :pinned, :created_at_ms, :updated_at_ms"
            ")"))) {
        setLastError(query.lastError().text());
        return -1;
    }

    query.bindValue(QStringLiteral(":name"), trimmedName);
    query.bindValue(QStringLiteral(":definition_json"), definitionJson.trimmed());
    query.bindValue(QStringLiteral(":sort_json"), sortJson.trimmed());
    if (limitCount > 0) {
        query.bindValue(QStringLiteral(":limit_count"), limitCount);
    } else {
        query.bindValue(QStringLiteral(":limit_count"), QVariant());
    }
    query.bindValue(QStringLiteral(":enabled"), enabled ? 1 : 0);
    query.bindValue(QStringLiteral(":pinned"), pinned ? 1 : 0);
    query.bindValue(QStringLiteral(":created_at_ms"), nowMs);
    query.bindValue(QStringLiteral(":updated_at_ms"), nowMs);
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return -1;
    }

    const int id = query.lastInsertId().toInt();
    ++m_revision;
    emit collectionsChanged();
    setLastError(QString());
    return id;
}

bool SmartCollectionsEngine::updateCollection(int id,
                                              const QString &name,
                                              const QString &definitionJson,
                                              const QString &sortJson,
                                              int limitCount,
                                              bool enabled,
                                              bool pinned)
{
    if (!m_enabled) {
        setLastError(QStringLiteral("Smart collections engine is disabled"));
        return false;
    }
    if (id <= 0) {
        setLastError(QStringLiteral("Invalid smart collection id"));
        return false;
    }
    if (!ensureConnection()) {
        return false;
    }

    const QString trimmedName = name.trimmed();
    if (trimmedName.isEmpty()) {
        setLastError(QStringLiteral("Collection name is empty"));
        return false;
    }

    QString error;
    if (!validateDefinitionJson(definitionJson, &error)) {
        setLastError(error);
        return false;
    }
    const QString compiledSort = compileSort(sortJson, &error);
    if (compiledSort.isEmpty() && !sortJson.trimmed().isEmpty()) {
        setLastError(error);
        return false;
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
    QSqlQuery query(db);
    if (!query.prepare(QStringLiteral(
            "UPDATE smart_collections SET "
            "name = :name, "
            "definition_json = :definition_json, "
            "sort_json = :sort_json, "
            "limit_count = :limit_count, "
            "enabled = :enabled, "
            "pinned = :pinned, "
            "updated_at_ms = :updated_at_ms "
            "WHERE id = :id"))) {
        setLastError(query.lastError().text());
        return false;
    }

    query.bindValue(QStringLiteral(":name"), trimmedName);
    query.bindValue(QStringLiteral(":definition_json"), definitionJson.trimmed());
    query.bindValue(QStringLiteral(":sort_json"), sortJson.trimmed());
    if (limitCount > 0) {
        query.bindValue(QStringLiteral(":limit_count"), limitCount);
    } else {
        query.bindValue(QStringLiteral(":limit_count"), QVariant());
    }
    query.bindValue(QStringLiteral(":enabled"), enabled ? 1 : 0);
    query.bindValue(QStringLiteral(":pinned"), pinned ? 1 : 0);
    query.bindValue(QStringLiteral(":updated_at_ms"), nowMs);
    query.bindValue(QStringLiteral(":id"), id);

    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }

    if (query.numRowsAffected() <= 0) {
        setLastError(QStringLiteral("Smart collection not found"));
        return false;
    }

    ++m_revision;
    emit collectionsChanged();
    setLastError(QString());
    return true;
}

bool SmartCollectionsEngine::deleteCollection(int id)
{
    if (!m_enabled) {
        setLastError(QStringLiteral("Smart collections engine is disabled"));
        return false;
    }
    if (id <= 0) {
        setLastError(QStringLiteral("Invalid smart collection id"));
        return false;
    }
    if (!ensureConnection()) {
        return false;
    }

    QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
    QSqlQuery query(db);
    if (!query.prepare(QStringLiteral("DELETE FROM smart_collections WHERE id = :id"))) {
        setLastError(query.lastError().text());
        return false;
    }
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }

    if (query.numRowsAffected() <= 0) {
        setLastError(QStringLiteral("Smart collection not found"));
        return false;
    }

    ++m_revision;
    emit collectionsChanged();
    setLastError(QString());
    return true;
}

QVariantList SmartCollectionsEngine::resolveCollectionTracks(int id, int overrideLimit) const
{
    QVariantList result;
    if (!m_enabled || id <= 0) {
        return result;
    }
    if (!ensureConnection()) {
        return result;
    }

    QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
    QSqlQuery collectionQuery(db);
    if (!collectionQuery.prepare(QStringLiteral(
            "SELECT definition_json, sort_json, limit_count, enabled "
            "FROM smart_collections WHERE id = :id LIMIT 1"))) {
        setLastError(collectionQuery.lastError().text());
        return result;
    }
    collectionQuery.bindValue(QStringLiteral(":id"), id);
    if (!collectionQuery.exec()) {
        setLastError(collectionQuery.lastError().text());
        return result;
    }
    if (!collectionQuery.next()) {
        setLastError(QStringLiteral("Smart collection not found"));
        return result;
    }

    const bool collectionEnabled = collectionQuery.value(3).toInt() != 0;
    if (!collectionEnabled) {
        setLastError(QString());
        return result;
    }

    const QString definitionJson = collectionQuery.value(0).toString();
    const QString sortJson = collectionQuery.value(1).toString();
    const int configuredLimit = collectionQuery.value(2).isNull() ? -1 : collectionQuery.value(2).toInt();
    const int effectiveLimit = overrideLimit > 0 ? overrideLimit : configuredLimit;

    CompiledWhere compiledWhere;
    QString error;
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (!compileDefinition(definitionJson, nowMs, &compiledWhere, &error)) {
        setLastError(error);
        return result;
    }
    const QString compiledSort = compileSort(sortJson, &error);
    if (compiledSort.isEmpty()) {
        setLastError(error);
        return result;
    }

    QString sql = QStringLiteral(
        "SELECT "
        "t.canonical_path, t.title, t.artist, t.album, t.duration_ms, t.format, "
        "t.bitrate, t.sample_rate, t.bit_depth, t.album_art_uri, t.added_at_ms, "
        "COALESCE(s.play_count, 0), COALESCE(s.skip_count, 0), "
        "COALESCE(s.completion_count, 0), COALESCE(s.total_listen_ms, 0), "
        "COALESCE(s.last_played_at_ms, 0), COALESCE(s.favorite, 0), COALESCE(s.rating, 0) "
        "FROM tracks t "
        "LEFT JOIN track_stats s ON s.track_id = t.id "
        "WHERE t.deleted_at_ms IS NULL");

    if (!compiledWhere.sql.trimmed().isEmpty()) {
        sql += QStringLiteral(" AND (") + compiledWhere.sql + QStringLiteral(")");
    }

    sql += QStringLiteral(" ORDER BY ") + compiledSort;
    if (effectiveLimit > 0) {
        sql += QStringLiteral(" LIMIT :limit_count");
    }

    QSqlQuery query(db);
    if (!query.prepare(sql)) {
        setLastError(query.lastError().text());
        return result;
    }

    for (const auto &binding : compiledWhere.bindings) {
        query.bindValue(binding.first, binding.second);
    }
    if (effectiveLimit > 0) {
        query.bindValue(QStringLiteral(":limit_count"), effectiveLimit);
    }

    if (!query.exec()) {
        setLastError(query.lastError().text());
        return result;
    }

    while (query.next()) {
        result.push_back(trackRecordFromQuery(query));
    }

    setLastError(QString());
    return result;
}

QVariantMap SmartCollectionsEngine::loadContextPlaybackProgress() const
{
    QVariantMap payload;
    payload.insert(QStringLiteral("schema"), 1);
    payload.insert(QStringLiteral("playlists"), QVariantMap{});
    payload.insert(QStringLiteral("collections"), QVariantMap{});
    payload.insert(QStringLiteral("working"), QVariantMap{});
    payload.insert(QStringLiteral("activeContextType"), QStringLiteral("working"));
    payload.insert(QStringLiteral("activeContextId"), QStringLiteral("working"));
    payload.insert(QStringLiteral("active"),
                   QVariantMap{
                       {QStringLiteral("type"), QStringLiteral("working")},
                       {QStringLiteral("id"), QStringLiteral("working")}
                   });

    if (!m_enabled) {
        return payload;
    }
    if (!ensureConnection()) {
        return payload;
    }

    QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
    if (!pruneContextPlaybackProgressTable(&db)) {
        return payload;
    }

    QSqlQuery query(db);
    if (!query.prepare(QStringLiteral(
            "SELECT context_type, context_id, file_path, track_index, position_ms "
            "FROM context_playback_progress "
            "ORDER BY updated_at_ms DESC"))) {
        setLastError(query.lastError().text());
        return payload;
    }
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return payload;
    }

    QVariantMap playlists;
    QVariantMap collections;
    QVariantMap working;
    QString activeContextType = QStringLiteral("working");
    QString activeContextId = QStringLiteral("working");
    while (query.next()) {
        const QString contextType = query.value(0).toString().trimmed().toLower();
        const QString contextId = query.value(1).toString().trimmed();
        QVariantMap state;
        state.insert(QStringLiteral("filePath"), query.value(2).toString().trimmed());
        state.insert(QStringLiteral("currentIndex"), qMax(-1, query.value(3).toInt()));
        state.insert(QStringLiteral("positionMs"), qMax<qint64>(0, query.value(4).toLongLong()));

        if (contextType == QStringLiteral("playlist")) {
            if (!contextId.isEmpty()) {
                playlists.insert(contextId, state);
            }
        } else if (contextType == QStringLiteral("collection")) {
            if (!contextId.isEmpty()) {
                collections.insert(contextId, state);
            }
        } else if (contextType == QStringLiteral("working")) {
            working = state;
        } else if (contextType == QStringLiteral("active")) {
            const QString candidateType = query.value(2).toString().trimmed().toLower();
            if (candidateType == QStringLiteral("playlist")
                || candidateType == QStringLiteral("collection")
                || candidateType == QStringLiteral("working")) {
                activeContextType = candidateType;
                activeContextId = candidateType == QStringLiteral("working")
                    ? QStringLiteral("working")
                    : contextId;
            }
        }
    }

    payload.insert(QStringLiteral("playlists"), playlists);
    payload.insert(QStringLiteral("collections"), collections);
    payload.insert(QStringLiteral("working"), working);
    payload.insert(QStringLiteral("activeContextType"), activeContextType);
    payload.insert(QStringLiteral("activeContextId"), activeContextId);
    payload.insert(QStringLiteral("active"),
                   QVariantMap{
                       {QStringLiteral("type"), activeContextType},
                       {QStringLiteral("id"), activeContextId}
                   });
    setLastError(QString());
    return payload;
}

bool SmartCollectionsEngine::saveContextPlaybackProgress(const QVariantMap &payload)
{
    if (!m_enabled) {
        setLastError(QStringLiteral("Smart collections engine is disabled"));
        return false;
    }
    if (!ensureConnection()) {
        return false;
    }

    QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
    if (!db.transaction()) {
        setLastError(db.lastError().text());
        return false;
    }

    const auto rollbackWithError = [this, &db](const QString &error) {
        db.rollback();
        setLastError(error);
        return false;
    };

    QSqlQuery deleteQuery(db);
    if (!deleteQuery.exec(QStringLiteral("DELETE FROM context_playback_progress WHERE context_type = 'playlist'"))) {
        return rollbackWithError(deleteQuery.lastError().text());
    }
    if (!deleteQuery.exec(QStringLiteral("DELETE FROM context_playback_progress WHERE context_type = 'collection'"))) {
        return rollbackWithError(deleteQuery.lastError().text());
    }
    if (!deleteQuery.exec(QStringLiteral("DELETE FROM context_playback_progress WHERE context_type = 'working'"))) {
        return rollbackWithError(deleteQuery.lastError().text());
    }
    if (!deleteQuery.exec(QStringLiteral("DELETE FROM context_playback_progress WHERE context_type = 'active'"))) {
        return rollbackWithError(deleteQuery.lastError().text());
    }

    QSqlQuery insertQuery(db);
    if (!insertQuery.prepare(QStringLiteral(
            "INSERT INTO context_playback_progress ("
            "context_type, context_id, file_path, track_index, position_ms, updated_at_ms"
            ") VALUES ("
            ":context_type, :context_id, :file_path, :track_index, :position_ms, :updated_at_ms"
            ")"))) {
        return rollbackWithError(insertQuery.lastError().text());
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const QVariantMap playlistMap = payload.value(QStringLiteral("playlists")).toMap();
    const QVariantMap collectionMap = payload.value(QStringLiteral("collections")).toMap();
    const QVariantMap workingState = normalizeProgressState(payload.value(QStringLiteral("working")).toMap());
    const QVariantMap activeNode = payload.value(QStringLiteral("active")).toMap();
    QString activeContextType = activeNode.value(QStringLiteral("type")).toString().trimmed().toLower();
    QString activeContextId = activeNode.value(QStringLiteral("id")).toString().trimmed();
    if (activeContextType.isEmpty()) {
        activeContextType = payload.value(QStringLiteral("activeContextType")).toString().trimmed().toLower();
    }
    if (activeContextId.isEmpty()) {
        activeContextId = payload.value(QStringLiteral("activeContextId")).toString().trimmed();
    }
    const bool validActiveType = activeContextType == QStringLiteral("playlist")
        || activeContextType == QStringLiteral("collection")
        || activeContextType == QStringLiteral("working");
    if (!validActiveType) {
        activeContextType = QStringLiteral("working");
        activeContextId = QStringLiteral("working");
    } else if (activeContextType == QStringLiteral("working")) {
        activeContextId = QStringLiteral("working");
    } else if (activeContextId.isEmpty()) {
        activeContextType = QStringLiteral("working");
        activeContextId = QStringLiteral("working");
    }

    auto insertContextMap = [&insertQuery, nowMs](const QString &contextType, const QVariantMap &contextMap) -> bool {
        const auto keys = contextMap.keys();
        for (const QString &key : keys) {
            const QString contextId = key.trimmed();
            if (contextId.isEmpty()) {
                continue;
            }
            const QVariantMap state = SmartCollectionsEngine::normalizeProgressState(contextMap.value(key).toMap());
            insertQuery.bindValue(QStringLiteral(":context_type"), contextType);
            insertQuery.bindValue(QStringLiteral(":context_id"), contextId);
            insertQuery.bindValue(QStringLiteral(":file_path"), state.value(QStringLiteral("filePath")).toString());
            insertQuery.bindValue(QStringLiteral(":track_index"), state.value(QStringLiteral("currentIndex")).toInt());
            insertQuery.bindValue(QStringLiteral(":position_ms"), state.value(QStringLiteral("positionMs")).toLongLong());
            insertQuery.bindValue(QStringLiteral(":updated_at_ms"), nowMs);
            if (!insertQuery.exec()) {
                return false;
            }
        }
        return true;
    };

    if (!insertContextMap(QStringLiteral("playlist"), playlistMap)) {
        return rollbackWithError(insertQuery.lastError().text());
    }
    if (!insertContextMap(QStringLiteral("collection"), collectionMap)) {
        return rollbackWithError(insertQuery.lastError().text());
    }

    insertQuery.bindValue(QStringLiteral(":context_type"), QStringLiteral("working"));
    insertQuery.bindValue(QStringLiteral(":context_id"), QStringLiteral("working"));
    insertQuery.bindValue(QStringLiteral(":file_path"), workingState.value(QStringLiteral("filePath")).toString());
    insertQuery.bindValue(QStringLiteral(":track_index"), workingState.value(QStringLiteral("currentIndex")).toInt());
    insertQuery.bindValue(QStringLiteral(":position_ms"), workingState.value(QStringLiteral("positionMs")).toLongLong());
    insertQuery.bindValue(QStringLiteral(":updated_at_ms"), nowMs);
    if (!insertQuery.exec()) {
        return rollbackWithError(insertQuery.lastError().text());
    }

    insertQuery.bindValue(QStringLiteral(":context_type"), QStringLiteral("active"));
    insertQuery.bindValue(QStringLiteral(":context_id"), activeContextId);
    insertQuery.bindValue(QStringLiteral(":file_path"), activeContextType);
    insertQuery.bindValue(QStringLiteral(":track_index"), -1);
    insertQuery.bindValue(QStringLiteral(":position_ms"), 0);
    insertQuery.bindValue(QStringLiteral(":updated_at_ms"), nowMs);
    if (!insertQuery.exec()) {
        return rollbackWithError(insertQuery.lastError().text());
    }

    if (!pruneContextPlaybackProgressTable(&db)) {
        const QString error = m_lastError.isEmpty() ? QStringLiteral("Failed to prune context playback progress") : m_lastError;
        return rollbackWithError(error);
    }

    if (!db.commit()) {
        setLastError(db.lastError().text());
        return false;
    }

    setLastError(QString());
    return true;
}

QVariantMap SmartCollectionsEngine::normalizeProgressState(const QVariantMap &rawState)
{
    QVariantMap normalized;
    normalized.insert(QStringLiteral("filePath"), rawState.value(QStringLiteral("filePath")).toString().trimmed());
    normalized.insert(QStringLiteral("currentIndex"), qMax(-1, rawState.value(QStringLiteral("currentIndex"), -1).toInt()));
    normalized.insert(QStringLiteral("positionMs"),
                      qMax<qint64>(0, rawState.value(QStringLiteral("positionMs"), 0).toLongLong()));
    return normalized;
}

bool SmartCollectionsEngine::pruneContextPlaybackProgressTable(QSqlDatabase *db) const
{
    if (!db || !db->isValid() || !db->isOpen()) {
        setLastError(QStringLiteral("Database connection is not open"));
        return false;
    }

    const qint64 cutoffMs = QDateTime::currentMSecsSinceEpoch() - kContextProgressMaxAgeMs;
    QSqlQuery query(*db);
    if (!query.prepare(QStringLiteral(
            "DELETE FROM context_playback_progress "
            "WHERE updated_at_ms < :cutoff_ms"))) {
        setLastError(query.lastError().text());
        return false;
    }
    query.bindValue(QStringLiteral(":cutoff_ms"), cutoffMs);
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }

    if (!query.prepare(QStringLiteral(
            "DELETE FROM context_playback_progress "
            "WHERE rowid IN ("
            "SELECT rowid FROM context_playback_progress "
            "ORDER BY updated_at_ms DESC "
            "LIMIT -1 OFFSET :max_rows"
            ")"))) {
        setLastError(query.lastError().text());
        return false;
    }
    query.bindValue(QStringLiteral(":max_rows"), kContextProgressMaxRows);
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }

    return true;
}

bool SmartCollectionsEngine::ensureConnection() const
{
    if (!m_enabled) {
        setLastError(QStringLiteral("Smart collections engine is disabled"));
        return false;
    }
    if (m_databasePath.isEmpty()) {
        setLastError(QStringLiteral("Database path is empty"));
        return false;
    }

    if (m_connectionName.isEmpty()) {
        m_connectionName = QStringLiteral("waveflux-smart-collections");
    }

    if (QSqlDatabase::contains(m_connectionName)) {
        QSqlDatabase existingDb = QSqlDatabase::database(m_connectionName, false);
        if (existingDb.isValid() &&
            existingDb.isOpen() &&
            normalizePath(existingDb.databaseName()) == normalizePath(m_databasePath)) {
            return true;
        }

        if (existingDb.isValid()) {
            existingDb.close();
        }
        existingDb = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
    }

    QDir().mkpath(QFileInfo(m_databasePath).absolutePath());
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    db.setDatabaseName(m_databasePath);
    if (!db.open()) {
        const QString message = db.lastError().text();
        db = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
        setLastError(message);
        return false;
    }

    static const QStringList pragmas = {
        QStringLiteral("PRAGMA foreign_keys = ON"),
        QStringLiteral("PRAGMA query_only = OFF"),
        QStringLiteral("PRAGMA busy_timeout = 3000")
    };

    QSqlQuery query(db);
    for (const QString &pragma : pragmas) {
        if (!query.exec(pragma)) {
            const QString error = query.lastError().text();
            db.close();
            db = QSqlDatabase();
            QSqlDatabase::removeDatabase(m_connectionName);
            setLastError(error);
            return false;
        }
    }

    return true;
}

void SmartCollectionsEngine::closeConnection() const
{
    if (m_connectionName.isEmpty()) {
        return;
    }
    if (!QSqlDatabase::contains(m_connectionName)) {
        return;
    }

    {
        QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
        if (db.isValid()) {
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(m_connectionName);
}

bool SmartCollectionsEngine::ensureDefaultCollections()
{
    if (!m_enabled || m_defaultsEnsured) {
        return true;
    }
    if (!ensureConnection()) {
        return false;
    }

    bool tableIsEmpty = false;
    if (!collectionsTableIsEmpty(&tableIsEmpty)) {
        return false;
    }

    if (tableIsEmpty) {
        if (!insertDefaultCollections()) {
            return false;
        }
        ++m_revision;
        emit collectionsChanged();
    }

    m_defaultsEnsured = true;
    return true;
}

bool SmartCollectionsEngine::collectionsTableIsEmpty(bool *isEmptyOut) const
{
    if (!isEmptyOut) {
        setLastError(QStringLiteral("Invalid output pointer"));
        return false;
    }
    *isEmptyOut = true;

    if (!ensureConnection()) {
        return false;
    }

    QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("SELECT COUNT(1) FROM smart_collections"))) {
        setLastError(query.lastError().text());
        return false;
    }
    if (!query.next()) {
        setLastError(QStringLiteral("Failed to read smart_collections count"));
        return false;
    }
    *isEmptyOut = query.value(0).toLongLong() == 0;
    setLastError(QString());
    return true;
}

bool SmartCollectionsEngine::insertDefaultCollections() const
{
    if (!ensureConnection()) {
        return false;
    }

    struct Preset {
        QString name;
        QJsonObject definition;
        QJsonObject sort;
        int limit = -1;
        bool pinned = false;
    };

    const QList<Preset> presets = {
        {
            QStringLiteral("Недавно добавленные"),
            QJsonObject{
                {QStringLiteral("logic"), QStringLiteral("all")},
                {QStringLiteral("rules"),
                 QJsonArray{
                     QJsonObject{
                         {QStringLiteral("field"), QStringLiteral("added_days_ago")},
                         {QStringLiteral("op"), QStringLiteral("<=")},
                         {QStringLiteral("value"), 30}}}}},
            QJsonObject{
                {QStringLiteral("fields"),
                 QJsonArray{
                     QJsonObject{
                         {QStringLiteral("field"), QStringLiteral("added_at_ms")},
                         {QStringLiteral("dir"), QStringLiteral("desc")}}}}},
            300,
            true},
        {
            QStringLiteral("Часто слушаемые"),
            QJsonObject{
                {QStringLiteral("logic"), QStringLiteral("all")},
                {QStringLiteral("rules"),
                 QJsonArray{
                     QJsonObject{
                         {QStringLiteral("field"), QStringLiteral("play_count")},
                         {QStringLiteral("op"), QStringLiteral(">=")},
                         {QStringLiteral("value"), 20}}}}},
            QJsonObject{
                {QStringLiteral("fields"),
                 QJsonArray{
                     QJsonObject{
                         {QStringLiteral("field"), QStringLiteral("play_count")},
                         {QStringLiteral("dir"), QStringLiteral("desc")}},
                     QJsonObject{
                         {QStringLiteral("field"), QStringLiteral("last_played_at_ms")},
                         {QStringLiteral("dir"), QStringLiteral("desc")}}}}},
            500,
            true},
        {
            QStringLiteral("Давно не слушал"),
            QJsonObject{
                {QStringLiteral("logic"), QStringLiteral("all")},
                {QStringLiteral("rules"),
                 QJsonArray{
                     QJsonObject{
                         {QStringLiteral("field"), QStringLiteral("play_count")},
                         {QStringLiteral("op"), QStringLiteral(">")},
                         {QStringLiteral("value"), 0}},
                     QJsonObject{
                         {QStringLiteral("field"), QStringLiteral("last_played_days_ago")},
                         {QStringLiteral("op"), QStringLiteral(">")},
                         {QStringLiteral("value"), 45}}}}},
            QJsonObject{
                {QStringLiteral("fields"),
                 QJsonArray{
                     QJsonObject{
                         {QStringLiteral("field"), QStringLiteral("last_played_days_ago")},
                         {QStringLiteral("dir"), QStringLiteral("desc")}}}}},
            500,
            false},
        {
            QStringLiteral("Никогда не слушал"),
            QJsonObject{
                {QStringLiteral("logic"), QStringLiteral("all")},
                {QStringLiteral("rules"),
                 QJsonArray{
                     QJsonObject{
                         {QStringLiteral("field"), QStringLiteral("play_count")},
                         {QStringLiteral("op"), QStringLiteral("=")},
                         {QStringLiteral("value"), 0}}}}},
            QJsonObject{
                {QStringLiteral("fields"),
                 QJsonArray{
                     QJsonObject{
                         {QStringLiteral("field"), QStringLiteral("added_at_ms")},
                         {QStringLiteral("dir"), QStringLiteral("desc")}}}}},
            800,
            false},
        {
            QStringLiteral("Hi-Res"),
            QJsonObject{
                {QStringLiteral("logic"), QStringLiteral("any")},
                {QStringLiteral("rules"),
                 QJsonArray{
                     QJsonObject{
                         {QStringLiteral("field"), QStringLiteral("bit_depth")},
                         {QStringLiteral("op"), QStringLiteral(">")},
                         {QStringLiteral("value"), 16}},
                     QJsonObject{
                         {QStringLiteral("field"), QStringLiteral("sample_rate")},
                         {QStringLiteral("op"), QStringLiteral(">")},
                         {QStringLiteral("value"), 48000}}}}},
            QJsonObject{
                {QStringLiteral("fields"),
                 QJsonArray{
                     QJsonObject{
                         {QStringLiteral("field"), QStringLiteral("bit_depth")},
                         {QStringLiteral("dir"), QStringLiteral("desc")}},
                     QJsonObject{
                         {QStringLiteral("field"), QStringLiteral("sample_rate")},
                         {QStringLiteral("dir"), QStringLiteral("desc")}}}}},
            800,
            false}
    };

    QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
    if (!db.transaction()) {
        setLastError(db.lastError().text());
        return false;
    }

    QSqlQuery query(db);
    if (!query.prepare(QStringLiteral(
            "INSERT INTO smart_collections ("
            "name, definition_json, sort_json, limit_count, enabled, pinned, created_at_ms, updated_at_ms"
            ") VALUES ("
            ":name, :definition_json, :sort_json, :limit_count, 1, :pinned, :created_at_ms, :updated_at_ms"
            ")"))) {
        setLastError(query.lastError().text());
        db.rollback();
        return false;
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    for (const Preset &preset : presets) {
        query.bindValue(QStringLiteral(":name"), preset.name);
        query.bindValue(QStringLiteral(":definition_json"),
                        QString::fromUtf8(QJsonDocument(preset.definition).toJson(QJsonDocument::Compact)));
        query.bindValue(QStringLiteral(":sort_json"),
                        QString::fromUtf8(QJsonDocument(preset.sort).toJson(QJsonDocument::Compact)));
        if (preset.limit > 0) {
            query.bindValue(QStringLiteral(":limit_count"), preset.limit);
        } else {
            query.bindValue(QStringLiteral(":limit_count"), QVariant());
        }
        query.bindValue(QStringLiteral(":pinned"), preset.pinned ? 1 : 0);
        query.bindValue(QStringLiteral(":created_at_ms"), nowMs);
        query.bindValue(QStringLiteral(":updated_at_ms"), nowMs);
        if (!query.exec()) {
            setLastError(query.lastError().text());
            db.rollback();
            return false;
        }
    }

    if (!db.commit()) {
        setLastError(db.lastError().text());
        return false;
    }

    setLastError(QString());
    return true;
}

bool SmartCollectionsEngine::validateDefinitionJson(const QString &definitionJson, QString *errorOut) const
{
    CompiledWhere compiled;
    return compileDefinition(definitionJson,
                             QDateTime::currentMSecsSinceEpoch(),
                             &compiled,
                             errorOut);
}

bool SmartCollectionsEngine::compileDefinition(const QString &definitionJson,
                                               qint64 nowMs,
                                               CompiledWhere *compiledOut,
                                               QString *errorOut) const
{
    if (!compiledOut || !errorOut) {
        return false;
    }
    compiledOut->sql.clear();
    compiledOut->bindings.clear();
    errorOut->clear();

    const QString trimmed = definitionJson.trimmed();
    if (trimmed.isEmpty()) {
        compiledOut->sql = QStringLiteral("1=1");
        return true;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(trimmed.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        *errorOut = QStringLiteral("Invalid definition_json: %1").arg(parseError.errorString());
        return false;
    }
    if (!document.isObject()) {
        *errorOut = QStringLiteral("definition_json must be a JSON object");
        return false;
    }

    CompileContext context;
    context.nowMs = nowMs;
    QString sql;
    if (!compileNode(document.object(), &context, &sql, errorOut)) {
        return false;
    }

    if (sql.trimmed().isEmpty()) {
        sql = QStringLiteral("1=1");
    }
    compiledOut->sql = sql;
    compiledOut->bindings = context.bindings;
    return true;
}

bool SmartCollectionsEngine::compileNode(const QJsonValue &nodeValue,
                                         CompileContext *context,
                                         QString *sqlOut,
                                         QString *errorOut) const
{
    if (!context || !sqlOut || !errorOut) {
        return false;
    }
    if (!nodeValue.isObject()) {
        *errorOut = QStringLiteral("Rule node must be an object");
        return false;
    }

    const QJsonObject nodeObject = nodeValue.toObject();
    if (nodeObject.contains(QStringLiteral("rules"))) {
        const QString logic = nodeObject.value(QStringLiteral("logic"))
                                  .toString(QStringLiteral("all"))
                                  .trimmed()
                                  .toLower();
        const QString connector = logic == QStringLiteral("any")
            ? QStringLiteral(" OR ")
            : QStringLiteral(" AND ");

        const QJsonValue rulesValue = nodeObject.value(QStringLiteral("rules"));
        if (!rulesValue.isArray()) {
            *errorOut = QStringLiteral("'rules' must be an array");
            return false;
        }

        QStringList parts;
        const QJsonArray rulesArray = rulesValue.toArray();
        parts.reserve(rulesArray.size());
        for (const QJsonValue &item : rulesArray) {
            QString part;
            if (!compileNode(item, context, &part, errorOut)) {
                return false;
            }
            if (!part.trimmed().isEmpty()) {
                parts.push_back(part);
            }
        }

        if (parts.isEmpty()) {
            *sqlOut = QStringLiteral("1=1");
            return true;
        }
        if (parts.size() == 1) {
            *sqlOut = parts.front();
            return true;
        }
        *sqlOut = QStringLiteral("(%1)").arg(parts.join(connector));
        return true;
    }

    return compileRule(nodeObject, context, sqlOut, errorOut);
}

bool SmartCollectionsEngine::compileRule(const QJsonObject &ruleObject,
                                         CompileContext *context,
                                         QString *sqlOut,
                                         QString *errorOut) const
{
    if (!context || !sqlOut || !errorOut) {
        return false;
    }

    const QString field = ruleObject.value(QStringLiteral("field")).toString().trimmed().toLower();
    const QString op = ruleObject.value(QStringLiteral("op")).toString().trimmed().toLower();
    const QJsonValue value = ruleObject.value(QStringLiteral("value"));

    if (field.isEmpty()) {
        *errorOut = QStringLiteral("Rule is missing 'field'");
        return false;
    }
    if (op.isEmpty()) {
        *errorOut = QStringLiteral("Rule is missing 'op'");
        return false;
    }

    if (field == QStringLiteral("text") && op == QStringLiteral("match")) {
        const QVector<QString> tokens = splitMatchTokens(value.toString());
        if (tokens.isEmpty()) {
            *sqlOut = QStringLiteral("1=1");
            return true;
        }

        QStringList tokenConditions;
        tokenConditions.reserve(tokens.size());
        for (const QString &token : tokens) {
            QString localToken = token.trimmed();
            if (localToken.isEmpty()) {
                continue;
            }

            QStringList fields;
            QString tokenValue = localToken;
            const int colon = localToken.indexOf(QLatin1Char(':'));
            if (colon > 0 && colon + 1 < localToken.size()) {
                const QString prefix = localToken.left(colon).trimmed().toLower();
                tokenValue = localToken.mid(colon + 1).trimmed();
                if (prefix == QStringLiteral("title")) {
                    fields.push_back(QStringLiteral("LOWER(COALESCE(t.title, ''))"));
                } else if (prefix == QStringLiteral("artist")) {
                    fields.push_back(QStringLiteral("LOWER(COALESCE(t.artist, ''))"));
                } else if (prefix == QStringLiteral("album")) {
                    fields.push_back(QStringLiteral("LOWER(COALESCE(t.album, ''))"));
                } else if (prefix == QStringLiteral("path")) {
                    fields.push_back(QStringLiteral("LOWER(COALESCE(t.canonical_path, ''))"));
                }
            }

            if (fields.isEmpty()) {
                fields.push_back(QStringLiteral("LOWER(COALESCE(t.title, ''))"));
                fields.push_back(QStringLiteral("LOWER(COALESCE(t.artist, ''))"));
                fields.push_back(QStringLiteral("LOWER(COALESCE(t.album, ''))"));
                fields.push_back(QStringLiteral("LOWER(COALESCE(t.canonical_path, ''))"));
            }

            const QString parameterName =
                bindValue(context,
                          QStringLiteral("%%%1%%").arg(escapeLikePattern(tokenValue.toLower())));

            QStringList fieldPredicates;
            fieldPredicates.reserve(fields.size());
            for (const QString &expression : fields) {
                fieldPredicates.push_back(
                    QStringLiteral("%1 LIKE %2 ESCAPE '\\'").arg(expression, parameterName));
            }
            tokenConditions.push_back(QStringLiteral("(%1)").arg(fieldPredicates.join(QStringLiteral(" OR "))));
        }

        if (tokenConditions.isEmpty()) {
            *sqlOut = QStringLiteral("1=1");
            return true;
        }
        *sqlOut = QStringLiteral("(%1)").arg(tokenConditions.join(QStringLiteral(" AND ")));
        return true;
    }

    const FieldSpec fieldSpec = fieldSpecByName(field);
    if (fieldSpec.expression.isEmpty()) {
        *errorOut = QStringLiteral("Unsupported field: %1").arg(field);
        return false;
    }

    if (fieldSpec.usesNowMs && !context->nowBound) {
        context->bindings.push_back(qMakePair(QStringLiteral(":now_ms"), QVariant(context->nowMs)));
        context->nowBound = true;
    }

    const QString expression = fieldSpec.expression;

    auto textParameter = [&](const QString &rawValue) {
        return bindValue(context, rawValue.toLower());
    };
    auto numericParameter = [&](const QJsonValue &rawValue) {
        if (rawValue.isDouble()) {
            return bindValue(context, rawValue.toDouble());
        }
        return bindValue(context, rawValue.toVariant());
    };

    if (op == QStringLiteral("contains")) {
        if (fieldSpec.kind != FieldKind::Text) {
            *errorOut = QStringLiteral("Operator 'contains' requires text field");
            return false;
        }
        const QString parameterName = bindValue(
            context,
            QStringLiteral("%%%1%%").arg(escapeLikePattern(value.toString().toLower())));
        *sqlOut = QStringLiteral("(LOWER(%1) LIKE %2 ESCAPE '\\')").arg(expression, parameterName);
        return true;
    }

    if (op == QStringLiteral("starts_with")) {
        if (fieldSpec.kind != FieldKind::Text) {
            *errorOut = QStringLiteral("Operator 'starts_with' requires text field");
            return false;
        }
        const QString parameterName = bindValue(
            context,
            QStringLiteral("%1%%").arg(escapeLikePattern(value.toString().toLower())));
        *sqlOut = QStringLiteral("(LOWER(%1) LIKE %2 ESCAPE '\\')").arg(expression, parameterName);
        return true;
    }

    if (op == QStringLiteral("in")) {
        if (!value.isArray()) {
            *errorOut = QStringLiteral("Operator 'in' expects array value");
            return false;
        }

        const QJsonArray array = value.toArray();
        if (array.isEmpty()) {
            *sqlOut = QStringLiteral("0=1");
            return true;
        }

        QStringList params;
        params.reserve(array.size());
        for (const QJsonValue &item : array) {
            if (fieldSpec.kind == FieldKind::Text) {
                params.push_back(textParameter(item.toString()));
            } else {
                params.push_back(numericParameter(item));
            }
        }

        if (fieldSpec.kind == FieldKind::Text) {
            *sqlOut = QStringLiteral("(LOWER(%1) IN (%2))").arg(expression, params.join(QStringLiteral(", ")));
        } else {
            *sqlOut = QStringLiteral("(%1 IN (%2))").arg(expression, params.join(QStringLiteral(", ")));
        }
        return true;
    }

    if (op == QStringLiteral("between")) {
        if (!value.isArray()) {
            *errorOut = QStringLiteral("Operator 'between' expects array value [min, max]");
            return false;
        }
        const QJsonArray array = value.toArray();
        if (array.size() != 2) {
            *errorOut = QStringLiteral("Operator 'between' expects exactly two values");
            return false;
        }

        if (fieldSpec.kind == FieldKind::Text) {
            const QString p1 = textParameter(array.at(0).toString());
            const QString p2 = textParameter(array.at(1).toString());
            *sqlOut = QStringLiteral("(LOWER(%1) BETWEEN %2 AND %3)").arg(expression, p1, p2);
        } else {
            const QString p1 = numericParameter(array.at(0));
            const QString p2 = numericParameter(array.at(1));
            *sqlOut = QStringLiteral("(%1 BETWEEN %2 AND %3)").arg(expression, p1, p2);
        }
        return true;
    }

    if (op == QStringLiteral("match")) {
        if (fieldSpec.kind != FieldKind::Text) {
            *errorOut = QStringLiteral("Operator 'match' requires text field");
            return false;
        }
        const QVector<QString> tokens = splitMatchTokens(value.toString());
        if (tokens.isEmpty()) {
            *sqlOut = QStringLiteral("1=1");
            return true;
        }

        QStringList tokenConditions;
        tokenConditions.reserve(tokens.size());
        for (const QString &token : tokens) {
            const QString parameterName =
                bindValue(context,
                          QStringLiteral("%%%1%%").arg(escapeLikePattern(token.toLower())));
            tokenConditions.push_back(
                QStringLiteral("(LOWER(%1) LIKE %2 ESCAPE '\\')").arg(expression, parameterName));
        }
        *sqlOut = QStringLiteral("(%1)").arg(tokenConditions.join(QStringLiteral(" AND ")));
        return true;
    }

    if (op == QStringLiteral("=") ||
        op == QStringLiteral("!=") ||
        op == QStringLiteral(">") ||
        op == QStringLiteral(">=") ||
        op == QStringLiteral("<") ||
        op == QStringLiteral("<=")) {
        const QString sqlOp = op == QStringLiteral("!=") ? QStringLiteral("<>") : op;
        QString parameterName;
        if (fieldSpec.kind == FieldKind::Text) {
            parameterName = textParameter(value.toString());
            *sqlOut = QStringLiteral("(LOWER(%1) %2 %3)").arg(expression, sqlOp, parameterName);
        } else {
            parameterName = numericParameter(value);
            *sqlOut = QStringLiteral("(%1 %2 %3)").arg(expression, sqlOp, parameterName);
        }
        return true;
    }

    *errorOut = QStringLiteral("Unsupported operator: %1").arg(op);
    return false;
}

QString SmartCollectionsEngine::compileSort(const QString &sortJson, QString *errorOut) const
{
    if (errorOut) {
        errorOut->clear();
    }

    const QString trimmed = sortJson.trimmed();
    if (trimmed.isEmpty()) {
        return QStringLiteral("COALESCE(t.added_at_ms, 0) DESC, t.id ASC");
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(trimmed.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (errorOut) {
            *errorOut = QStringLiteral("Invalid sort_json: %1").arg(parseError.errorString());
        }
        return {};
    }

    QJsonArray sortItems;
    if (document.isArray()) {
        sortItems = document.array();
    } else if (document.isObject()) {
        const QJsonObject object = document.object();
        const QJsonValue fieldsValue = object.value(QStringLiteral("fields"));
        if (!fieldsValue.isArray()) {
            if (errorOut) {
                *errorOut = QStringLiteral("sort_json.fields must be an array");
            }
            return {};
        }
        sortItems = fieldsValue.toArray();
    } else {
        if (errorOut) {
            *errorOut = QStringLiteral("sort_json must be object or array");
        }
        return {};
    }

    QStringList clauses;
    clauses.reserve(sortItems.size() + 1);
    for (const QJsonValue &itemValue : sortItems) {
        if (!itemValue.isObject()) {
            continue;
        }

        const QJsonObject item = itemValue.toObject();
        const QString field = item.value(QStringLiteral("field")).toString().trimmed().toLower();
        const QString directionRaw = item.value(QStringLiteral("dir"))
                                         .toString(QStringLiteral("asc"))
                                         .trimmed()
                                         .toLower();
        const QString expression = sortableExpressionByField(field);
        if (expression.isEmpty()) {
            if (errorOut) {
                *errorOut = QStringLiteral("Unsupported sort field: %1").arg(field);
            }
            return {};
        }

        const QString direction = directionRaw == QStringLiteral("desc")
            ? QStringLiteral("DESC")
            : QStringLiteral("ASC");
        clauses.push_back(QStringLiteral("%1 %2").arg(expression, direction));
    }

    clauses.push_back(QStringLiteral("t.id ASC"));
    return clauses.join(QStringLiteral(", "));
}

QString SmartCollectionsEngine::bindValue(CompileContext *context, const QVariant &value) const
{
    const QString name = QStringLiteral(":p%1").arg(context->nextBindId++);
    context->bindings.push_back(qMakePair(name, value));
    return name;
}

QString SmartCollectionsEngine::escapeLikePattern(QString value)
{
    value.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
    value.replace(QStringLiteral("%"), QStringLiteral("\\%"));
    value.replace(QStringLiteral("_"), QStringLiteral("\\_"));
    return value;
}

QVector<QString> SmartCollectionsEngine::splitMatchTokens(const QString &value)
{
    QVector<QString> tokens;
    QString current;
    bool inQuotes = false;

    const QString normalized = value.trimmed().toLower();
    for (const QChar ch : normalized) {
        if (ch == QLatin1Char('"')) {
            inQuotes = !inQuotes;
            continue;
        }

        if (!inQuotes && ch.isSpace()) {
            if (!current.isEmpty()) {
                tokens.push_back(current);
                current.clear();
            }
            continue;
        }

        current.append(ch);
    }

    if (!current.isEmpty()) {
        tokens.push_back(current);
    }
    return tokens;
}

QVariantMap SmartCollectionsEngine::collectionRecordFromQuery(const QSqlQuery &query)
{
    QVariantMap record;
    record.insert(QStringLiteral("id"), query.value(0));
    record.insert(QStringLiteral("name"), query.value(1));
    record.insert(QStringLiteral("definitionJson"), query.value(2));
    record.insert(QStringLiteral("sortJson"), query.value(3));
    record.insert(QStringLiteral("limitCount"), query.value(4));
    record.insert(QStringLiteral("enabled"), query.value(5).toInt() != 0);
    record.insert(QStringLiteral("pinned"), query.value(6).toInt() != 0);
    record.insert(QStringLiteral("createdAtMs"), query.value(7));
    record.insert(QStringLiteral("updatedAtMs"), query.value(8));
    return record;
}

QVariantMap SmartCollectionsEngine::trackRecordFromQuery(const QSqlQuery &query)
{
    QVariantMap record;
    record.insert(QStringLiteral("filePath"), query.value(0).toString());
    record.insert(QStringLiteral("title"), query.value(1).toString());
    record.insert(QStringLiteral("artist"), query.value(2).toString());
    record.insert(QStringLiteral("album"), query.value(3).toString());
    record.insert(QStringLiteral("durationMs"), query.value(4).toLongLong());
    record.insert(QStringLiteral("format"), query.value(5).toString());
    record.insert(QStringLiteral("bitrate"), query.value(6).toInt());
    record.insert(QStringLiteral("sampleRate"), query.value(7).toInt());
    record.insert(QStringLiteral("bitDepth"), query.value(8).toInt());
    record.insert(QStringLiteral("albumArt"), query.value(9).toString());
    record.insert(QStringLiteral("addedAtMs"), query.value(10).toLongLong());
    record.insert(QStringLiteral("playCount"), query.value(11).toInt());
    record.insert(QStringLiteral("skipCount"), query.value(12).toInt());
    record.insert(QStringLiteral("completionCount"), query.value(13).toInt());
    record.insert(QStringLiteral("totalListenMs"), query.value(14).toLongLong());
    record.insert(QStringLiteral("lastPlayedAtMs"), query.value(15).toLongLong());
    record.insert(QStringLiteral("favorite"), query.value(16).toInt() != 0);
    record.insert(QStringLiteral("rating"), query.value(17).toInt());
    return record;
}

void SmartCollectionsEngine::setLastError(const QString &error) const
{
    if (m_lastError == error) {
        return;
    }
    m_lastError = error;
    emit const_cast<SmartCollectionsEngine *>(this)->lastErrorChanged();
}
