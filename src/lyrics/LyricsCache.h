#pragma once

#include "LyricsTypes.h"
#include <QMutex>
#include <QSqlDatabase>
#include <QString>
#include <optional>

namespace WaveFlux::Lyrics {

class LyricsCache {
public:
    LyricsCache();
    ~LyricsCache();

    bool init(const QString &customDbPath = QString());
    void close();

    std::optional<LyricsDocument> getDocument(const QString &docId);
    bool storeDocument(const LyricsDocument &doc);

    // Query results (cached search queries)
    struct QueryResult {
        QString documentId;
        bool isNegative = false;
        bool expired = false;
    };
    std::optional<QueryResult> getQueryResult(const QString &queryKey);
    bool storeQueryResult(const QString &queryKey, const QString &docId, bool isNegative);

    // Track preferences & delays
    std::optional<qint64> getUserDelay(const QString &trackKey);
    bool storeUserDelay(const QString &trackKey, qint64 delayMs);

    std::optional<QString> getTrackOverride(const QString &trackKey);
    bool storeTrackOverride(const QString &trackKey, const QString &docId);
    bool clearTrackOverride(const QString &trackKey);

    // Imported documents
    bool storeImportedDocument(const QString &trackKey, const LyricsDocument &doc);
    std::optional<LyricsDocument> getImportedDocument(const QString &trackKey);

    // Maintenance
    void evictIfNeeded();
    void purgeExpired();

private:
    QString m_connectionName;
    mutable QMutex m_mutex;
    bool m_initialized = false;

    QSqlDatabase db() const;
    void ensureSchema();
};

} // namespace WaveFlux::Lyrics
