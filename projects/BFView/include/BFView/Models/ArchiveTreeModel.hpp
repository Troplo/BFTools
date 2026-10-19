//
// Created by troplo on 09/21/26.
//

#ifndef BFVIEW_ARCHIVETREEMODEL_H
#define BFVIEW_ARCHIVETREEMODEL_H

#include "Nomad/Archives/DirectoryTree.hpp"
#include <QAbstractItemModel>
#include <memory>

namespace BFView::Models {
    class ArchiveTreeModel : public QAbstractItemModel {
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

        explicit ArchiveTreeModel(
            std::shared_ptr<Nomad::Archives::ArchiveNode> root,
            QObject* parent = nullptr);
        ~ArchiveTreeModel() override = default;

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

        void SetRootNode(std::shared_ptr<Nomad::Archives::ArchiveNode> root);
        [[nodiscard]] std::shared_ptr<Nomad::Archives::ArchiveNode>
        GetRootNode() const {
            return m_root;
        }
        [[nodiscard]] const Nomad::Archives::ArchiveNode*
        NodeFromIndex(const QModelIndex& index) const;

      private:
        std::shared_ptr<Nomad::Archives::ArchiveNode> m_root;
    };

} // namespace BFView::Models

#endif // BFVIEW_ARCHIVETREEMODEL_H
