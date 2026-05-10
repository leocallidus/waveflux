#ifndef BATCHAUDIOCONVERTERPRESETMANAGER_H
#define BATCHAUDIOCONVERTERPRESETMANAGER_H

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

class BatchAudioConverterPresetManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QVariantList userPresets READ userPresets NOTIFY presetsChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(int revision READ revision NOTIFY presetsChanged)

public:
    struct Preset {
        QString id;
        QString name;
        QVariantMap settings;
        qint64 updatedAtMs = 0;

        bool operator==(const Preset &other) const = default;
    };

    explicit BatchAudioConverterPresetManager(QObject *parent = nullptr);

    QVariantList userPresets() const;
    QString lastError() const { return m_lastError; }
    int revision() const { return m_revision; }

    Q_INVOKABLE QVariantList listUserPresets() const;
    Q_INVOKABLE QVariantMap getPreset(const QString &presetId) const;
    Q_INVOKABLE QString createUserPreset(const QString &name, const QVariantMap &settings);
    Q_INVOKABLE bool updateUserPreset(const QString &presetId,
                                      const QString &name,
                                      const QVariantMap &settings);
    Q_INVOKABLE bool renameUserPreset(const QString &presetId, const QString &name);
    Q_INVOKABLE bool deleteUserPreset(const QString &presetId);
    Q_INVOKABLE QVariantList exportUserPresetsSnapshot() const;
    Q_INVOKABLE bool replaceUserPresets(const QVariantList &serializedPresets);

signals:
    void presetsChanged();
    void lastErrorChanged();

private:
    static QVariantMap normalizePresetSettings(const QVariantMap &settings);
    static QVariantMap presetToVariantMap(const Preset &preset);
    static bool parseSerializedPreset(const QVariantMap &serialized,
                                      Preset *presetOut,
                                      QString *errorOut);
    int findUserPresetIndex(const QString &presetId) const;
    bool isPresetNameUsedCaseInsensitive(const QString &candidateName,
                                         const QString &ignorePresetId = QString()) const;
    QString makeNextUserPresetId();
    QString makeUniqueUserPresetName(const QString &requestedName,
                                     const QString &ignorePresetId = QString()) const;
    static QString sanitizeName(const QString &name);
    void setLastError(const QString &error);
    void bumpRevisionAndNotify();

    QList<Preset> m_userPresets;
    QString m_lastError;
    int m_revision = 0;
    int m_nextUserId = 1;
};

#endif // BATCHAUDIOCONVERTERPRESETMANAGER_H
