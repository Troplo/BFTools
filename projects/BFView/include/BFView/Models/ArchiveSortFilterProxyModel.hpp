//
// Created by troplo on 09/21/26.
//

#ifndef BFVIEW_ARCHIVESORTFILTERPROXYMODEL_H
#define BFVIEW_ARCHIVESORTFILTERPROXYMODEL_H

#include <QSortFilterProxyModel>

namespace BFView::Models {
    class ArchiveSortFilterProxyModel : public QSortFilterProxyModel {
        Q_OBJECT

      public:
        explicit ArchiveSortFilterProxyModel(QObject *parent = nullptr);
        ~ArchiveSortFilterProxyModel() override = default;

      protected:
        bool lessThan(const QModelIndex &left,
                      const QModelIndex &right) const override;
    };

} // namespace BFView::Models

#endif // BFVIEW_ARCHIVESORTFILTERPROXYMODEL_H
