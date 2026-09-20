//
// Created by troplo on 09/21/26.
//

#include "BFView/Models/ArchiveTreeModel.hpp"
#include <QFileIconProvider>
#include <QIcon>
#include <format>

using namespace Nomad::Archives;

namespace BFView::Models {
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

    ArchiveTreeModel::ArchiveTreeModel(std::shared_ptr<ArchiveNode> root,
                                       QObject *parent)
        : QAbstractItemModel(parent), m_root(std::move(root)) {
        if (!m_root) {
            m_root = std::make_shared<ArchiveNode>();
            m_root->name = "/";
            m_root->fullPath = "/";
            m_root->isDirectory = true;
        }
    }

    void ArchiveTreeModel::SetRootNode(std::shared_ptr<ArchiveNode> root) {
        beginResetModel();
        m_root = std::move(root);
        if (!m_root) {
            m_root = std::make_shared<ArchiveNode>();
            m_root->name = "/";
            m_root->fullPath = "/";
            m_root->isDirectory = true;
        }
        endResetModel();
    }

    const ArchiveNode *
    ArchiveTreeModel::NodeFromIndex(const QModelIndex &index) const {
        if (!index.isValid())
            return m_root.get();
        return static_cast<const ArchiveNode *>(index.internalPointer());
    }

    QModelIndex ArchiveTreeModel::index(int row, int column,
                                        const QModelIndex &parent) const {
        if (!hasIndex(row, column, parent))
            return {};

        if (!parent.isValid()) {
            if (row == 0 && m_root) {
                return createIndex(0, column, m_root.get());
            }
            return {};
        }

        const auto *parentNode =
            static_cast<const ArchiveNode *>(parent.internalPointer());
        if (parentNode && row >= 0 &&
            row < static_cast<int>(parentNode->children.size())) {
            return createIndex(row, column, parentNode->children[row].get());
        }
        return {};
    }

    QModelIndex ArchiveTreeModel::parent(const QModelIndex &child) const {
        if (!child.isValid())
            return {};

        const auto *childNode =
            static_cast<const ArchiveNode *>(child.internalPointer());
        if (!childNode || childNode == m_root.get()) {
            return {};
        }

        const ArchiveNode *parentNode = childNode->parent;
        if (!parentNode) {
            return {};
        }

        if (parentNode == m_root.get()) {
            return createIndex(0, 0, m_root.get());
        }

        return createIndex(static_cast<int>(parentNode->RowInParent()), 0,
                           const_cast<ArchiveNode *>(parentNode));
    }

    int ArchiveTreeModel::rowCount(const QModelIndex &parent) const {
        if (parent.column() > 0)
            return 0;

        if (!parent.isValid()) {
            return m_root ? 1 : 0;
        }

        const auto *parentNode =
            static_cast<const ArchiveNode *>(parent.internalPointer());
        return (parentNode && parentNode->isDirectory)
                   ? static_cast<int>(parentNode->children.size())
                   : 0;
    }

    int ArchiveTreeModel::columnCount(const QModelIndex & /*parent*/) const {
        return ColumnCount;
    }

    QVariant ArchiveTreeModel::data(const QModelIndex &index, int role) const {
        if (!index.isValid())
            return {};

        const auto *node =
            static_cast<const ArchiveNode *>(index.internalPointer());
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

    QVariant ArchiveTreeModel::headerData(int section,
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

} // namespace BFView::Models
