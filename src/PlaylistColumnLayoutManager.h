#ifndef PLAYLISTCOLUMNLAYOUTMANAGER_H
#define PLAYLISTCOLUMNLAYOUTMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QSettings>
#include <QVector>
#include <QtGlobal>

class PlaylistColumnLayoutManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QVariantList normalColumns READ normalColumns NOTIFY normalColumnsChanged)
    Q_PROPERTY(QVariantList compactColumns READ compactColumns NOTIFY compactColumnsChanged)
    Q_PROPERTY(QString compactHeaderMode READ compactHeaderMode WRITE setCompactHeaderMode NOTIFY compactHeaderModeChanged)
    Q_PROPERTY(bool normalHasVisibleColumns READ normalHasVisibleColumns NOTIFY normalColumnsChanged)
    Q_PROPERTY(bool compactHasVisibleColumns READ compactHasVisibleColumns NOTIFY compactColumnsChanged)
    Q_PROPERTY(bool compactIsExactDefault READ compactIsExactDefault NOTIFY compactColumnsChanged)
    Q_PROPERTY(QVariantList catalog READ catalog NOTIFY catalogChanged)
    Q_PROPERTY(int layoutRevision READ layoutRevision NOTIFY layoutRevisionChanged)

public:
    enum class VisibilityMode {
        Shown,
        Automatic,
        Hidden
    };
    Q_ENUM(VisibilityMode)

    struct ColumnDescriptor {
        QString id;
        QString translationKey;
        QString valueKind; // "position", "summary", "text", "duration", "number", "year", "trackNumber", "date", "url"
        QString roleName;
        int defaultWidth = 100;
        int minimumWidth = 40;
        int maximumWidth = 0; // 0 for unlimited
        qreal stretchWeight = 0.0;
        QString alignment = QStringLiteral("left"); // "left", "right", "center"
        bool sortable = true;
        int automaticPriority = 100; // lower number = higher priority to retain in automatic mode
        int automaticMinWidth = 500;  // minimum viewport width before this automatic column is omitted
        VisibilityMode normalDefaultMode = VisibilityMode::Hidden;
        int normalDefaultOrder = 99;
        VisibilityMode compactDefaultMode = VisibilityMode::Hidden;
        int compactDefaultOrder = 99;
    };

    struct ColumnConfig {
        QString id;
        VisibilityMode visibility = VisibilityMode::Hidden;
        int width = 0;

        bool operator==(const ColumnConfig &other) const {
            return id == other.id && visibility == other.visibility && width == other.width;
        }
        bool operator!=(const ColumnConfig &other) const {
            return !(*this == other);
        }
    };

    explicit PlaylistColumnLayoutManager(QObject *parent = nullptr);
    ~PlaylistColumnLayoutManager() override;

    static const QVector<ColumnDescriptor> &catalogDescriptors();
    static const ColumnDescriptor *findDescriptor(const QString &id);

    QVariantList catalog() const;
    QVariantList normalColumns() const;
    QVariantList compactColumns() const;
    QString compactHeaderMode() const { return m_compactHeaderMode; }
    void setCompactHeaderMode(const QString &mode);

    bool normalHasVisibleColumns() const;
    bool compactHasVisibleColumns() const;
    bool compactIsExactDefault() const;

    Q_INVOKABLE QVariantList columnsForSkin(const QString &skin) const;
    Q_INVOKABLE void setColumnsForSkin(const QString &skin, const QVariantList &columns);
    Q_INVOKABLE void setColumnVisibility(const QString &skin, const QString &columnId, const QString &visibility);
    Q_INVOKABLE void moveColumn(const QString &skin, int fromIndex, int toIndex);
    Q_INVOKABLE void copySkinLayout(const QString &sourceSkin, const QString &targetSkin);
    Q_INVOKABLE void resetSkin(const QString &skin);
    Q_INVOKABLE void resetAllSkins();
    Q_INVOKABLE bool hasVisibleColumns(const QString &skin) const;
    Q_INVOKABLE bool isExactDefaultLayout(const QString &skin) const;
    Q_INVOKABLE bool isColumnVisible(const QString &skin, const QString &columnId) const;
    Q_INVOKABLE void toggleColumnVisibility(const QString &skin, const QString &columnId);

    Q_INVOKABLE int widthBucket(const QString &skin, qreal availableWidth) const;
    Q_INVOKABLE QVariantList effectiveVisibleColumns(const QString &skin, qreal availableWidth) const;
    Q_INVOKABLE QVariantMap columnDescriptor(const QString &columnId) const;
    Q_INVOKABLE QString formatValue(const QString &columnId, const QVariant &value, const QVariantMap &extra = {}) const;
    Q_INVOKABLE QString alignString(const QString &columnId) const;
    Q_INVOKABLE bool isUrlSchemeAllowed(const QString &url) const;
    Q_INVOKABLE int layoutRevision() const { return m_layoutRevision; }

    // Load & Save
    void loadSettings();
    void saveSettings();

    static QString visibilityModeToString(VisibilityMode mode);
    static VisibilityMode visibilityModeFromString(const QString &str, VisibilityMode fallback = VisibilityMode::Hidden);

signals:
    void normalColumnsChanged();
    void compactColumnsChanged();
    void compactHeaderModeChanged();
    void catalogChanged();
    void layoutChanged(const QString &skin);
    void layoutRevisionChanged();

private:
    static QVector<ColumnConfig> defaultNormalConfig();
    static QVector<ColumnConfig> defaultCompactConfig();
    static QVector<ColumnConfig> sanitizeConfig(const QVector<ColumnConfig> &rawConfig, const QVector<ColumnConfig> &defaultConfig);
    static QVariantList configToVariantList(const QVector<ColumnConfig> &config);
    static QVector<ColumnConfig> variantListToConfig(const QVariantList &list, const QVector<ColumnConfig> &fallback);

    QVector<ColumnConfig> m_normalColumns;
    QVector<ColumnConfig> m_compactColumns;
    QString m_compactHeaderMode = QStringLiteral("automatic"); // "automatic", "alwaysShown", "alwaysHidden"
    int m_layoutRevision = 0;

    mutable QHash<QString, QVariantList> m_effectiveColumnsCache;
    mutable QHash<QString, QVariantMap> m_columnDescriptorCache;
};

#endif // PLAYLISTCOLUMNLAYOUTMANAGER_H
