#ifndef MIGRATIONMANAGER_H
#define MIGRATIONMANAGER_H

#include <QString>

class QSqlDatabase;

class MigrationManager
{
public:
    static constexpr int kLatestVersion = 3;

    bool migrate(const QString &connectionName);
    QString lastError() const { return m_lastError; }

private:
    bool readUserVersion(const QString &connectionName, int *versionOut);
    bool applyMigrationStep(QSqlDatabase *db, int targetVersion);
    bool applyMigrationV1(QSqlDatabase *db);
    bool applyMigrationV2(QSqlDatabase *db);
    bool applyMigrationV3(QSqlDatabase *db);
    bool executeStatement(QSqlDatabase *db, const QString &sql);
    bool runIntegrityCheck(const QString &connectionName);

    QString m_lastError;
};

#endif // MIGRATIONMANAGER_H
