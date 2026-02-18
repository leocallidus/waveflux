#ifndef SEARCHREPOSITORY_H
#define SEARCHREPOSITORY_H

#include "TrackModel.h"

#include <QHash>
#include <QSet>
#include <QString>
#include <QVector>
#include <QtGlobal>

class SearchRepository
{
public:
    struct Request {
        QString normalizedQuery;
        int fieldMask = TrackModel::SearchFieldAll;
        int quickFilterMask = TrackModel::SearchQuickFilterNone;
        QVector<QString> orderedFilePaths;
    };

    struct Result {
        bool usedSqlite = false;
        bool success = false;
        bool usedFts = false;
        QString error;
        QVector<quint8> matches;
        QVector<int> prefixMatches;
        int matchCount = 0;
    };

    void configure(bool enabled, const QString &databasePath);
    bool isEnabled() const { return m_enabled; }
    QString databasePath() const { return m_databasePath; }
    QString lastError() const { return m_lastError; }

    Result evaluate(const Request &request) const;

private:
    struct ParsedQuery;
    struct SearchToken;

    bool ensureConnection() const;
    bool detectFtsAvailability() const;
    static QString normalizePath(const QString &path);
    static QString canonicalizePath(const QString &path);
    static QString escapeLikePattern(QString value);
    static QString escapeFtsToken(QString value);
    static QVector<QString> splitQueryTokens(const QString &normalizedQuery);
    static ParsedQuery parseSearchQuery(const QString &normalizedQuery);
    static bool isLosslessFormat(const QString &format);
    bool queryMatchingCanonicalPaths(const ParsedQuery &parsed,
                                     int effectiveFieldMask,
                                     int effectiveQuickFilterMask,
                                     QSet<QString> *matchedPathsOut,
                                     bool *usedFtsOut,
                                     bool *usedLikeOut,
                                     QString *errorOut) const;
    static bool canUseFtsFastPath(const ParsedQuery &parsed);
    static QString buildFtsMatchExpression(const ParsedQuery &parsed, int effectiveFieldMask);
    static QString buildAnyFieldLikeCondition(int effectiveFieldMask, const QString &parameterName);
    static QString buildLikeQuerySql(const ParsedQuery &parsed,
                                     int effectiveFieldMask,
                                     int requiredQuickFilters,
                                     int excludedQuickFilters);
    static void bindLikeQueryValues(const ParsedQuery &parsed,
                                    int requiredQuickFilters,
                                    int excludedQuickFilters,
                                    class QSqlQuery *query);
    void setLastError(const QString &error) const;

    bool m_enabled = false;
    QString m_databasePath;
    mutable QString m_lastError;
    mutable QString m_connectionName;
    mutable bool m_ftsKnown = false;
    mutable bool m_ftsAvailable = false;
    mutable QHash<QString, QString> m_canonicalPathCache;
};

#endif // SEARCHREPOSITORY_H
