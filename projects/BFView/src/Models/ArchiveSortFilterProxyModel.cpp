//
// Created by troplo on 09/21/26.
//

#include "BFView/Models/ArchiveSortFilterProxyModel.hpp"

namespace BFView::Models {
    ArchiveSortFilterProxyModel::ArchiveSortFilterProxyModel(QObject* parent)
        : QSortFilterProxyModel(parent) {}

    bool ArchiveSortFilterProxyModel::lessThan(const QModelIndex& left,
                                               const QModelIndex& right) const {
        QModelIndex leftTypeIdx =
            sourceModel()->index(left.row(), 1, left.parent());
        QModelIndex rightTypeIdx =
            sourceModel()->index(right.row(), 1, right.parent());

        bool leftIsDir =
            sourceModel()->data(leftTypeIdx, Qt::UserRole).toBool();
        bool rightIsDir =
            sourceModel()->data(rightTypeIdx, Qt::UserRole).toBool();

        if (leftIsDir != rightIsDir) {
            return sortOrder() == Qt::AscendingOrder ? leftIsDir : !leftIsDir;
        }

        int col = left.column();
        if (col == 2 || col == 3 ||
            col == 4) { // CompressedSize, UncompressedSize, Offset
            qulonglong leftVal =
                sourceModel()->data(left, Qt::UserRole).toULongLong();
            qulonglong rightVal =
                sourceModel()->data(right, Qt::UserRole).toULongLong();
            return leftVal < rightVal;
        }

        return QSortFilterProxyModel::lessThan(left, right);
    }

} // namespace BFView::Models
