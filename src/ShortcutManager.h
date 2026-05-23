#ifndef SHORTCUTMANAGER_H
#define SHORTCUTMANAGER_H

#include <QObject>
#include <QHash>
#include <QSettings>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include "ShortcutRegistry.h"

class ShortcutManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int revision READ revision NOTIFY shortcutsChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    explicit ShortcutManager(QObject *parent = nullptr);

    int revision() const { return m_revision; }
    QString lastError() const { return m_lastError; }

    Q_INVOKABLE QVariantList shortcutDefinitions() const;
    Q_INVOKABLE QVariantList shortcutRows() const;
    Q_INVOKABLE bool hasShortcut(const QString &id) const;
    Q_INVOKABLE QString defaultSequence(const QString &id) const;
    Q_INVOKABLE QString customSequence(const QString &id) const;
    Q_INVOKABLE QString effectiveSequence(const QString &id) const;
    Q_INVOKABLE QString displaySequence(const QString &id) const;
    Q_INVOKABLE QString displayDefaultSequence(const QString &id) const;
    Q_INVOKABLE bool shortcutEnabled(const QString &id) const;
    Q_INVOKABLE QVariantMap validateSequence(const QString &id, const QString &sequence) const;
    Q_INVOKABLE QVariantMap conflictReportForSequence(const QString &id, const QString &sequence) const;
    Q_INVOKABLE QVariantList conflictsForSequence(const QString &id, const QString &sequence) const;
    Q_INVOKABLE QVariantMap setCustomSequenceResolvingConflicts(const QString &id,
                                                                const QString &sequence,
                                                                bool replaceConflicts);
    Q_INVOKABLE bool setCustomSequence(const QString &id, const QString &sequence);
    Q_INVOKABLE bool clearCustomSequence(const QString &id);
    Q_INVOKABLE bool resetShortcut(const QString &id);
    Q_INVOKABLE bool resetGroup(const QString &group);
    Q_INVOKABLE bool resetAll();

signals:
    void shortcutsChanged();
    void lastErrorChanged();

private:
    static QString normalizeSequence(const QString &sequence);
    static int sequenceKeyCount(const QString &sequence);
    static QString displaySequenceText(const QString &sequence);
    static QVariantMap definitionToVariantMap(const ShortcutDefinition &definition);
    static bool contextsConflict(const QString &left, const QString &right);
    static QString contextConflictReason(const QString &left, const QString &right);

    const ShortcutDefinition *definitionForId(const QString &id) const;
    QString effectiveSequenceForDefinition(const ShortcutDefinition &definition) const;
    QVariantMap conflictToVariantMap(const ShortcutDefinition &target,
                                     const ShortcutDefinition &conflict) const;
    bool clearConflictForReplacement(const ShortcutDefinition &definition);
    void loadOverrides();
    void saveOverrides();
    void setLastError(const QString &error);
    void markChanged();

    QSettings m_settings;
    QVector<ShortcutDefinition> m_definitions;
    QHash<QString, ShortcutDefinition> m_definitionById;
    QHash<QString, QString> m_overrides;
    int m_revision = 0;
    QString m_lastError;
};

#endif // SHORTCUTMANAGER_H
