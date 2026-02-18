#include "library/DatabaseManager.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QStandardPaths>

DatabaseManager::DatabaseManager()
    : m_connectionName(QStringLiteral("waveflux-main"))
{
}

DatabaseManager::~DatabaseManager()
{
    close();
}

QString DatabaseManager::defaultDatabasePath()
{
    const QString basePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return basePath + QStringLiteral("/library/waveflux.db");
}

bool DatabaseManager::openDefaultDatabase()
{
    return openDatabase(defaultDatabasePath());
}

bool DatabaseManager::openDatabase(const QString &databasePath)
{
    if (databasePath.trimmed().isEmpty()) {
        m_lastError = QStringLiteral("Empty database path");
        return false;
    }

    close();

    const QFileInfo info(databasePath);
    QDir().mkpath(info.absolutePath());

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    db.setDatabaseName(databasePath);
    if (!db.open()) {
        m_lastError = db.lastError().text();
        db = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
        return false;
    }

    if (!configurePragmas(&db)) {
        const QString pragmaError = m_lastError;
        db.close();
        db = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
        m_lastError = pragmaError;
        return false;
    }

    m_databasePath = databasePath;
    m_lastError.clear();
    return true;
}

void DatabaseManager::close()
{
    if (!QSqlDatabase::contains(m_connectionName)) {
        return;
    }

    {
        QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
        if (db.isValid() && db.isOpen()) {
            db.close();
        }
    }

    QSqlDatabase::removeDatabase(m_connectionName);
}

bool DatabaseManager::isOpen() const
{
    if (!QSqlDatabase::contains(m_connectionName)) {
        return false;
    }

    const QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
    return db.isValid() && db.isOpen();
}

bool DatabaseManager::configurePragmas(QSqlDatabase *db)
{
    if (!db || !db->isValid() || !db->isOpen()) {
        m_lastError = QStringLiteral("Database connection is not open");
        return false;
    }

    const QStringList pragmas = {
        QStringLiteral("PRAGMA foreign_keys = ON"),
        QStringLiteral("PRAGMA journal_mode = WAL"),
        QStringLiteral("PRAGMA synchronous = NORMAL"),
        QStringLiteral("PRAGMA temp_store = MEMORY")
    };

    QSqlQuery query(*db);
    for (const QString &pragma : pragmas) {
        if (!query.exec(pragma)) {
            m_lastError = query.lastError().text();
            return false;
        }
    }

    return true;
}
