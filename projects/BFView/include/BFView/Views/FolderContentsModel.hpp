//
// Created by troplo on 09/21/26.
//

#ifndef BFVIEW_FOLDERCONTENTSMODEL_H
#define BFVIEW_FOLDERCONTENTSMODEL_H

#include "Nomad/Archives/DirectoryTree.hpp"
#include <QAbstractItemModel>
#include <memory>
#include <vector>

namespace BFView::Views {
    class FolderContentsModel : public QAbstractItemModel {
        Q_OBJECT

      public:
        enum Column {
            Name = 0,
            Type,
            CompressedSize,
            UncompressedSize,
            Offset,
            Compression,
            Hash,
            ColumnCount
        };

        explicit FolderContentsModel(QObject* parent = nullptr);
        ~FolderContentsModel() override = default;

        [[nodiscard]] QModelIndex
        index(int row, int column,
              const QModelIndex& parent = QModelIndex()) const override;
        [[nodiscard]] QModelIndex
        parent(const QModelIndex& child) const override;
        [[nodiscard]] int
        rowCount(const QModelIndex& parent = QModelIndex()) const override;
        [[nodiscard]] int
        columnCount(const QModelIndex& parent = QModelIndex()) const override;
        [[nodiscard]] QVariant data(const QModelIndex& index,
                                    int role = Qt::DisplayRole) const override;
        [[nodiscard]] QVariant
        headerData(int section, Qt::Orientation orientation,
                   int role = Qt::DisplayRole) const override;

        void SetFolderNode(const Nomad::Archives::ArchiveNode* folderNode);
        [[nodiscard]] const Nomad::Archives::ArchiveNode*
        NodeFromIndex(const QModelIndex& index) const;

      private:
        const Nomad::Archives::ArchiveNode* m_folderNode{nullptr};
    };

} // namespace BFView::Views

#endif // BFVIEW_FOLDERCONTENTSMODEL_H
