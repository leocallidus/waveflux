#include "library/SearchRepository.h"
#include "PerformanceProfiler.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>
#include <QVariant>

namespace {
QString titleLikeCondition(const QString &parameterName)
{
    return QStringLiteral(
        "("
        "LOWER(COALESCE(title, '')) LIKE %1 ESCAPE '\\' "
        "OR LOWER(CASE "
        "    WHEN title IS NOT NULL AND length(trim(title)) > 0 "
        "    THEN COALESCE(artist, '') || ' - ' || title "
        "    ELSE file_name "
        "END) LIKE %1 ESCAPE '\\'"
        ")")
        .arg(parameterName);
}

QString artistLikeCondition(const QString &parameterName)
{
    return QStringLiteral("(LOWER(COALESCE(artist, '')) LIKE %1 ESCAPE '\\')").arg(parameterName);
}

QString albumLikeCondition(const QString &parameterName)
{
    return QStringLiteral("(LOWER(COALESCE(album, '')) LIKE %1 ESCAPE '\\')").arg(parameterName);
}

QString pathLikeCondition(const QString &parameterName)
{
    return QStringLiteral("(LOWER(COALESCE(canonical_path, '')) LIKE %1 ESCAPE '\\')").arg(parameterName);
}
} // namespace

struct SearchRepository::SearchToken {
    enum class Field {
        Any,
        Title,
        Artist,
        Album,
        Path
    };

    Field field = Field::Any;
    QString value;
    bool negated = false;
};

struct SearchRepository::ParsedQuery {
    QVector<SearchToken> tokens;
    int requiredQuickFilters = TrackModel::SearchQuickFilterNone;
    int excludedQuickFilters = TrackModel::SearchQuickFilterNone;
};

void SearchRepository::configure(bool enabled, const QString &databasePath)
{
    const QString normalizedPath = normalizePath(databasePath);
    const bool sameConfig = (m_enabled == enabled) && (normalizePath(m_databasePath) == normalizedPath);
    if (sameConfig) {
        return;
    }

    if (!m_connectionName.isEmpty() && QSqlDatabase::contains(m_connectionName)) {
        QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
        if (db.isValid()) {
            db.close();
        }
        db = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
    }

    m_enabled = enabled;
    m_databasePath = normalizedPath;
    m_lastError.clear();
    m_ftsKnown = false;
    m_ftsAvailable = false;
    m_canonicalPathCache.clear();
}

SearchRepository::Result SearchRepository::evaluate(const Request &request) const
{
    Result result;
    result.usedSqlite = m_enabled && !m_databasePath.isEmpty();
    QElapsedTimer timer;
    timer.start();

    auto recordProfile = [&](const Result &value, bool usedLike) {
        if (PerformanceProfiler *profiler = PerformanceProfiler::instance()) {
            profiler->recordSearchQuery(timer.nsecsElapsed(),
                                        value.usedSqlite,
                                        value.usedFts,
                                        usedLike,
                                        value.success);
        }
    };

    if (!result.usedSqlite) {
        recordProfile(result, false);
        return result;
    }

    if (!ensureConnection()) {
        result.error = m_lastError;
        recordProfile(result, false);
        return result;
    }

    const int effectiveFieldMask = (request.fieldMask == TrackModel::SearchFieldNone)
        ? TrackModel::SearchFieldAll
        : request.fieldMask;
    const int effectiveQuickFilterMask = request.quickFilterMask;

    const ParsedQuery parsed = parseSearchQuery(request.normalizedQuery.trimmed());

    QSet<QString> matchedCanonicalPaths;
    bool usedFts = false;
    bool usedLike = false;
    QString queryError;
    if (!queryMatchingCanonicalPaths(parsed,
                                     effectiveFieldMask,
                                     effectiveQuickFilterMask,
                                     &matchedCanonicalPaths,
                                     &usedFts,
                                     &usedLike,
                                     &queryError)) {
        result.error = queryError;
        setLastError(queryError);
        result.usedFts = usedFts;
        recordProfile(result, usedLike);
        return result;
    }

    result.matches.resize(request.orderedFilePaths.size());
    result.prefixMatches.resize(request.orderedFilePaths.size() + 1);
    result.prefixMatches[0] = 0;

    int matchedCount = 0;
    for (int i = 0; i < request.orderedFilePaths.size(); ++i) {
        const QString normalizedPath = normalizePath(request.orderedFilePaths.at(i));
        auto cacheIt = m_canonicalPathCache.constFind(normalizedPath);
        if (cacheIt == m_canonicalPathCache.constEnd()) {
            const QString canonical = canonicalizePath(normalizedPath);
            m_canonicalPathCache.insert(normalizedPath, canonical);
            cacheIt = m_canonicalPathCache.constFind(normalizedPath);
        }

        const bool matched = cacheIt != m_canonicalPathCache.constEnd() &&
                             !cacheIt.value().isEmpty() &&
                             matchedCanonicalPaths.contains(cacheIt.value());
        result.matches[i] = matched ? 1 : 0;
        if (matched) {
            ++matchedCount;
        }
        result.prefixMatches[i + 1] = matchedCount;
    }

    result.matchCount = matchedCount;
    result.usedFts = usedFts;
    result.success = true;
    setLastError(QString());
    recordProfile(result, usedLike);
    return result;
}

bool SearchRepository::ensureConnection() const
{
    if (!m_enabled) {
        setLastError(QStringLiteral("Search repository is disabled"));
        return false;
    }
    if (m_databasePath.isEmpty()) {
        setLastError(QStringLiteral("Search repository database path is empty"));
        return false;
    }

    if (m_connectionName.isEmpty()) {
        m_connectionName = QStringLiteral("waveflux-search-reader-%1")
            .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));
    }

    if (QSqlDatabase::contains(m_connectionName)) {
        QSqlDatabase existing = QSqlDatabase::database(m_connectionName, false);
        if (existing.isValid() &&
            existing.isOpen() &&
            normalizePath(existing.databaseName()) == normalizePath(m_databasePath)) {
            return true;
        }

        if (existing.isValid()) {
            existing.close();
        }
        existing = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
    }

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    db.setDatabaseName(m_databasePath);
    if (!db.open()) {
        const QString error = db.lastError().text();
        db = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
        setLastError(error);
        return false;
    }

    static const QStringList pragmas = {
        QStringLiteral("PRAGMA foreign_keys = ON"),
        QStringLiteral("PRAGMA query_only = ON"),
        QStringLiteral("PRAGMA busy_timeout = 2000")
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

    m_ftsKnown = false;
    m_ftsAvailable = false;
    return true;
}

bool SearchRepository::detectFtsAvailability() const
{
    if (m_ftsKnown) {
        return m_ftsAvailable;
    }

    QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
    if (!db.isValid() || !db.isOpen()) {
        setLastError(QStringLiteral("Search database connection is not open"));
        return false;
    }

    QSqlQuery query(db);
    if (!query.exec(QStringLiteral(
            "SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = 'tracks_fts' LIMIT 1"))) {
        setLastError(query.lastError().text());
        return false;
    }

    m_ftsAvailable = query.next();
    m_ftsKnown = true;
    return m_ftsAvailable;
}

QString SearchRepository::normalizePath(const QString &path)
{
    return QDir::cleanPath(QDir::fromNativeSeparators(path.trimmed()));
}

QString SearchRepository::canonicalizePath(const QString &path)
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

    if (info.isAbsolute()) {
        return normalizePath(info.absoluteFilePath());
    }
    return normalizePath(QFileInfo(QDir::current(), normalized).absoluteFilePath());
}

QString SearchRepository::escapeLikePattern(QString value)
{
    value.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
    value.replace(QStringLiteral("%"), QStringLiteral("\\%"));
    value.replace(QStringLiteral("_"), QStringLiteral("\\_"));
    return value;
}

QString SearchRepository::escapeFtsToken(QString value)
{
    value.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return value;
}

QVector<QString> SearchRepository::splitQueryTokens(const QString &normalizedQuery)
{
    QVector<QString> tokens;
    QString current;
    bool inQuotes = false;

    for (const QChar ch : normalizedQuery) {
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

SearchRepository::ParsedQuery SearchRepository::parseSearchQuery(const QString &normalizedQuery)
{
    ParsedQuery parsed;

    const QVector<QString> rawTokens = splitQueryTokens(normalizedQuery);
    parsed.tokens.reserve(rawTokens.size());

    for (QString token : rawTokens) {
        token = token.trimmed();
        if (token.isEmpty()) {
            continue;
        }

        bool negated = false;
        if (token.size() > 1 && token.startsWith(QLatin1Char('-'))) {
            negated = true;
            token.remove(0, 1);
        }

        const int colonIndex = token.indexOf(QLatin1Char(':'));
        if (colonIndex > 0 && colonIndex + 1 < token.size()) {
            const QString prefix = token.left(colonIndex);
            const QString value = token.mid(colonIndex + 1).trimmed();

            SearchToken::Field field = SearchToken::Field::Any;
            if (prefix == QStringLiteral("title")) {
                field = SearchToken::Field::Title;
            } else if (prefix == QStringLiteral("artist")) {
                field = SearchToken::Field::Artist;
            } else if (prefix == QStringLiteral("album")) {
                field = SearchToken::Field::Album;
            } else if (prefix == QStringLiteral("path")) {
                field = SearchToken::Field::Path;
            }

            if (field != SearchToken::Field::Any) {
                if (!value.isEmpty()) {
                    parsed.tokens.push_back({field, value, negated});
                }
                continue;
            }

            if (prefix == QStringLiteral("is") || prefix == QStringLiteral("filter")) {
                int bit = TrackModel::SearchQuickFilterNone;
                if (value == QStringLiteral("lossless")) {
                    bit = TrackModel::SearchQuickFilterLossless;
                } else if (value == QStringLiteral("hires") ||
                           value == QStringLiteral("hi-res") ||
                           value == QStringLiteral("hi_res")) {
                    bit = TrackModel::SearchQuickFilterHiRes;
                }

                if (bit != TrackModel::SearchQuickFilterNone) {
                    if (negated) {
                        parsed.excludedQuickFilters |= bit;
                    } else {
                        parsed.requiredQuickFilters |= bit;
                    }
                    continue;
                }
            }
        }

        if (!token.isEmpty()) {
            parsed.tokens.push_back({SearchToken::Field::Any, token, negated});
        }
    }

    return parsed;
}

bool SearchRepository::isLosslessFormat(const QString &format)
{
    const QString normalized = format.trimmed().toUpper();
    return normalized == QStringLiteral("FLAC") ||
           normalized == QStringLiteral("WAV") ||
           normalized == QStringLiteral("ALAC") ||
           normalized == QStringLiteral("AIFF");
}

bool SearchRepository::queryMatchingCanonicalPaths(const ParsedQuery &parsed,
                                                   int effectiveFieldMask,
                                                   int effectiveQuickFilterMask,
                                                   QSet<QString> *matchedPathsOut,
                                                   bool *usedFtsOut,
                                                   bool *usedLikeOut,
                                                   QString *errorOut) const
{
    if (!matchedPathsOut || !usedFtsOut || !usedLikeOut || !errorOut) {
        return false;
    }
    matchedPathsOut->clear();
    *usedFtsOut = false;
    *usedLikeOut = false;
    errorOut->clear();

    const int requiredQuickFilters = effectiveQuickFilterMask | parsed.requiredQuickFilters;
    const int excludedQuickFilters = parsed.excludedQuickFilters;

    QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
    if (!db.isValid() || !db.isOpen()) {
        *errorOut = QStringLiteral("Search database connection is not open");
        return false;
    }

    QString ftsError;
    if (detectFtsAvailability() && canUseFtsFastPath(parsed)) {
        QStringList whereClauses = {QStringLiteral("t.deleted_at_ms IS NULL")};
        if (requiredQuickFilters & TrackModel::SearchQuickFilterLossless) {
            whereClauses.push_back(QStringLiteral(
                "UPPER(COALESCE(t.format, '')) IN ('FLAC', 'WAV', 'ALAC', 'AIFF')"));
        }
        if (requiredQuickFilters & TrackModel::SearchQuickFilterHiRes) {
            whereClauses.push_back(QStringLiteral("(t.bit_depth > 16 OR t.sample_rate > 48000)"));
        }
        if (excludedQuickFilters & TrackModel::SearchQuickFilterLossless) {
            whereClauses.push_back(QStringLiteral(
                "UPPER(COALESCE(t.format, '')) NOT IN ('FLAC', 'WAV', 'ALAC', 'AIFF')"));
        }
        if (excludedQuickFilters & TrackModel::SearchQuickFilterHiRes) {
            whereClauses.push_back(QStringLiteral("NOT (t.bit_depth > 16 OR t.sample_rate > 48000)"));
        }

        const QString ftsMatch = buildFtsMatchExpression(parsed, effectiveFieldMask);
        if (!ftsMatch.isEmpty()) {
            QString sql = QStringLiteral(
                "SELECT t.canonical_path "
                "FROM tracks t "
                "JOIN tracks_fts ON tracks_fts.rowid = t.id "
                "WHERE tracks_fts MATCH :fts_match");
            if (!whereClauses.isEmpty()) {
                sql += QStringLiteral(" AND ") + whereClauses.join(QStringLiteral(" AND "));
            }

            QSqlQuery query(db);
            if (query.prepare(sql)) {
                query.bindValue(QStringLiteral(":fts_match"), ftsMatch);
                if (query.exec()) {
                    while (query.next()) {
                        matchedPathsOut->insert(normalizePath(query.value(0).toString()));
                    }
                    *usedFtsOut = true;
                    return true;
                }
                ftsError = query.lastError().text();
            } else {
                ftsError = query.lastError().text();
            }
        }
    }

    const QString sql = buildLikeQuerySql(parsed,
                                          effectiveFieldMask,
                                          requiredQuickFilters,
                                          excludedQuickFilters);
    *usedLikeOut = true;
    QSqlQuery likeQuery(db);
    if (!likeQuery.prepare(sql)) {
        *errorOut = likeQuery.lastError().text();
        return false;
    }
    bindLikeQueryValues(parsed, requiredQuickFilters, excludedQuickFilters, &likeQuery);
    if (!likeQuery.exec()) {
        *errorOut = likeQuery.lastError().text();
        if (!ftsError.isEmpty()) {
            *errorOut += QStringLiteral(" | FTS fallback: ") + ftsError;
        }
        return false;
    }

    while (likeQuery.next()) {
        matchedPathsOut->insert(normalizePath(likeQuery.value(0).toString()));
    }

    return true;
}

bool SearchRepository::canUseFtsFastPath(const ParsedQuery &parsed)
{
    if (parsed.tokens.isEmpty()) {
        return false;
    }

    for (const SearchToken &token : parsed.tokens) {
        if (token.negated || token.value.trimmed().isEmpty()) {
            return false;
        }
    }
    return true;
}

QString SearchRepository::buildFtsMatchExpression(const ParsedQuery &parsed, int effectiveFieldMask)
{
    if (parsed.tokens.isEmpty()) {
        return {};
    }

    QStringList tokenExpressions;
    tokenExpressions.reserve(parsed.tokens.size());

    for (const SearchToken &token : parsed.tokens) {
        const QString value = token.value.trimmed();
        if (value.isEmpty()) {
            continue;
        }

        const QString quoted = QStringLiteral("\"%1\"").arg(escapeFtsToken(value));
        QString expression;
        switch (token.field) {
        case SearchToken::Field::Title:
            expression = QStringLiteral("title : %1").arg(quoted);
            break;
        case SearchToken::Field::Artist:
            expression = QStringLiteral("artist : %1").arg(quoted);
            break;
        case SearchToken::Field::Album:
            expression = QStringLiteral("album : %1").arg(quoted);
            break;
        case SearchToken::Field::Path:
            expression = QStringLiteral("canonical_path : %1").arg(quoted);
            break;
        case SearchToken::Field::Any: {
            QStringList fields;
            if (effectiveFieldMask & TrackModel::SearchFieldTitle) {
                fields.push_back(QStringLiteral("title : %1").arg(quoted));
            }
            if (effectiveFieldMask & TrackModel::SearchFieldArtist) {
                fields.push_back(QStringLiteral("artist : %1").arg(quoted));
            }
            if (effectiveFieldMask & TrackModel::SearchFieldAlbum) {
                fields.push_back(QStringLiteral("album : %1").arg(quoted));
            }
            if (effectiveFieldMask & TrackModel::SearchFieldPath) {
                fields.push_back(QStringLiteral("canonical_path : %1").arg(quoted));
            }
            if (fields.isEmpty()) {
                expression = quoted;
            } else if (fields.size() == 1) {
                expression = fields.front();
            } else {
                expression = QStringLiteral("(%1)").arg(fields.join(QStringLiteral(" OR ")));
            }
            break;
        }
        }

        if (!expression.isEmpty()) {
            tokenExpressions.push_back(expression);
        }
    }

    return tokenExpressions.join(QStringLiteral(" AND "));
}

QString SearchRepository::buildAnyFieldLikeCondition(int effectiveFieldMask, const QString &parameterName)
{
    QStringList fields;
    if (effectiveFieldMask & TrackModel::SearchFieldTitle) {
        fields.push_back(titleLikeCondition(parameterName));
    }
    if (effectiveFieldMask & TrackModel::SearchFieldArtist) {
        fields.push_back(artistLikeCondition(parameterName));
    }
    if (effectiveFieldMask & TrackModel::SearchFieldAlbum) {
        fields.push_back(albumLikeCondition(parameterName));
    }
    if (effectiveFieldMask & TrackModel::SearchFieldPath) {
        fields.push_back(pathLikeCondition(parameterName));
    }

    if (fields.isEmpty()) {
        fields.push_back(titleLikeCondition(parameterName));
        fields.push_back(artistLikeCondition(parameterName));
        fields.push_back(albumLikeCondition(parameterName));
        fields.push_back(pathLikeCondition(parameterName));
    }

    return QStringLiteral("(%1)").arg(fields.join(QStringLiteral(" OR ")));
}

QString SearchRepository::buildLikeQuerySql(const ParsedQuery &parsed,
                                            int effectiveFieldMask,
                                            int requiredQuickFilters,
                                            int excludedQuickFilters)
{
    QStringList conditions;
    conditions.push_back(QStringLiteral("deleted_at_ms IS NULL"));

    if (requiredQuickFilters & TrackModel::SearchQuickFilterLossless) {
        conditions.push_back(QStringLiteral(
            "UPPER(COALESCE(format, '')) IN ('FLAC', 'WAV', 'ALAC', 'AIFF')"));
    }
    if (requiredQuickFilters & TrackModel::SearchQuickFilterHiRes) {
        conditions.push_back(QStringLiteral("(bit_depth > 16 OR sample_rate > 48000)"));
    }
    if (excludedQuickFilters & TrackModel::SearchQuickFilterLossless) {
        conditions.push_back(QStringLiteral(
            "UPPER(COALESCE(format, '')) NOT IN ('FLAC', 'WAV', 'ALAC', 'AIFF')"));
    }
    if (excludedQuickFilters & TrackModel::SearchQuickFilterHiRes) {
        conditions.push_back(QStringLiteral("NOT (bit_depth > 16 OR sample_rate > 48000)"));
    }

    for (int i = 0; i < parsed.tokens.size(); ++i) {
        const SearchToken &token = parsed.tokens.at(i);
        const QString parameterName = QStringLiteral(":token_%1").arg(i);

        QString tokenCondition;
        switch (token.field) {
        case SearchToken::Field::Title:
            tokenCondition = titleLikeCondition(parameterName);
            break;
        case SearchToken::Field::Artist:
            tokenCondition = artistLikeCondition(parameterName);
            break;
        case SearchToken::Field::Album:
            tokenCondition = albumLikeCondition(parameterName);
            break;
        case SearchToken::Field::Path:
            tokenCondition = pathLikeCondition(parameterName);
            break;
        case SearchToken::Field::Any:
            tokenCondition = buildAnyFieldLikeCondition(effectiveFieldMask, parameterName);
            break;
        }

        if (tokenCondition.isEmpty()) {
            continue;
        }

        if (token.negated) {
            conditions.push_back(QStringLiteral("NOT (%1)").arg(tokenCondition));
        } else {
            conditions.push_back(tokenCondition);
        }
    }

    QString sql = QStringLiteral("SELECT canonical_path FROM tracks");
    if (!conditions.isEmpty()) {
        sql += QStringLiteral(" WHERE ") + conditions.join(QStringLiteral(" AND "));
    }
    return sql;
}

void SearchRepository::bindLikeQueryValues(const ParsedQuery &parsed,
                                           int /*requiredQuickFilters*/,
                                           int /*excludedQuickFilters*/,
                                           QSqlQuery *query)
{
    if (!query) {
        return;
    }

    for (int i = 0; i < parsed.tokens.size(); ++i) {
        const QString value = parsed.tokens.at(i).value.trimmed().toLower();
        const QString parameterName = QStringLiteral(":token_%1").arg(i);
        const QString pattern = QStringLiteral("%%%1%%").arg(escapeLikePattern(value));
        query->bindValue(parameterName, pattern);
    }
}

void SearchRepository::setLastError(const QString &error) const
{
    m_lastError = error;
}
