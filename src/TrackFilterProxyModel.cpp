#include "TrackFilterProxyModel.h"

#include "TrackModel.h"

TrackFilterProxyModel::TrackFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);

    connect(this, &QAbstractItemModel::rowsInserted, this, &TrackFilterProxyModel::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &TrackFilterProxyModel::countChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &TrackFilterProxyModel::countChanged);
}

void TrackFilterProxyModel::setNormalizedQuery(const QString &query)
{
    const QString normalized = query.trimmed().toLower();
    if (m_normalizedQuery == normalized) {
        return;
    }

    m_normalizedQuery = normalized;
    emit normalizedQueryChanged();
    refreshFilter();
}

void TrackFilterProxyModel::setFieldMask(int fieldMask)
{
    if (m_fieldMask == fieldMask) {
        return;
    }

    m_fieldMask = fieldMask;
    emit fieldMaskChanged();
    refreshFilter();
}

void TrackFilterProxyModel::setQuickFilterMask(int quickFilterMask)
{
    if (m_quickFilterMask == quickFilterMask) {
        return;
    }

    m_quickFilterMask = quickFilterMask;
    emit quickFilterMaskChanged();
    refreshFilter();
}

void TrackFilterProxyModel::setSourceModel(QAbstractItemModel *sourceModel)
{
    if (this->sourceModel() == sourceModel) {
        return;
    }

    disconnect(m_searchRevisionConnection);
    QSortFilterProxyModel::setSourceModel(sourceModel);

    if (TrackModel *model = trackModel()) {
        m_searchRevisionConnection = connect(model,
                                             &TrackModel::searchRevisionChanged,
                                             this,
                                             &TrackFilterProxyModel::refreshFilter);
    }

    refreshFilter();
}

QVariant TrackFilterProxyModel::data(const QModelIndex &index, int role) const
{
    if (role == SourceIndexRole) {
        return sourceIndexAt(index.row());
    }
    return QSortFilterProxyModel::data(index, role);
}

QHash<int, QByteArray> TrackFilterProxyModel::roleNames() const
{
    QHash<int, QByteArray> roles = QSortFilterProxyModel::roleNames();
    roles.insert(SourceIndexRole, QByteArrayLiteral("sourceIndex"));
    return roles;
}

int TrackFilterProxyModel::sourceIndexAt(int proxyRow) const
{
    if (proxyRow < 0 || proxyRow >= rowCount()) {
        return -1;
    }
    return mapToSource(index(proxyRow, 0)).row();
}

int TrackFilterProxyModel::proxyIndexForSource(int sourceRow) const
{
    if (!sourceModel() || sourceRow < 0 || sourceRow >= sourceModel()->rowCount()) {
        return -1;
    }
    return mapFromSource(sourceModel()->index(sourceRow, 0)).row();
}

bool TrackFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    Q_UNUSED(sourceParent)
    const TrackModel *model = trackModel();
    if (!model) {
        return true;
    }

    return model->matchesSearchAdvancedNormalized(sourceRow,
                                                   m_normalizedQuery,
                                                   m_fieldMask,
                                                   m_quickFilterMask);
}

void TrackFilterProxyModel::refreshFilter()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    beginFilterChange();
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
#else
    invalidateFilter();
#endif
}

TrackModel *TrackFilterProxyModel::trackModel() const
{
    return qobject_cast<TrackModel *>(sourceModel());
}
