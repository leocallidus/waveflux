#ifndef SETTINGSREGISTRY_H
#define SETTINGSREGISTRY_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

struct CategoryDescriptor {
    QString id;
    QString titleKey;
    QString descriptionKey;
    QString iconName;
    QString pageComponentUrl;
    int order = 0;
    QStringList legacySectionIds;
};

struct GroupDescriptor {
    QString id;
    QString categoryId;
    QString titleKey;
    QString descriptionKey;
    int order = 0;
    bool isAdvanced = false;
};

struct SettingDescriptor {
    QString id;
    QString categoryId;
    QString groupId;
    QString titleKey;
    QString descriptionKey;
    QStringList keywordKeys;
    int order = 0;
    QString controlKind;
    QStringList dependencyIds;
    QString capabilityKey;
    QString resetStrategy;
    QVariantMap searchProviderMetadata;
};

class SettingsRegistry : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList categories READ categories CONSTANT)

public:
    explicit SettingsRegistry(QObject *parent = nullptr);
    ~SettingsRegistry() override = default;

    static SettingsRegistry *instance();

    Q_INVOKABLE QVariantList categories() const;
    Q_INVOKABLE QVariantMap category(const QString &id) const;
    Q_INVOKABLE QVariantMap group(const QString &id) const;
    Q_INVOKABLE QVariantList groupsForCategory(const QString &categoryId) const;
    Q_INVOKABLE QVariantList settingsForGroup(const QString &groupId) const;
    Q_INVOKABLE QVariantList settingsForCategory(const QString &categoryId) const;
    Q_INVOKABLE QVariantMap setting(const QString &id) const;
    Q_INVOKABLE QVariantList allSettings() const;
    Q_INVOKABLE QString mapLegacySectionId(const QString &legacySectionId) const;
    Q_INVOKABLE QVariantList search(const QString &query, const QString &language = QString()) const;
    Q_INVOKABLE QStringList validateIntegrity() const;

    const QVector<CategoryDescriptor> &categoryDescriptors() const { return m_categories; }
    const QVector<GroupDescriptor> &groupDescriptors() const { return m_groups; }
    const QVector<SettingDescriptor> &settingDescriptors() const { return m_settings; }

private:
    void initDescriptors();

    QVector<CategoryDescriptor> m_categories;
    QVector<GroupDescriptor> m_groups;
    QVector<SettingDescriptor> m_settings;
};

#endif // SETTINGSREGISTRY_H
