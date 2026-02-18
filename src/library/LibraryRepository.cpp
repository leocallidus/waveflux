#include "library/LibraryRepository.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QMetaObject>
#include <QPointer>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>
#include <QVariant>

namespace {
struct DbTrackRow {
    QString canonicalPath;
    QString fileName;
    QString title;
    QString artist;
    QString album;
    qint64 durationMs = 0;
    QString format;
    int bitrate = 0;
    int sampleRate = 0;
    int bitDepth = 0;
    QString albumArtUri;
    qint64 fileSizeBytes = 0;
    qint64 mtimeMs = 0;
    qint64 addedAtMs = 0;
    qint64 updatedAtMs = 0;
};

QString normalizePath(const QString &path)
{
    return QDir::cleanPath(QDir::fromNativeSeparators(path.trimmed()));
}

QString absolutePath(const QString &path)
{
    const QFileInfo info(path);
    if (info.isAbsolute()) {
        return normalizePath(info.absoluteFilePath());
    }
    return normalizePath(QFileInfo(QDir::current(), path).absoluteFilePath());
}

QString canonicalizePath(const QString &path)
{
    const QString normalized = normalizePath(path);
    if (normalized.isEmpty()) {
        return {};
    }

    const QFileInfo info(normalized);
    const QString canonical = normalizePath(info.canonicalFilePath());
    if (!canonical.isEmpty()) {
        return canonical;
    }

    return absolutePath(normalized);
}

QString canonicalizeDirectoryPath(const QString &path)
{
    QString canonical = canonicalizePath(path);
    if (!canonical.isEmpty()) {
        return canonical;
    }
    return absolutePath(path);
}

QString escapeLikePattern(QString value)
{
    value.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
    value.replace(QStringLiteral("%"), QStringLiteral("\\%"));
    value.replace(QStringLiteral("_"), QStringLiteral("\\_"));
    return value;
}
} // namespace

class LibraryRepository::Worker : public QObject
{
public:
    explicit Worker(LibraryRepository *owner)
        : m_owner(owner)
    {
    }

    ~Worker() override
    {
        closeConnection();
    }

    void configure(bool enabled, const QString &databasePath)
    {
        m_enabled = enabled;
        m_databasePath = databasePath.trimmed();

        if (!m_enabled) {
            closeConnection();
            return;
        }

        if (m_databasePath.isEmpty()) {
            reportError(QStringLiteral("configure"),
                        QStringLiteral("SQLite library ingest enabled but database path is empty"));
            closeConnection();
        }
    }

    void upsertTracks(const QVector<LibraryTrackUpsertData> &tracks)
    {
        if (!m_enabled || tracks.isEmpty()) {
            return;
        }
        if (!ensureConnection()) {
            return;
        }

        static const QString kUpsertSql = QStringLiteral(
            "INSERT INTO tracks ("
            "canonical_path, file_name, title, artist, album, duration_ms, format, bitrate, "
            "sample_rate, bit_depth, album_art_uri, file_size_bytes, mtime_ms, added_at_ms, "
            "updated_at_ms, deleted_at_ms) "
            "VALUES ("
            ":canonical_path, :file_name, :title, :artist, :album, :duration_ms, :format, :bitrate, "
            ":sample_rate, :bit_depth, :album_art_uri, :file_size_bytes, :mtime_ms, :added_at_ms, "
            ":updated_at_ms, NULL) "
            "ON CONFLICT(canonical_path) DO UPDATE SET "
            "file_name = excluded.file_name, "
            "title = CASE "
            "    WHEN excluded.title IS NOT NULL AND length(trim(excluded.title)) > 0 "
            "    THEN excluded.title "
            "    ELSE tracks.title "
            "END, "
            "artist = CASE "
            "    WHEN excluded.artist IS NOT NULL AND length(trim(excluded.artist)) > 0 "
            "    THEN excluded.artist "
            "    ELSE tracks.artist "
            "END, "
            "album = CASE "
            "    WHEN excluded.album IS NOT NULL AND length(trim(excluded.album)) > 0 "
            "    THEN excluded.album "
            "    ELSE tracks.album "
            "END, "
            "duration_ms = CASE "
            "    WHEN excluded.duration_ms > 0 THEN excluded.duration_ms ELSE tracks.duration_ms "
            "END, "
            "format = CASE "
            "    WHEN excluded.format IS NOT NULL AND length(trim(excluded.format)) > 0 "
            "    THEN excluded.format "
            "    ELSE tracks.format "
            "END, "
            "bitrate = CASE "
            "    WHEN excluded.bitrate > 0 THEN excluded.bitrate ELSE tracks.bitrate "
            "END, "
            "sample_rate = CASE "
            "    WHEN excluded.sample_rate > 0 THEN excluded.sample_rate ELSE tracks.sample_rate "
            "END, "
            "bit_depth = CASE "
            "    WHEN excluded.bit_depth > 0 THEN excluded.bit_depth ELSE tracks.bit_depth "
            "END, "
            "album_art_uri = CASE "
            "    WHEN excluded.album_art_uri IS NOT NULL AND length(trim(excluded.album_art_uri)) > 0 "
            "    THEN excluded.album_art_uri "
            "    ELSE tracks.album_art_uri "
            "END, "
            "file_size_bytes = excluded.file_size_bytes, "
            "mtime_ms = excluded.mtime_ms, "
            "updated_at_ms = excluded.updated_at_ms, "
            "deleted_at_ms = NULL");

        QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
        if (!db.transaction()) {
            reportError(QStringLiteral("upsertTracks"), db.lastError().text());
            return;
        }

        QSqlQuery query(db);
        if (!query.prepare(kUpsertSql)) {
            reportError(QStringLiteral("upsertTracks"), query.lastError().text());
            db.rollback();
            return;
        }

        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        for (const LibraryTrackUpsertData &track : tracks) {
            const DbTrackRow row = buildRow(track, nowMs);
            if (row.canonicalPath.isEmpty()) {
                continue;
            }

            query.bindValue(QStringLiteral(":canonical_path"), row.canonicalPath);
            query.bindValue(QStringLiteral(":file_name"), row.fileName);
            query.bindValue(QStringLiteral(":title"), row.title);
            query.bindValue(QStringLiteral(":artist"), row.artist);
            query.bindValue(QStringLiteral(":album"), row.album);
            query.bindValue(QStringLiteral(":duration_ms"), row.durationMs);
            query.bindValue(QStringLiteral(":format"), row.format);
            query.bindValue(QStringLiteral(":bitrate"), row.bitrate);
            query.bindValue(QStringLiteral(":sample_rate"), row.sampleRate);
            query.bindValue(QStringLiteral(":bit_depth"), row.bitDepth);
            query.bindValue(QStringLiteral(":album_art_uri"), row.albumArtUri);
            query.bindValue(QStringLiteral(":file_size_bytes"), row.fileSizeBytes);
            query.bindValue(QStringLiteral(":mtime_ms"), row.mtimeMs);
            query.bindValue(QStringLiteral(":added_at_ms"), row.addedAtMs);
            query.bindValue(QStringLiteral(":updated_at_ms"), row.updatedAtMs);

            if (!query.exec()) {
                const QString message = QStringLiteral("%1 (path=%2)")
                    .arg(query.lastError().text(), row.canonicalPath);
                reportError(QStringLiteral("upsertTracks"), message);
                db.rollback();
                return;
            }
        }

        if (!db.commit()) {
            reportError(QStringLiteral("upsertTracks"), db.lastError().text());
        }
    }

    void softDeleteTrack(const QString &filePath)
    {
        if (!m_enabled || filePath.trimmed().isEmpty()) {
            return;
        }
        if (!ensureConnection()) {
            return;
        }

        const QString canonicalPath = canonicalizePath(filePath);
        if (canonicalPath.isEmpty()) {
            return;
        }

        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "UPDATE tracks "
            "SET deleted_at_ms = :deleted_at_ms, updated_at_ms = :updated_at_ms "
            "WHERE canonical_path = :canonical_path AND deleted_at_ms IS NULL"));
        query.bindValue(QStringLiteral(":deleted_at_ms"), nowMs);
        query.bindValue(QStringLiteral(":updated_at_ms"), nowMs);
        query.bindValue(QStringLiteral(":canonical_path"), canonicalPath);
        if (!query.exec()) {
            reportError(QStringLiteral("softDeleteTrack"), query.lastError().text());
        }
    }

    void softDeleteAll()
    {
        if (!m_enabled) {
            return;
        }
        if (!ensureConnection()) {
            return;
        }

        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "UPDATE tracks "
            "SET deleted_at_ms = :deleted_at_ms, updated_at_ms = :updated_at_ms "
            "WHERE deleted_at_ms IS NULL"));
        query.bindValue(QStringLiteral(":deleted_at_ms"), nowMs);
        query.bindValue(QStringLiteral(":updated_at_ms"), nowMs);
        if (!query.exec()) {
            reportError(QStringLiteral("softDeleteAll"), query.lastError().text());
        }
    }

    void reconcileFolderScan(const QString &folderPath, const QStringList &presentFilePaths)
    {
        if (!m_enabled || folderPath.trimmed().isEmpty()) {
            return;
        }
        if (!ensureConnection()) {
            return;
        }

        const QString canonicalRoot = canonicalizeDirectoryPath(folderPath);
        if (canonicalRoot.isEmpty()) {
            return;
        }

        QSet<QString> presentCanonicalPaths;
        presentCanonicalPaths.reserve(presentFilePaths.size());
        for (const QString &path : presentFilePaths) {
            const QString canonical = canonicalizePath(path);
            if (!canonical.isEmpty()) {
                presentCanonicalPaths.insert(canonical);
            }
        }

        QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
        if (!db.transaction()) {
            reportError(QStringLiteral("reconcileFolderScan"), db.lastError().text());
            return;
        }

        QSqlQuery query(db);
        if (!query.exec(QStringLiteral(
                "CREATE TEMP TABLE IF NOT EXISTS present_paths ("
                "canonical_path TEXT PRIMARY KEY"
                ") WITHOUT ROWID"))) {
            reportError(QStringLiteral("reconcileFolderScan"), query.lastError().text());
            db.rollback();
            return;
        }

        if (!query.exec(QStringLiteral("DELETE FROM present_paths"))) {
            reportError(QStringLiteral("reconcileFolderScan"), query.lastError().text());
            db.rollback();
            return;
        }

        if (!presentCanonicalPaths.isEmpty()) {
            if (!query.prepare(QStringLiteral(
                    "INSERT OR IGNORE INTO present_paths(canonical_path) VALUES (:canonical_path)"))) {
                reportError(QStringLiteral("reconcileFolderScan"), query.lastError().text());
                db.rollback();
                return;
            }

            for (const QString &canonicalPath : presentCanonicalPaths) {
                query.bindValue(QStringLiteral(":canonical_path"), canonicalPath);
                if (!query.exec()) {
                    reportError(QStringLiteral("reconcileFolderScan"), query.lastError().text());
                    db.rollback();
                    return;
                }
            }
        }

        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        const QString prefix = canonicalRoot == QStringLiteral("/")
            ? QStringLiteral("/")
            : canonicalRoot + QStringLiteral("/");
        const QString likePattern = escapeLikePattern(prefix) + QStringLiteral("%");

        if (presentCanonicalPaths.isEmpty()) {
            if (!query.prepare(QStringLiteral(
                    "UPDATE tracks "
                    "SET deleted_at_ms = :deleted_at_ms, updated_at_ms = :updated_at_ms "
                    "WHERE deleted_at_ms IS NULL "
                    "  AND canonical_path LIKE :path_prefix ESCAPE '\\'"))) {
                reportError(QStringLiteral("reconcileFolderScan"), query.lastError().text());
                db.rollback();
                return;
            }
        } else {
            if (!query.prepare(QStringLiteral(
                    "UPDATE tracks "
                    "SET deleted_at_ms = :deleted_at_ms, updated_at_ms = :updated_at_ms "
                    "WHERE deleted_at_ms IS NULL "
                    "  AND canonical_path LIKE :path_prefix ESCAPE '\\' "
                    "  AND NOT EXISTS ("
                    "      SELECT 1 FROM present_paths p "
                    "      WHERE p.canonical_path = tracks.canonical_path"
                    "  )"))) {
                reportError(QStringLiteral("reconcileFolderScan"), query.lastError().text());
                db.rollback();
                return;
            }
        }

        query.bindValue(QStringLiteral(":deleted_at_ms"), nowMs);
        query.bindValue(QStringLiteral(":updated_at_ms"), nowMs);
        query.bindValue(QStringLiteral(":path_prefix"), likePattern);
        if (!query.exec()) {
            reportError(QStringLiteral("reconcileFolderScan"), query.lastError().text());
            db.rollback();
            return;
        }

        if (!db.commit()) {
            reportError(QStringLiteral("reconcileFolderScan"), db.lastError().text());
        }
    }

    bool writePlaybackEvents(const QVector<LibraryPlaybackEventData> &events)
    {
        if (!m_enabled || events.isEmpty()) {
            return true;
        }
        if (!ensureConnection()) {
            return false;
        }

        QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
        if (!db.transaction()) {
            reportError(QStringLiteral("writePlaybackEvents"), db.lastError().text());
            return false;
        }

        QSqlQuery findTrackIdQuery(db);
        if (!findTrackIdQuery.prepare(QStringLiteral(
                "SELECT id FROM tracks WHERE canonical_path = :canonical_path LIMIT 1"))) {
            reportError(QStringLiteral("writePlaybackEvents"), findTrackIdQuery.lastError().text());
            db.rollback();
            return false;
        }

        QSqlQuery insertPlayEventQuery(db);
        if (!insertPlayEventQuery.prepare(QStringLiteral(
                "INSERT INTO play_events ("
                "track_id, started_at_ms, ended_at_ms, listen_ms, completion_ratio, source, "
                "was_skipped, session_id"
                ") VALUES ("
                ":track_id, :started_at_ms, :ended_at_ms, :listen_ms, :completion_ratio, :source, "
                ":was_skipped, :session_id"
                ")"))) {
            reportError(QStringLiteral("writePlaybackEvents"), insertPlayEventQuery.lastError().text());
            db.rollback();
            return false;
        }

        QSqlQuery upsertTrackStatsQuery(db);
        if (!upsertTrackStatsQuery.prepare(QStringLiteral(
                "INSERT INTO track_stats ("
                "track_id, play_count, skip_count, completion_count, total_listen_ms, "
                "last_played_at_ms, last_skipped_at_ms"
                ") VALUES ("
                ":track_id, :play_count, :skip_count, :completion_count, :total_listen_ms, "
                ":last_played_at_ms, :last_skipped_at_ms"
                ") ON CONFLICT(track_id) DO UPDATE SET "
                "play_count = track_stats.play_count + excluded.play_count, "
                "skip_count = track_stats.skip_count + excluded.skip_count, "
                "completion_count = track_stats.completion_count + excluded.completion_count, "
                "total_listen_ms = track_stats.total_listen_ms + excluded.total_listen_ms, "
                "last_played_at_ms = CASE "
                "    WHEN excluded.last_played_at_ms IS NULL THEN track_stats.last_played_at_ms "
                "    WHEN track_stats.last_played_at_ms IS NULL THEN excluded.last_played_at_ms "
                "    WHEN excluded.last_played_at_ms > track_stats.last_played_at_ms "
                "    THEN excluded.last_played_at_ms "
                "    ELSE track_stats.last_played_at_ms "
                "END, "
                "last_skipped_at_ms = CASE "
                "    WHEN excluded.last_skipped_at_ms IS NULL THEN track_stats.last_skipped_at_ms "
                "    WHEN track_stats.last_skipped_at_ms IS NULL THEN excluded.last_skipped_at_ms "
                "    WHEN excluded.last_skipped_at_ms > track_stats.last_skipped_at_ms "
                "    THEN excluded.last_skipped_at_ms "
                "    ELSE track_stats.last_skipped_at_ms "
                "END"))) {
            reportError(QStringLiteral("writePlaybackEvents"), upsertTrackStatsQuery.lastError().text());
            db.rollback();
            return false;
        }

        for (const LibraryPlaybackEventData &event : events) {
            const QString canonicalPath = canonicalizePath(event.filePath);
            if (canonicalPath.isEmpty()) {
                continue;
            }

            findTrackIdQuery.bindValue(QStringLiteral(":canonical_path"), canonicalPath);
            if (!findTrackIdQuery.exec()) {
                const QString message = QStringLiteral("%1 (path=%2)")
                    .arg(findTrackIdQuery.lastError().text(), canonicalPath);
                reportError(QStringLiteral("writePlaybackEvents"), message);
                db.rollback();
                return false;
            }

            if (!findTrackIdQuery.next()) {
                continue;
            }
            const qint64 trackId = findTrackIdQuery.value(0).toLongLong();

            const qint64 startedAtMs = qMax<qint64>(0, event.startedAtMs);
            const qint64 endedAtMs = qMax(startedAtMs, event.endedAtMs);
            const qint64 listenMs = qMax<qint64>(0, event.listenMs);
            const double completionRatio = qBound(0.0, event.completionRatio, 1.0);
            const bool wasSkipped = event.wasSkipped;
            const bool wasCompleted = event.wasCompleted;

            insertPlayEventQuery.bindValue(QStringLiteral(":track_id"), trackId);
            insertPlayEventQuery.bindValue(QStringLiteral(":started_at_ms"), startedAtMs);
            insertPlayEventQuery.bindValue(QStringLiteral(":ended_at_ms"), endedAtMs);
            insertPlayEventQuery.bindValue(QStringLiteral(":listen_ms"), listenMs);
            insertPlayEventQuery.bindValue(QStringLiteral(":completion_ratio"), completionRatio);
            insertPlayEventQuery.bindValue(QStringLiteral(":source"), event.source);
            insertPlayEventQuery.bindValue(QStringLiteral(":was_skipped"), wasSkipped ? 1 : 0);
            insertPlayEventQuery.bindValue(QStringLiteral(":session_id"), event.sessionId);
            if (!insertPlayEventQuery.exec()) {
                const QString message = QStringLiteral("%1 (path=%2)")
                    .arg(insertPlayEventQuery.lastError().text(), canonicalPath);
                reportError(QStringLiteral("writePlaybackEvents"), message);
                db.rollback();
                return false;
            }

            upsertTrackStatsQuery.bindValue(QStringLiteral(":track_id"), trackId);
            upsertTrackStatsQuery.bindValue(QStringLiteral(":play_count"), 1);
            upsertTrackStatsQuery.bindValue(QStringLiteral(":skip_count"), wasSkipped ? 1 : 0);
            upsertTrackStatsQuery.bindValue(QStringLiteral(":completion_count"), wasCompleted ? 1 : 0);
            upsertTrackStatsQuery.bindValue(QStringLiteral(":total_listen_ms"), listenMs);
            upsertTrackStatsQuery.bindValue(QStringLiteral(":last_played_at_ms"), endedAtMs);
            if (wasSkipped) {
                upsertTrackStatsQuery.bindValue(QStringLiteral(":last_skipped_at_ms"), endedAtMs);
            } else {
                upsertTrackStatsQuery.bindValue(QStringLiteral(":last_skipped_at_ms"), QVariant());
            }

            if (!upsertTrackStatsQuery.exec()) {
                const QString message = QStringLiteral("%1 (path=%2)")
                    .arg(upsertTrackStatsQuery.lastError().text(), canonicalPath);
                reportError(QStringLiteral("writePlaybackEvents"), message);
                db.rollback();
                return false;
            }
        }

        if (!db.commit()) {
            reportError(QStringLiteral("writePlaybackEvents"), db.lastError().text());
            return false;
        }
        return true;
    }

private:
    DbTrackRow buildRow(const LibraryTrackUpsertData &track, qint64 nowMs) const
    {
        DbTrackRow row;
        row.canonicalPath = canonicalizePath(track.filePath);
        if (row.canonicalPath.isEmpty()) {
            return row;
        }

        const QFileInfo info(row.canonicalPath);
        row.fileName = info.fileName();
        if (row.fileName.isEmpty()) {
            row.fileName = QFileInfo(track.filePath).fileName();
        }
        if (row.fileName.isEmpty()) {
            row.fileName = row.canonicalPath;
        }

        row.title = track.title.trimmed();
        row.artist = track.artist.trimmed();
        row.album = track.album.trimmed();
        row.durationMs = qMax<qint64>(0, track.durationMs);
        row.format = track.format.trimmed();
        row.bitrate = qMax(0, track.bitrate);
        row.sampleRate = qMax(0, track.sampleRate);
        row.bitDepth = qMax(0, track.bitDepth);
        row.albumArtUri = track.albumArtUri.trimmed();

        if (info.exists()) {
            row.fileSizeBytes = info.size();
            row.mtimeMs = info.lastModified().toMSecsSinceEpoch();
        }
        row.addedAtMs = track.addedAtMs > 0 ? track.addedAtMs : nowMs;
        row.updatedAtMs = nowMs;
        return row;
    }

    bool ensureConnection()
    {
        if (!m_enabled) {
            return false;
        }
        if (m_databasePath.isEmpty()) {
            reportError(QStringLiteral("ensureConnection"), QStringLiteral("Database path is empty"));
            return false;
        }

        if (m_connectionName.isEmpty()) {
            m_connectionName = QStringLiteral("waveflux-library-writer-%1")
                .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));
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
            reportError(QStringLiteral("ensureConnection"), message);
            return false;
        }

        static const QStringList kPragmas = {
            QStringLiteral("PRAGMA foreign_keys = ON"),
            QStringLiteral("PRAGMA journal_mode = WAL"),
            QStringLiteral("PRAGMA synchronous = NORMAL"),
            QStringLiteral("PRAGMA temp_store = MEMORY"),
            QStringLiteral("PRAGMA busy_timeout = 3000")
        };

        QSqlQuery query(db);
        for (const QString &pragma : kPragmas) {
            if (!query.exec(pragma)) {
                const QString message = query.lastError().text();
                db.close();
                db = QSqlDatabase();
                QSqlDatabase::removeDatabase(m_connectionName);
                reportError(QStringLiteral("ensureConnection"), message);
                return false;
            }
        }

        return true;
    }

    void closeConnection()
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

    void reportError(const QString &operation, const QString &message)
    {
        QPointer<LibraryRepository> ownerGuard(m_owner);
        QMetaObject::invokeMethod(
            m_owner,
            [ownerGuard, operation, message]() {
                if (!ownerGuard) {
                    return;
                }
                ownerGuard->reportError(operation, message);
            },
            Qt::QueuedConnection);
    }

    LibraryRepository *m_owner = nullptr;
    bool m_enabled = false;
    QString m_databasePath;
    QString m_connectionName;
};

LibraryRepository::LibraryRepository(QObject *parent)
    : QObject(parent)
{
    m_thread = new QThread(this);
    m_worker = new Worker(this);
    m_worker->moveToThread(m_thread);
    m_thread->start();
}

LibraryRepository::~LibraryRepository()
{
    if (m_worker) {
        Worker *worker = m_worker;
        m_worker = nullptr;

        if (m_thread && m_thread->isRunning()) {
            const bool deletedInWorkerThread = QMetaObject::invokeMethod(
                worker,
                [worker]() {
                    delete worker;
                },
                Qt::BlockingQueuedConnection);
            if (!deletedInWorkerThread) {
                delete worker;
            }
        } else {
            delete worker;
        }
    }

    if (m_thread && m_thread->isRunning()) {
        m_thread->quit();
        m_thread->wait();
    }
}

void LibraryRepository::configure(bool enabled, const QString &databasePath)
{
    m_enabled = enabled;
    m_databasePath = databasePath.trimmed();

    if (!m_worker) {
        return;
    }

    Worker *worker = m_worker;
    QMetaObject::invokeMethod(
        m_worker,
        [worker, enabled, databasePath]() {
            if (!worker) {
                return;
            }
            worker->configure(enabled, databasePath);
        },
        Qt::QueuedConnection);
}

void LibraryRepository::enqueueUpsertTrack(const LibraryTrackUpsertData &track)
{
    if (track.filePath.trimmed().isEmpty()) {
        return;
    }

    QVector<LibraryTrackUpsertData> batch;
    batch.reserve(1);
    batch.append(track);
    enqueueUpsertTracks(batch);
}

void LibraryRepository::enqueueUpsertTracks(const QVector<LibraryTrackUpsertData> &tracks)
{
    if (tracks.isEmpty() || !canEnqueue()) {
        return;
    }

    Worker *worker = m_worker;
    QMetaObject::invokeMethod(
        m_worker,
        [worker, tracks]() {
            if (!worker) {
                return;
            }
            worker->upsertTracks(tracks);
        },
        Qt::QueuedConnection);
}

void LibraryRepository::enqueueSoftDeleteTrack(const QString &filePath)
{
    if (filePath.trimmed().isEmpty() || !canEnqueue()) {
        return;
    }

    Worker *worker = m_worker;
    QMetaObject::invokeMethod(
        m_worker,
        [worker, filePath]() {
            if (!worker) {
                return;
            }
            worker->softDeleteTrack(filePath);
        },
        Qt::QueuedConnection);
}

void LibraryRepository::enqueueSoftDeleteAll()
{
    if (!canEnqueue()) {
        return;
    }

    Worker *worker = m_worker;
    QMetaObject::invokeMethod(
        m_worker,
        [worker]() {
            if (!worker) {
                return;
            }
            worker->softDeleteAll();
        },
        Qt::QueuedConnection);
}

void LibraryRepository::enqueueReconcileFolderScan(const QString &folderPath,
                                                   const QStringList &presentFilePaths)
{
    if (folderPath.trimmed().isEmpty() || !canEnqueue()) {
        return;
    }

    Worker *worker = m_worker;
    QMetaObject::invokeMethod(
        m_worker,
        [worker, folderPath, presentFilePaths]() {
            if (!worker) {
                return;
            }
            worker->reconcileFolderScan(folderPath, presentFilePaths);
        },
        Qt::QueuedConnection);
}

void LibraryRepository::enqueuePlaybackEvents(const QVector<LibraryPlaybackEventData> &events)
{
    if (events.isEmpty() || !canEnqueue()) {
        return;
    }

    Worker *worker = m_worker;
    QMetaObject::invokeMethod(
        m_worker,
        [worker, events]() {
            if (!worker) {
                return;
            }
            worker->writePlaybackEvents(events);
        },
        Qt::QueuedConnection);
}

bool LibraryRepository::writePlaybackEventsBlocking(const QVector<LibraryPlaybackEventData> &events)
{
    if (events.isEmpty()) {
        return true;
    }
    if (!canEnqueue()) {
        return false;
    }
    if (m_thread && QThread::currentThread() == m_thread) {
        return m_worker->writePlaybackEvents(events);
    }

    bool success = false;
    Worker *worker = m_worker;
    const bool invoked = QMetaObject::invokeMethod(
        m_worker,
        [worker, events, &success]() {
            if (!worker) {
                success = false;
                return;
            }
            success = worker->writePlaybackEvents(events);
        },
        Qt::BlockingQueuedConnection);
    return invoked && success;
}

bool LibraryRepository::canEnqueue() const
{
    return m_worker && m_enabled && !m_databasePath.isEmpty();
}

void LibraryRepository::reportError(const QString &operation, const QString &message)
{
    m_lastError = message;
    emit errorOccurred(operation, message);
}
