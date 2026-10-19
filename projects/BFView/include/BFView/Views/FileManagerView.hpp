//
// Created by troplo on 09/21/26.
//

#ifndef BFVIEW_FILEMANAGERVIEW_H
#define BFVIEW_FILEMANAGERVIEW_H

#include "BFView/Models/ArchiveSortFilterProxyModel.hpp"
#include "BFView/Models/ArchiveTreeModel.hpp"
#include "BFView/Views/FolderContentsModel.hpp"
#include "BFView/Views/LocationBreadcrumb.hpp"
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFocusEvent>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPoint>
#include <QSplitter>
#include <QTreeView>
#include <QWidget>
#include <vector>

namespace BFView::Views {
    class ContentsTreeView : public QTreeView {
        Q_OBJECT

      public:
        explicit ContentsTreeView(QWidget* parent = nullptr);
        ~ContentsTreeView() override = default;

      signals:
        void RequestDragStart(const QModelIndex& index);
        void RequestDragIndexes(const QModelIndexList& indexes);
        void RequestNavigateSelected();
        void RequestDeleteSelected();

      protected:
        void mousePressEvent(QMouseEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;
        void focusOutEvent(QFocusEvent* event) override;
        void keyPressEvent(QKeyEvent* event) override;
        QItemSelectionModel::SelectionFlags
        selectionCommand(const QModelIndex& index,
                         const QEvent* event = nullptr) const override;

      private:
        QPoint m_dragStartPos{-1, -1};
    };

    class FileManagerView : public QWidget {
        Q_OBJECT

      public:
        explicit FileManagerView(QWidget* parent = nullptr);
        ~FileManagerView() override = default;

        void SetArchiveRoot(std::shared_ptr<Nomad::Archives::ArchiveNode> root);
        void SetSearchFilter(const QString& filterText);
        void FocusLocationInput();

        [[nodiscard]] const Nomad::Archives::ArchiveNode*
        GetSelectedNode() const;
        [[nodiscard]] std::vector<const Nomad::Archives::ArchiveNode*>
        GetSelectedNodes() const;
        [[nodiscard]] const Nomad::Archives::ArchiveNode*
        GetCurrentFolderNode() const {
            return m_currentFolderNode;
        }
        void SetCurrentFolder(const Nomad::Archives::ArchiveNode* folderNode);
        void NavigateToPath(const QString& path);

      signals:
        void FolderChanged(const QString& fullPath);
        void RequestExtractNode(const Nomad::Archives::ArchiveNode* node);
        void RequestExtractNodes(
            const std::vector<const Nomad::Archives::ArchiveNode*>& nodes);
        void RequestOpenFile(const Nomad::Archives::ArchiveNode* node,
                             bool bypassCustomHandler, bool openWithChooser);
        void RequestExtractAll();
        void RequestAddFile();
        void RequestAddFolder();
        void RequestNewFolder();
        void RequestDeleteNode(const Nomad::Archives::ArchiveNode* node);
        void RequestDeleteNodes(
            const std::vector<const Nomad::Archives::ArchiveNode*>& nodes);
        void FilesDropped(const QStringList& filePaths,
                          const QString& targetFolderRelPath);
        void ArchiveFileDropped(const QString& fatPath);
        void RequestDragOut(const Nomad::Archives::ArchiveNode* node);
        void RequestDragOutNodes(
            const std::vector<const Nomad::Archives::ArchiveNode*>& nodes);

      protected:
        void dragEnterEvent(QDragEnterEvent* event) override;
        void dragMoveEvent(QDragMoveEvent* event) override;
        void dragLeaveEvent(QDragLeaveEvent* event) override;
        void dropEvent(QDropEvent* event) override;

      private slots:
        void OnTreeSelectionChanged(const QModelIndex& current,
                                    const QModelIndex& previous);
        void OnContentsDoubleClicked(const QModelIndex& index);
        void OnContextMenuRequested(const QPoint& pos);
        void OnTreeContextMenuRequested(const QPoint& pos);
        void OnContentsDragStart(const QModelIndex& index);
        void OnContentsDragIndexes(const QModelIndexList& indexes);

      private:
        void SetupUi();

        Models::ArchiveTreeModel* m_treeModel{nullptr};
        FolderContentsModel* m_contentsModel{nullptr};
        Models::ArchiveSortFilterProxyModel* m_treeProxyModel{nullptr};
        Models::ArchiveSortFilterProxyModel* m_contentsProxyModel{nullptr};

        LocationBreadcrumb* m_breadcrumb{nullptr};
        QTreeView* m_treeView{nullptr};
        ContentsTreeView* m_contentsView{nullptr};
        QLabel* m_dropOverlayLabel{nullptr};

        const Nomad::Archives::ArchiveNode* m_currentFolderNode{nullptr};
        bool m_isSyncingTreeSelection{false};
    };

} // namespace BFView::Views

#endif // BFVIEW_FILEMANAGERVIEW_H
