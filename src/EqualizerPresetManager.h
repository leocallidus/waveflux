#ifndef EQUALIZERPRESETMANAGER_H
#define EQUALIZERPRESETMANAGER_H

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QList>
#include <QString>
#include <QStringList>
#include <QtGlobal>

class QJsonObject;

class EqualizerPresetManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QVariantList presets READ presets NOTIFY presetsChanged)
    Q_PROPERTY(QVariantList builtInPresets READ builtInPresets NOTIFY presetsChanged)
    Q_PROPERTY(QVariantList userPresets READ userPresets NOTIFY presetsChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(int revision READ revision NOTIFY presetsChanged)
    Q_PROPERTY(int bandCount READ bandCount CONSTANT)
    Q_PROPERTY(double minGainDb READ minGainDb CONSTANT)
    Q_PROPERTY(double maxGainDb READ maxGainDb CONSTANT)

public:
    struct Preset {
        QString id;
        QString name;
        QVariantList gainsDb;
        bool builtIn = false;
        qint64 updatedAtMs = 0;

        bool operator==(const Preset &other) const = default;
    };

    struct ImportResult {
        bool success = false;
        int importedCount = 0;
        int replacedCount = 0;
        int skippedCount = 0;
        QStringList errors;
        QString mergePolicy;

        QVariantMap toVariantMap() const;
    };

    struct ExportResult {
        bool success = false;
        int exportedCount = 0;
        QString error;
        QString jsonPayload;
        QString schema;

        QVariantMap toVariantMap() const;
    };

    explicit EqualizerPresetManager(QObject *parent = nullptr);

    QVariantList presets() const;
    QVariantList builtInPresets() const;
    QVariantList userPresets() const;
    QString lastError() const { return m_lastError; }
    int revision() const { return m_revision; }
    int bandCount() const { return kBandCount; }
    double minGainDb() const { return kMinGainDb; }
    double maxGainDb() const { return kMaxGainDb; }

    Q_INVOKABLE QVariantList listPresets() const;
    Q_INVOKABLE QVariantList listBuiltInPresets() const;
    Q_INVOKABLE QVariantList listUserPresets() const;
    Q_INVOKABLE QVariantMap getPreset(const QString &presetId) const;
    Q_INVOKABLE bool hasPreset(const QString &presetId) const;

    Q_INVOKABLE QString createUserPreset(const QString &name, const QVariantList &gainsDb);
    Q_INVOKABLE bool updateUserPreset(const QString &presetId,
                                      const QString &name,
                                      const QVariantList &gainsDb);
    Q_INVOKABLE bool renameUserPreset(const QString &presetId, const QString &name);
    Q_INVOKABLE bool deleteUserPreset(const QString &presetId);
    Q_INVOKABLE void clearUserPresets();

    Q_INVOKABLE QVariantList exportUserPresetsSnapshot() const;
    Q_INVOKABLE bool replaceUserPresets(const QVariantList &serializedPresets);
    Q_INVOKABLE QVariantMap exportPresetToJson(const QString &presetId) const;
    Q_INVOKABLE QVariantMap exportUserPresetsToJson() const;
    Q_INVOKABLE QVariantMap exportBundleV1ToJson() const;
    Q_INVOKABLE QVariantMap exportPresetToJsonFile(const QString &presetId, const QString &filePath) const;
    Q_INVOKABLE QVariantMap exportUserPresetsToJsonFile(const QString &filePath) const;
    Q_INVOKABLE QVariantMap exportBundleV1ToJsonFile(const QString &filePath) const;
    Q_INVOKABLE QVariantMap importPresetsFromJson(const QString &jsonPayload,
                                                  const QString &mergePolicy = QStringLiteral("keep_both"));
    Q_INVOKABLE QVariantMap importPresetsFromJsonFile(const QString &filePath,
                                                      const QString &mergePolicy = QStringLiteral("keep_both"));

signals:
    void presetsChanged();
    void lastErrorChanged();

private:
    static constexpr int kBandCount = 10;
    static constexpr double kMinGainDb = -24.0;
    static constexpr double kMaxGainDb = 12.0;

    static Preset makeBuiltInPreset(const QString &id,
                                    const QString &name,
                                    const QVariantList &gainsDb);
    static QVariantList normalizeGains(const QVariantList &gainsDb);
    static QVariantMap presetToVariantMap(const Preset &preset);
    static bool parseSerializedPreset(const QVariantMap &serialized,
                                      Preset *presetOut,
                                      QString *errorOut);
    int findBuiltInPresetIndex(const QString &presetId) const;
    int findUserPresetIndex(const QString &presetId) const;
    int findUserPresetIndexByNameCaseInsensitive(const QList<Preset> &presets, const QString &name) const;
    bool isPresetNameUsedCaseInsensitive(const QList<Preset> &presets,
                                         const QString &candidateName,
                                         const QString &ignorePresetId = QString()) const;
    QString makeNextUserPresetId();
    QString makeNextUserPresetIdInList(const QList<Preset> &presets, int *nextId) const;
    QString makeUniqueUserPresetName(const QString &requestedName,
                                     const QString &ignorePresetId = QString()) const;
    QString makeUniqueUserPresetNameInList(const QList<Preset> &presets,
                                           const QString &requestedName,
                                           const QString &ignorePresetId = QString()) const;
    static QJsonObject presetToJsonObject(const Preset &preset);
    static bool parsePresetFromJsonObject(const QJsonObject &object,
                                          Preset *presetOut,
                                          QString *errorOut);
    static bool writeTextFileAtomically(const QString &filePath,
                                        const QString &utf8Text,
                                        QString *errorOut);
    static bool readTextFile(const QString &filePath,
                             QString *utf8TextOut,
                             QString *errorOut);
    ExportResult exportPresetsAsBundleV1(const QList<Preset> &presets) const;
    static bool isValidMergePolicy(const QString &mergePolicy);
    static QString sanitizeName(const QString &name);
    void setLastError(const QString &error);
    void bumpRevisionAndNotify();

    QList<Preset> m_builtInPresets;
    QList<Preset> m_userPresets;
    QString m_lastError;
    int m_revision = 0;
    int m_nextUserId = 1;
};

#endif // EQUALIZERPRESETMANAGER_H
