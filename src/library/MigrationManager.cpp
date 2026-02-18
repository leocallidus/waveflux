#include "library/MigrationManager.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>

bool MigrationManager::migrate(const QString &connectionName)
{
    int currentVersion = 0;
    if (!readUserVersion(connectionName, &currentVersion)) {
        return false;
    }

    if (currentVersion > kLatestVersion) {
        m_lastError = QStringLiteral("Unsupported schema version %1, latest supported is %2")
            .arg(currentVersion)
            .arg(kLatestVersion);
        return false;
    }

    if (currentVersion == kLatestVersion) {
        m_lastError.clear();
#if defined(QT_DEBUG)
        if (!runIntegrityCheck(connectionName)) {
            return false;
        }
#endif
        return true;
    }

    QSqlDatabase db = QSqlDatabase::database(connectionName, false);
    if (!db.isValid() || !db.isOpen()) {
        m_lastError = QStringLiteral("Database connection is not open");
        return false;
    }

    for (int targetVersion = currentVersion + 1; targetVersion <= kLatestVersion; ++targetVersion) {
        if (!db.transaction()) {
            m_lastError = db.lastError().text();
            return false;
        }

        if (!applyMigrationStep(&db, targetVersion)) {
            const QString stepError = m_lastError;
            db.rollback();
            m_lastError = QStringLiteral("Migration to v%1 failed: %2")
                .arg(targetVersion)
                .arg(stepError);
            return false;
        }

        if (!executeStatement(&db, QStringLiteral("PRAGMA user_version = %1").arg(targetVersion))) {
            const QString versionError = m_lastError;
            db.rollback();
            m_lastError = QStringLiteral("Failed to set schema version to v%1: %2")
                .arg(targetVersion)
                .arg(versionError);
            return false;
        }

        if (!db.commit()) {
            m_lastError = db.lastError().text();
            return false;
        }

        currentVersion = targetVersion;
    }

#if defined(QT_DEBUG)
    if (!runIntegrityCheck(connectionName)) {
        return false;
    }
#endif

    m_lastError.clear();
    return true;
}

bool MigrationManager::readUserVersion(const QString &connectionName, int *versionOut)
{
    if (!versionOut) {
        m_lastError = QStringLiteral("Invalid output pointer for user_version");
        return false;
    }

    QSqlDatabase db = QSqlDatabase::database(connectionName, false);
    if (!db.isValid() || !db.isOpen()) {
        m_lastError = QStringLiteral("Database connection is not open");
        return false;
    }

    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("PRAGMA user_version"))) {
        m_lastError = query.lastError().text();
        return false;
    }

    if (!query.next()) {
        m_lastError = QStringLiteral("Failed to read schema version");
        return false;
    }

    *versionOut = query.value(0).toInt();
    return true;
}

bool MigrationManager::applyMigrationStep(QSqlDatabase *db, int targetVersion)
{
    if (!db) {
        m_lastError = QStringLiteral("Invalid database pointer");
        return false;
    }

    switch (targetVersion) {
    case 1:
        return applyMigrationV1(db);
    case 2:
        return applyMigrationV2(db);
    case 3:
        return applyMigrationV3(db);
    default:
        m_lastError = QStringLiteral("Unknown migration target version: %1").arg(targetVersion);
        return false;
    }
}

bool MigrationManager::applyMigrationV1(QSqlDatabase *db)
{
    const QStringList statements = {
        QStringLiteral("CREATE TABLE IF NOT EXISTS tracks ("
                       "id INTEGER PRIMARY KEY, "
                       "canonical_path TEXT NOT NULL UNIQUE, "
                       "file_name TEXT NOT NULL, "
                       "title TEXT, "
                       "artist TEXT, "
                       "album TEXT, "
                       "duration_ms INTEGER NOT NULL DEFAULT 0, "
                       "format TEXT, "
                       "bitrate INTEGER NOT NULL DEFAULT 0, "
                       "sample_rate INTEGER NOT NULL DEFAULT 0, "
                       "bit_depth INTEGER NOT NULL DEFAULT 0, "
                       "album_art_uri TEXT, "
                       "file_size_bytes INTEGER NOT NULL DEFAULT 0, "
                       "mtime_ms INTEGER NOT NULL DEFAULT 0, "
                       "added_at_ms INTEGER NOT NULL, "
                       "updated_at_ms INTEGER NOT NULL, "
                       "deleted_at_ms INTEGER)"),

        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_tracks_artist ON tracks(artist)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_tracks_album ON tracks(album)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_tracks_added_at ON tracks(added_at_ms)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_tracks_deleted ON tracks(deleted_at_ms)"),

        QStringLiteral("CREATE VIRTUAL TABLE IF NOT EXISTS tracks_fts USING fts5("
                       "title, "
                       "artist, "
                       "album, "
                       "canonical_path, "
                       "content='tracks', "
                       "content_rowid='id', "
                       "tokenize='unicode61 remove_diacritics 2')"),

        QStringLiteral("CREATE TRIGGER IF NOT EXISTS tracks_ai AFTER INSERT ON tracks BEGIN "
                       "INSERT INTO tracks_fts(rowid, title, artist, album, canonical_path) "
                       "VALUES (new.id, new.title, new.artist, new.album, new.canonical_path); "
                       "END"),

        QStringLiteral("CREATE TRIGGER IF NOT EXISTS tracks_ad AFTER DELETE ON tracks BEGIN "
                       "INSERT INTO tracks_fts(tracks_fts, rowid, title, artist, album, canonical_path) "
                       "VALUES ('delete', old.id, old.title, old.artist, old.album, old.canonical_path); "
                       "END"),

        QStringLiteral("CREATE TRIGGER IF NOT EXISTS tracks_au AFTER UPDATE ON tracks BEGIN "
                       "INSERT INTO tracks_fts(tracks_fts, rowid, title, artist, album, canonical_path) "
                       "VALUES ('delete', old.id, old.title, old.artist, old.album, old.canonical_path); "
                       "INSERT INTO tracks_fts(rowid, title, artist, album, canonical_path) "
                       "VALUES (new.id, new.title, new.artist, new.album, new.canonical_path); "
                       "END"),

        QStringLiteral("CREATE TABLE IF NOT EXISTS track_stats ("
                       "track_id INTEGER PRIMARY KEY, "
                       "play_count INTEGER NOT NULL DEFAULT 0, "
                       "skip_count INTEGER NOT NULL DEFAULT 0, "
                       "completion_count INTEGER NOT NULL DEFAULT 0, "
                       "total_listen_ms INTEGER NOT NULL DEFAULT 0, "
                       "last_played_at_ms INTEGER, "
                       "last_skipped_at_ms INTEGER, "
                       "rating INTEGER, "
                       "favorite INTEGER NOT NULL DEFAULT 0, "
                       "FOREIGN KEY(track_id) REFERENCES tracks(id) ON DELETE CASCADE)"),

        QStringLiteral("CREATE TABLE IF NOT EXISTS play_events ("
                       "id INTEGER PRIMARY KEY, "
                       "track_id INTEGER NOT NULL, "
                       "started_at_ms INTEGER NOT NULL, "
                       "ended_at_ms INTEGER, "
                       "listen_ms INTEGER NOT NULL DEFAULT 0, "
                       "completion_ratio REAL NOT NULL DEFAULT 0, "
                       "source TEXT, "
                       "was_skipped INTEGER NOT NULL DEFAULT 0, "
                       "session_id TEXT, "
                       "FOREIGN KEY(track_id) REFERENCES tracks(id) ON DELETE CASCADE)"),

        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_play_events_track_time "
                       "ON play_events(track_id, started_at_ms DESC)"),

        QStringLiteral("CREATE TABLE IF NOT EXISTS smart_collections ("
                       "id INTEGER PRIMARY KEY, "
                       "name TEXT NOT NULL, "
                       "definition_json TEXT NOT NULL, "
                       "sort_json TEXT, "
                       "limit_count INTEGER, "
                       "enabled INTEGER NOT NULL DEFAULT 1, "
                       "pinned INTEGER NOT NULL DEFAULT 0, "
                       "created_at_ms INTEGER NOT NULL, "
                       "updated_at_ms INTEGER NOT NULL)")
    };

    for (const QString &statement : statements) {
        if (!executeStatement(db, statement)) {
            return false;
        }
    }

    return true;
}

bool MigrationManager::applyMigrationV2(QSqlDatabase *db)
{
    const QStringList statements = {
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_track_stats_play_count "
                       "ON track_stats(play_count)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_track_stats_last_played "
                       "ON track_stats(last_played_at_ms)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_track_stats_favorite "
                       "ON track_stats(favorite)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_track_stats_rating "
                       "ON track_stats(rating)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_smart_collections_enabled_pinned "
                       "ON smart_collections(enabled, pinned)")
    };

    for (const QString &statement : statements) {
        if (!executeStatement(db, statement)) {
            return false;
        }
    }

    return true;
}

bool MigrationManager::applyMigrationV3(QSqlDatabase *db)
{
    const QStringList statements = {
        QStringLiteral("CREATE TABLE IF NOT EXISTS context_playback_progress ("
                       "context_type TEXT NOT NULL, "
                       "context_id TEXT NOT NULL, "
                       "file_path TEXT, "
                       "track_index INTEGER NOT NULL DEFAULT -1, "
                       "position_ms INTEGER NOT NULL DEFAULT 0, "
                       "updated_at_ms INTEGER NOT NULL, "
                       "PRIMARY KEY (context_type, context_id))"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_context_progress_updated "
                       "ON context_playback_progress(updated_at_ms DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_context_progress_type "
                       "ON context_playback_progress(context_type)")
    };

    for (const QString &statement : statements) {
        if (!executeStatement(db, statement)) {
            return false;
        }
    }

    return true;
}

bool MigrationManager::executeStatement(QSqlDatabase *db, const QString &sql)
{
    if (!db) {
        m_lastError = QStringLiteral("Invalid database pointer");
        return false;
    }

    QSqlQuery query(*db);
    if (!query.exec(sql)) {
        m_lastError = query.lastError().text();
        return false;
    }

    return true;
}

bool MigrationManager::runIntegrityCheck(const QString &connectionName)
{
    QSqlDatabase db = QSqlDatabase::database(connectionName, false);
    if (!db.isValid() || !db.isOpen()) {
        m_lastError = QStringLiteral("Database connection is not open");
        return false;
    }

    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("PRAGMA quick_check"))) {
        m_lastError = query.lastError().text();
        return false;
    }

    if (!query.next()) {
        m_lastError = QStringLiteral("PRAGMA quick_check returned no rows");
        return false;
    }

    const QString result = query.value(0).toString().trimmed();
    if (result.compare(QStringLiteral("ok"), Qt::CaseInsensitive) != 0) {
        m_lastError = QStringLiteral("Integrity check failed: %1").arg(result);
        return false;
    }

    return true;
}
