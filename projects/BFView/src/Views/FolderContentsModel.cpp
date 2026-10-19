//
// Created by troplo on 09/21/26.
//

#include "BFView/Views/FolderContentsModel.hpp"
#include <QFileIconProvider>
#include <QIcon>
#include <format>

using namespace Nomad::Archives;

namespace BFView::Views {
    static QString FormatFileSize(uint64_t bytes) {
        if (bytes < 1024) {
            return QString("%1 B").arg(bytes);
        }
        double kb = bytes / 1024.0;
        if (kb < 1024.0) {
            return QString("%1 KB").arg(kb, 0, 'f', (kb < 10.0) ? 2 : 1);
        }
        double mb = kb / 1024.0;
        if (mb < 1024.0) {
            return QString("%1 MB").arg(mb, 0, 'f', 2);
        }
        double gb = mb / 1024.0;
        return QString("%1 GB").arg(gb, 0, 'f', 2);
    }

    FolderContentsModel::FolderContentsModel(QObject* parent)
        : QAbstractItemModel(parent) {}

    void FolderContentsModel::SetFolderNode(const ArchiveNode* folderNode) {
        beginResetModel();
        m_folderNode = folderNode;
        endResetModel();
    }

    const ArchiveNode*
    FolderContentsModel::NodeFromIndex(const QModelIndex& index) const {
        if (!index.isValid())
            return m_folderNode;
        return static_cast<const ArchiveNode*>(index.internalPointer());
    }

    QModelIndex FolderContentsModel::index(int row, int column,
                                           const QModelIndex& parent) const {
        if (!hasIndex(row, column, parent))
            return {};

        const ArchiveNode* parentNode = nullptr;
        if (!parent.isValid()) {
            parentNode = m_folderNode;
        } else {
            parentNode =
                static_cast<const ArchiveNode*>(parent.internalPointer());
        }

        if (parentNode && row >= 0 &&
            row < static_cast<int>(parentNode->children.size())) {
            return createIndex(row, column, parentNode->children[row].get());
        }
        return {};
    }

    QModelIndex FolderContentsModel::parent(const QModelIndex& child) const {
        if (!child.isValid())
            return {};

        const auto* childNode =
            static_cast<const ArchiveNode*>(child.internalPointer());
        if (!childNode)
            return {};

        const ArchiveNode* parentNode = childNode->parent;
        if (!parentNode || parentNode == m_folderNode) {
            return {};
        }

        const ArchiveNode* grandParentNode = parentNode->parent;
        if (!grandParentNode) {
            return {};
        }

        return createIndex(static_cast<int>(parentNode->RowInParent()), 0,
                           const_cast<ArchiveNode*>(parentNode));
    }

    int FolderContentsModel::rowCount(const QModelIndex& parent) const {
        if (parent.column() > 0)
            return 0;

        const ArchiveNode* parentNode = nullptr;
        if (!parent.isValid()) {
            parentNode = m_folderNode;
        } else {
            parentNode =
                static_cast<const ArchiveNode*>(parent.internalPointer());
        }

        return (parentNode && parentNode->isDirectory)
                   ? static_cast<int>(parentNode->children.size())
                   : 0;
    }

    int FolderContentsModel::columnCount(const QModelIndex& /*parent*/) const {
        return ColumnCount;
    }

    QVariant FolderContentsModel::data(const QModelIndex& index,
                                       int role) const {
        if (!index.isValid())
            return {};

        const auto* node =
            static_cast<const ArchiveNode*>(index.internalPointer());
        if (!node)
            return {};

        if (role == Qt::DisplayRole) {
            switch (index.column()) {
            case Name:
                return QString::fromStdString(node->name.empty() ? "/"
                                                                 : node->name);
            case Type:
                return node->isDirectory ? "Folder" : "File";
            case CompressedSize:
                if (node->isDirectory || !node->entryRef)
                    return "-";
                return FormatFileSize(node->entryRef->compressed_size);
            case UncompressedSize:
                if (node->isDirectory || !node->entryRef)
                    return "-";
                return FormatFileSize(node->entryRef->uncompressed_size);
            case Offset:
                if (node->isDirectory || !node->entryRef)
                    return "-";
                return QString::fromStdString(
                    std::format("0x{:X}", node->entryRef->offset));
            case Compression:
                if (node->isDirectory || !node->entryRef)
                    return "-";
                return QString::fromStdString(std::string(
                    CompressionToString(node->entryRef->compression)));
            case Hash:
                if (node->isDirectory || !node->entryRef)
                    return "-";
                return QString::fromStdString(
                    std::format("0x{:X}", node->entryRef->name_hash));
            default:
                return {};
            }
        } else if (role == Qt::ToolTipRole) {
            if (!node->isDirectory && node->entryRef) {
                switch (index.column()) {
                case CompressedSize:
                    return QString("%1 bytes")
                        .arg(node->entryRef->compressed_size);
                case UncompressedSize:
                    return QString("%1 bytes")
                        .arg(node->entryRef->uncompressed_size);
                default:
                    break;
                }
            }
        } else if (role == Qt::UserRole) {
            if (index.column() == Type) {
                return node->isDirectory;
            }
            if (!node->isDirectory && node->entryRef) {
                switch (index.column()) {
                case CompressedSize:
                    return static_cast<qulonglong>(
                        node->entryRef->compressed_size);
                case UncompressedSize:
                    return static_cast<qulonglong>(
                        node->entryRef->uncompressed_size);
                case Offset:
                    return static_cast<qulonglong>(node->entryRef->offset);
                default:
                    break;
                }
            }
            return 0ULL;
        } else if (role == Qt::TextAlignmentRole) {
            switch (index.column()) {
            case CompressedSize:
            case UncompressedSize:
            case Offset:
                return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
            default:
                return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
            }
        } else if (role == Qt::DecorationRole && index.column() == Name) {
            static QFileIconProvider iconProvider;
            if (node->isDirectory) {
                return iconProvider.icon(QFileIconProvider::Folder);
            } else {
                return iconProvider.icon(QFileIconProvider::File);
            }
        }

        return {};
    }

    QVariant FolderContentsModel::headerData(int section,
                                             Qt::Orientation orientation,
                                             int role) const {
        if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
            switch (section) {
            case Name:
                return "Name";
            case Type:
                return "Type";
            case CompressedSize:
                return "CS";
            case UncompressedSize:
                return "US";
            case Offset:
                return "Offset";
            case Compression:
                return "Comp";
            case Hash:
                return "Hash";
            default:
                return {};
            }
        }
        return {};
    }

} // namespace BFView::Views
