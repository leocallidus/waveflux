#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QString>

class QSqlDatabase;

class DatabaseManager
{
public:
    DatabaseManager();
    ~DatabaseManager();

    bool openDefaultDatabase();
    bool openDatabase(const QString &databasePath);
    void close();

    bool isOpen() const;
    QString connectionName() const { return m_connectionName; }
    QString databasePath() const { return m_databasePath; }
    QString lastError() const { return m_lastError; }

    static QString defaultDatabasePath();

private:
    bool configurePragmas(QSqlDatabase *db);

    QString m_connectionName;
    QString m_databasePath;
    QString m_lastError;
};

#endif // DATABASEMANAGER_H
