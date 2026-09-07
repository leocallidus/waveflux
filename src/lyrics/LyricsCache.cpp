#include "LyricsCache.h"
#include "LrcParser.h"
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QUuid>

namespace WaveFlux::Lyrics {

LyricsCache::LyricsCache()
    : m_connectionName(QStringLiteral("waveflux_lyrics_cache_%1").arg(QUuid::createUuid().toString(QUuid::Id128)))
{
}

LyricsCache::~LyricsCache()
{
    close();
}

void LyricsCache::close()
{
    QMutexLocker locker(&m_mutex);
    if (m_initialized) {
        {
            QSqlDatabase d = QSqlDatabase::database(m_connectionName);
            if (d.isOpen()) {
                d.close();
            }
        }
        QSqlDatabase::removeDatabase(m_connectionName);
        m_initialized = false;
    }
}

QSqlDatabase LyricsCache::db() const
{
    return QSqlDatabase::database(m_connectionName);
}

bool LyricsCache::init(const QString &customDbPath)
{
    QMutexLocker locker(&m_mutex);
    if (m_initialized) {
        return true;
    }

    QString dbPath = customDbPath;
    if (dbPath.isEmpty()) {
        const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        QDir().mkpath(dataDir);
        dbPath = QDir(dataDir).filePath(QStringLiteral("lyrics_cache.db"));
    } else {
        const QFileInfo info(dbPath);
        QDir().mkpath(info.absolutePath());
    }

    QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    database.setDatabaseName(dbPath);
    if (!database.open()) {
        return false;
    }

    m_initialized = true;
    ensureSchema();
    return true;
}

void LyricsCache::ensureSchema()
{
    QSqlQuery q(db());
    q.exec(QStringLiteral("PRAGMA foreign_keys = ON;"));
    q.exec(QStringLiteral("PRAGMA journal_mode = WAL;"));

    q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS documents ("
        "  id TEXT PRIMARY KEY,"
        "  kind INTEGER NOT NULL,"
        "  raw_content TEXT NOT NULL,"
        "  created_at INTEGER NOT NULL,"
        "  last_accessed_at INTEGER NOT NULL,"
        "  size_bytes INTEGER NOT NULL"
        ");"
    ));

    q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS query_results ("
        "  query_key TEXT PRIMARY KEY,"
        "  document_id TEXT REFERENCES documents(id) ON DELETE SET NULL,"
        "  is_negative INTEGER DEFAULT 0,"
        "  cached_at INTEGER NOT NULL,"
        "  expires_at INTEGER NOT NULL"
        ");"
    ));

    q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS track_preferences ("
        "  track_key TEXT PRIMARY KEY,"
        "  document_id TEXT REFERENCES documents(id) ON DELETE SET NULL,"
        "  user_delay_ms INTEGER DEFAULT 0,"
        "  manual_override INTEGER DEFAULT 0,"
        "  updated_at INTEGER NOT NULL"
        ");"
    ));

    q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS imported_documents ("
        "  track_key TEXT PRIMARY KEY,"
        "  document_id TEXT REFERENCES documents(id) ON DELETE CASCADE,"
        "  imported_at INTEGER NOT NULL"
        ");"
    ));

    q.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_docs_last_accessed ON documents(last_accessed_at);"));
    q.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_query_expires ON query_results(expires_at);"));
}

std::optional<LyricsDocument> LyricsCache::getDocument(const QString &docId)
{
    QMutexLocker locker(&m_mutex);
    if (!m_initialized || docId.isEmpty()) {
        return std::nullopt;
    }

    QSqlQuery q(db());
    q.prepare(QStringLiteral("SELECT kind, raw_content FROM documents WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), docId);
    if (!q.exec() || !q.next()) {
        return std::nullopt;
    }

    const int kindInt = q.value(0).toInt();
    const QString raw = q.value(1).toString();

    // Update last_accessed_at
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    QSqlQuery updateQ(db());
    updateQ.prepare(QStringLiteral("UPDATE documents SET last_accessed_at = :now WHERE id = :id"));
    updateQ.bindValue(QStringLiteral(":now"), now);
    updateQ.bindValue(QStringLiteral(":id"), docId);
    updateQ.exec();

    LyricsDocument doc;
    doc.id = docId;
    doc.contentHash = docId;
    doc.text = raw;
    doc.kind = static_cast<LyricsKind>(kindInt);

    if (doc.kind == LyricsKind::Synced) {
        ParsedLyrics parsed = LrcParser::parse(raw.toUtf8());
        doc.cues = std::move(parsed.cues);
        doc.plainText = std::move(parsed.plainText);
        doc.lrcOffsetMs = parsed.offsetMs;
    } else {
        doc.plainText = raw;
    }

    return doc;
}

bool LyricsCache::storeDocument(const LyricsDocument &doc)
{
    QMutexLocker locker(&m_mutex);
    if (!m_initialized || doc.id.isEmpty() || doc.text.isEmpty()) {
        return false;
    }

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const qint64 sizeBytes = doc.text.toUtf8().size();

    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "INSERT INTO documents (id, kind, raw_content, created_at, last_accessed_at, size_bytes) "
        "VALUES (:id, :kind, :content, :created, :accessed, :size) "
        "ON CONFLICT(id) DO UPDATE SET last_accessed_at = :accessed"
    ));
    q.bindValue(QStringLiteral(":id"), doc.id);
    q.bindValue(QStringLiteral(":kind"), static_cast<int>(doc.kind));
    q.bindValue(QStringLiteral(":content"), doc.text);
    q.bindValue(QStringLiteral(":created"), now);
    q.bindValue(QStringLiteral(":accessed"), now);
    q.bindValue(QStringLiteral(":size"), sizeBytes);

    const bool ok = q.exec();
    if (ok) {
        evictIfNeeded();
    }
    return ok;
}

std::optional<LyricsCache::QueryResult> LyricsCache::getQueryResult(const QString &queryKey)
{
    QMutexLocker locker(&m_mutex);
    if (!m_initialized || queryKey.isEmpty()) {
        return std::nullopt;
    }

    QSqlQuery q(db());
    q.prepare(QStringLiteral("SELECT document_id, is_negative, expires_at FROM query_results WHERE query_key = :k"));
    q.bindValue(QStringLiteral(":k"), queryKey);
    if (!q.exec() || !q.next()) {
        return std::nullopt;
    }

    QueryResult res;
    res.documentId = q.value(0).toString();
    res.isNegative = q.value(1).toInt() != 0;
    const qint64 expiresAt = q.value(2).toLongLong();
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    res.expired = (now >= expiresAt);

    return res;
}

bool LyricsCache::storeQueryResult(const QString &queryKey, const QString &docId, bool isNegative)
{
    QMutexLocker locker(&m_mutex);
    if (!m_initialized || queryKey.isEmpty()) {
        return false;
    }

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const qint64 ttl = isNegative ? kNegativeTtlSecs : kPositiveTtlSecs;
    const qint64 expiresAt = now + ttl;

    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "INSERT INTO query_results (query_key, document_id, is_negative, cached_at, expires_at) "
        "VALUES (:k, :doc, :neg, :cached, :expires) "
        "ON CONFLICT(query_key) DO UPDATE SET "
        "document_id = :doc, is_negative = :neg, cached_at = :cached, expires_at = :expires"
    ));
    q.bindValue(QStringLiteral(":k"), queryKey);
    q.bindValue(QStringLiteral(":doc"), docId.isEmpty() ? QVariant(QMetaType(QMetaType::QString)) : docId);
    q.bindValue(QStringLiteral(":neg"), isNegative ? 1 : 0);
    q.bindValue(QStringLiteral(":cached"), now);
    q.bindValue(QStringLiteral(":expires"), expiresAt);

    return q.exec();
}

std::optional<qint64> LyricsCache::getUserDelay(const QString &trackKey)
{
    QMutexLocker locker(&m_mutex);
    if (!m_initialized || trackKey.isEmpty()) {
        return std::nullopt;
    }

    QSqlQuery q(db());
    q.prepare(QStringLiteral("SELECT user_delay_ms FROM track_preferences WHERE track_key = :k"));
    q.bindValue(QStringLiteral(":k"), trackKey);
    if (!q.exec() || !q.next()) {
        return std::nullopt;
    }

    return q.value(0).toLongLong();
}

bool LyricsCache::storeUserDelay(const QString &trackKey, qint64 delayMs)
{
    QMutexLocker locker(&m_mutex);
    if (!m_initialized || trackKey.isEmpty()) {
        return false;
    }

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "INSERT INTO track_preferences (track_key, user_delay_ms, updated_at) "
        "VALUES (:k, :delay, :now) "
        "ON CONFLICT(track_key) DO UPDATE SET user_delay_ms = :delay, updated_at = :now"
    ));
    q.bindValue(QStringLiteral(":k"), trackKey);
    q.bindValue(QStringLiteral(":delay"), delayMs);
    q.bindValue(QStringLiteral(":now"), now);

    return q.exec();
}

std::optional<QString> LyricsCache::getTrackOverride(const QString &trackKey)
{
    QMutexLocker locker(&m_mutex);
    if (!m_initialized || trackKey.isEmpty()) {
        return std::nullopt;
    }

    QSqlQuery q(db());
    q.prepare(QStringLiteral("SELECT document_id FROM track_preferences WHERE track_key = :k AND manual_override = 1"));
    q.bindValue(QStringLiteral(":k"), trackKey);
    if (!q.exec() || !q.next()) {
        return std::nullopt;
    }

    const QString id = q.value(0).toString();
    if (id.isEmpty()) {
        return std::nullopt;
    }
    return id;
}

bool LyricsCache::storeTrackOverride(const QString &trackKey, const QString &docId)
{
    QMutexLocker locker(&m_mutex);
    if (!m_initialized || trackKey.isEmpty() || docId.isEmpty()) {
        return false;
    }

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "INSERT INTO track_preferences (track_key, document_id, manual_override, updated_at) "
        "VALUES (:k, :doc, 1, :now) "
        "ON CONFLICT(track_key) DO UPDATE SET document_id = :doc, manual_override = 1, updated_at = :now"
    ));
    q.bindValue(QStringLiteral(":k"), trackKey);
    q.bindValue(QStringLiteral(":doc"), docId);
    q.bindValue(QStringLiteral(":now"), now);

    return q.exec();
}

bool LyricsCache::clearTrackOverride(const QString &trackKey)
{
    QMutexLocker locker(&m_mutex);
    if (!m_initialized || trackKey.isEmpty()) {
        return false;
    }

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "UPDATE track_preferences SET document_id = NULL, manual_override = 0, updated_at = :now WHERE track_key = :k"
    ));
    q.bindValue(QStringLiteral(":now"), now);
    q.bindValue(QStringLiteral(":k"), trackKey);

    return q.exec();
}

bool LyricsCache::storeImportedDocument(const QString &trackKey, const LyricsDocument &doc)
{
    if (trackKey.isEmpty() || doc.id.isEmpty()) {
        return false;
    }

    if (!storeDocument(doc)) {
        return false;
    }

    QMutexLocker locker(&m_mutex);
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "INSERT INTO imported_documents (track_key, document_id, imported_at) "
        "VALUES (:k, :doc, :now) "
        "ON CONFLICT(track_key) DO UPDATE SET document_id = :doc, imported_at = :now"
    ));
    q.bindValue(QStringLiteral(":k"), trackKey);
    q.bindValue(QStringLiteral(":doc"), doc.id);
    q.bindValue(QStringLiteral(":now"), now);

    return q.exec();
}

std::optional<LyricsDocument> LyricsCache::getImportedDocument(const QString &trackKey)
{
    QString docId;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_initialized || trackKey.isEmpty()) {
            return std::nullopt;
        }

        QSqlQuery q(db());
        q.prepare(QStringLiteral("SELECT document_id FROM imported_documents WHERE track_key = :k"));
        q.bindValue(QStringLiteral(":k"), trackKey);
        if (!q.exec() || !q.next()) {
            return std::nullopt;
        }
        docId = q.value(0).toString();
    }

    if (docId.isEmpty()) {
        return std::nullopt;
    }
    return getDocument(docId);
}

void LyricsCache::evictIfNeeded()
{
    // Check limits: kMaxCachedDocuments, kMaxCacheBytes
    QSqlQuery statQ(db());
    if (!statQ.exec(QStringLiteral("SELECT COUNT(*), COALESCE(SUM(size_bytes), 0) FROM documents;"))) {
        return;
    }
    if (!statQ.next()) {
        return;
    }

    int count = statQ.value(0).toInt();
    qint64 totalBytes = statQ.value(1).toLongLong();

    if (count <= kMaxCachedDocuments && totalBytes <= kMaxCacheBytes) {
        return;
    }

    // Select candidate IDs to evict ordered by last_accessed_at ASC
    // Exclude documents referenced in track_preferences (with manual_override = 1) or imported_documents
    const QString selectSql = QStringLiteral(
        "SELECT id, size_bytes FROM documents "
        "WHERE id NOT IN (SELECT document_id FROM track_preferences WHERE manual_override = 1 AND document_id IS NOT NULL "
        "                 UNION "
        "                 SELECT document_id FROM imported_documents WHERE document_id IS NOT NULL) "
        "ORDER BY last_accessed_at ASC LIMIT 100;"
    );

    QSqlQuery candQ(db());
    if (!candQ.exec(selectSql)) {
        return;
    }

    QStringList idsToDelete;
    while (candQ.next()) {
        idsToDelete.append(candQ.value(0).toString());
        count--;
        totalBytes -= candQ.value(1).toLongLong();
        if (count <= (kMaxCachedDocuments * 9 / 10) && totalBytes <= (kMaxCacheBytes * 9 / 10)) {
            break;
        }
    }

    if (!idsToDelete.isEmpty()) {
        for (const QString &id : idsToDelete) {
            QSqlQuery delQ(db());
            delQ.prepare(QStringLiteral("DELETE FROM documents WHERE id = :id"));
            delQ.bindValue(QStringLiteral(":id"), id);
            delQ.exec();
        }
    }
}

void LyricsCache::purgeExpired()
{
    QMutexLocker locker(&m_mutex);
    if (!m_initialized) {
        return;
    }

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    QSqlQuery q(db());
    q.prepare(QStringLiteral("DELETE FROM query_results WHERE expires_at <= :now"));
    q.bindValue(QStringLiteral(":now"), now);
    q.exec();
}

} // namespace WaveFlux::Lyrics
