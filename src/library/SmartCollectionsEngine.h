#ifndef SMARTCOLLECTIONSENGINE_H
#define SMARTCOLLECTIONSENGINE_H

#include <QObject>
#include <QList>
#include <QPair>
#include <QString>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <QtGlobal>

class QJsonObject;
class QJsonValue;
class QSqlDatabase;
class QSqlQuery;

class SmartCollectionsEngine : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool enabled READ isEnabled NOTIFY enabledChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(int revision READ revision NOTIFY collectionsChanged)

public:
    explicit SmartCollectionsEngine(QObject *parent = nullptr);
    ~SmartCollectionsEngine() override;

    void configure(bool enabled, const QString &databasePath);

    bool isEnabled() const { return m_enabled; }
    QString databasePath() const { return m_databasePath; }
    QString lastError() const { return m_lastError; }
    int revision() const { return m_revision; }

    Q_INVOKABLE QVariantList listCollections() const;
    Q_INVOKABLE QVariantMap getCollection(int id) const;
    Q_INVOKABLE int createCollection(const QString &name,
                                     const QString &definitionJson,
                                     const QString &sortJson,
                                     int limitCount,
                                     bool enabled,
                                     bool pinned);
    Q_INVOKABLE bool updateCollection(int id,
                                      const QString &name,
                                      const QString &definitionJson,
                                      const QString &sortJson,
                                      int limitCount,
                                      bool enabled,
                                      bool pinned);
    Q_INVOKABLE bool deleteCollection(int id);
    Q_INVOKABLE QVariantList resolveCollectionTracks(int id, int overrideLimit = -1) const;
    Q_INVOKABLE QVariantMap loadContextPlaybackProgress() const;
    Q_INVOKABLE bool saveContextPlaybackProgress(const QVariantMap &payload);

signals:
    void enabledChanged();
    void lastErrorChanged();
    void collectionsChanged();

private:
    struct CompiledWhere {
        QString sql;
        QList<QPair<QString, QVariant>> bindings;
    };

    struct CompiledOrder {
        QString sql;
    };

    struct CompileContext {
        int nextBindId = 1;
        qint64 nowMs = 0;
        bool nowBound = false;
        QList<QPair<QString, QVariant>> bindings;
    };

    bool ensureConnection() const;
    void closeConnection() const;
    bool ensureDefaultCollections();
    bool collectionsTableIsEmpty(bool *isEmptyOut) const;
    bool insertDefaultCollections() const;
    bool validateDefinitionJson(const QString &definitionJson, QString *errorOut) const;
    bool compileDefinition(const QString &definitionJson,
                           qint64 nowMs,
                           CompiledWhere *compiledOut,
                           QString *errorOut) const;
    bool compileNode(const QJsonValue &nodeValue,
                     CompileContext *context,
                     QString *sqlOut,
                     QString *errorOut) const;
    bool compileRule(const QJsonObject &ruleObject,
                     CompileContext *context,
                     QString *sqlOut,
                     QString *errorOut) const;
    QString compileSort(const QString &sortJson, QString *errorOut) const;
    QString bindValue(CompileContext *context, const QVariant &value) const;
    static QString escapeLikePattern(QString value);
    static QVector<QString> splitMatchTokens(const QString &value);
    static QVariantMap collectionRecordFromQuery(const QSqlQuery &query);
    static QVariantMap trackRecordFromQuery(const QSqlQuery &query);
    static QVariantMap normalizeProgressState(const QVariantMap &rawState);
    bool pruneContextPlaybackProgressTable(QSqlDatabase *db) const;
    void setLastError(const QString &error) const;

    mutable bool m_enabled = false;
    mutable QString m_databasePath;
    mutable QString m_connectionName;
    mutable QString m_lastError;
    mutable bool m_defaultsEnsured = false;
    mutable int m_revision = 0;
};

#endif // SMARTCOLLECTIONSENGINE_H
