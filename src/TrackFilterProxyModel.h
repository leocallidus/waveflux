#ifndef TRACKFILTERPROXYMODEL_H
#define TRACKFILTERPROXYMODEL_H

#include <QSortFilterProxyModel>

class TrackModel;

/**
 * @brief Lightweight filtered view over TrackModel for playlist search.
 *
 * Keeping rejected rows out of the QML ListView avoids creating thousands of
 * zero-height delegates when a large playlist has only a few search matches.
 */
class TrackFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

    Q_PROPERTY(QString normalizedQuery READ normalizedQuery WRITE setNormalizedQuery NOTIFY normalizedQueryChanged)
    Q_PROPERTY(int fieldMask READ fieldMask WRITE setFieldMask NOTIFY fieldMaskChanged)
    Q_PROPERTY(int quickFilterMask READ quickFilterMask WRITE setQuickFilterMask NOTIFY quickFilterMaskChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum ExtraRole {
        SourceIndexRole = Qt::UserRole + 100
    };
    Q_ENUM(ExtraRole)

    explicit TrackFilterProxyModel(QObject *parent = nullptr);

    QString normalizedQuery() const { return m_normalizedQuery; }
    void setNormalizedQuery(const QString &query);

    int fieldMask() const { return m_fieldMask; }
    void setFieldMask(int fieldMask);

    int quickFilterMask() const { return m_quickFilterMask; }
    void setQuickFilterMask(int quickFilterMask);

    void setSourceModel(QAbstractItemModel *sourceModel) override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE int sourceIndexAt(int proxyRow) const;
    Q_INVOKABLE int proxyIndexForSource(int sourceRow) const;

signals:
    void normalizedQueryChanged();
    void fieldMaskChanged();
    void quickFilterMaskChanged();
    void countChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    void refreshFilter();
    TrackModel *trackModel() const;

    QString m_normalizedQuery;
    int m_fieldMask = 0;
    int m_quickFilterMask = 0;
    QMetaObject::Connection m_searchRevisionConnection;
};

#endif // TRACKFILTERPROXYMODEL_H
