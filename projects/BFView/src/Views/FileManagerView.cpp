//
// Created by troplo on 09/21/26.
//

#include "BFView/Views/FileManagerView.hpp"
#include <QApplication>
#include <QClipboard>
#include <QDrag>
#include <QFileInfo>
#include <QIcon>
#include <QMimeData>
#include <QShortcut>
#include <QUrl>
#include <QVBoxLayout>
#include <format>

using namespace Nomad::Archives;

namespace BFView::Views {
    ContentsTreeView::ContentsTreeView(QWidget* parent) : QTreeView(parent) {}

    void ContentsTreeView::mousePressEvent(QMouseEvent* event) {
        if (event->button() == Qt::LeftButton) {
            m_dragStartPos = event->pos();
        }
        QTreeView::mousePressEvent(event);
    }

    void ContentsTreeView::mouseMoveEvent(QMouseEvent* event) {
        if ((event->buttons() & Qt::LeftButton) &&
            m_dragStartPos != QPoint(-1, -1)) {
            int distance = (event->pos() - m_dragStartPos).manhattanLength();
            if (distance >= QApplication::startDragDistance()) {
                QModelIndex index = indexAt(m_dragStartPos);
                m_dragStartPos = QPoint(-1, -1);
                setState(NoState);
                if (index.isValid()) {
                    QModelIndexList selected = selectionModel()->selectedRows();
                    bool containsIndex = false;
                    for (const auto& idx : selected) {
                        if (idx.row() == index.row() &&
                            idx.parent() == index.parent()) {
                            containsIndex = true;
                            break;
                        }
                    }
                    if (!containsIndex) {
                        selected.clear();
                        selected.append(index);
                    }
                    emit RequestDragIndexes(selected);
                    return;
                }
            }
        }
        QTreeView::mouseMoveEvent(event);
    }

    void ContentsTreeView::mouseReleaseEvent(QMouseEvent* event) {
        m_dragStartPos = QPoint(-1, -1);
        QTreeView::mouseReleaseEvent(event);
        if (state() == DragSelectingState) {
            setState(NoState);
        }
    }

    void ContentsTreeView::focusOutEvent(QFocusEvent* event) {
        if (state() == DragSelectingState) {
            setState(NoState);
        }
        QTreeView::focusOutEvent(event);
    }

    QItemSelectionModel::SelectionFlags
    ContentsTreeView::selectionCommand(const QModelIndex& index,
                                       const QEvent* event) const {
        if (event && (event->type() == QEvent::KeyPress ||
                      event->type() == QEvent::KeyRelease)) {
            const auto* ke = static_cast<const QKeyEvent*>(event);
            if (!(ke->modifiers() &
                  (Qt::ShiftModifier | Qt::ControlModifier))) {
                return QItemSelectionModel::ClearAndSelect |
                       QItemSelectionModel::Rows;
            }
        }
        return QTreeView::selectionCommand(index, event);
    }

    void ContentsTreeView::keyPressEvent(QKeyEvent* event) {
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            emit RequestNavigateSelected();
            return;
        } else if (event->key() == Qt::Key_Delete) {
            emit RequestDeleteSelected();
            return;
        }

        const bool hasShift = (event->modifiers() & Qt::ShiftModifier);
        const bool hasCtrl = (event->modifiers() & Qt::ControlModifier);
        const bool hasAlt = (event->modifiers() & Qt::AltModifier);

        if (!hasShift && !hasCtrl && !hasAlt) {
            if (state() == DragSelectingState) {
                setState(NoState);
            }

            if (event->key() == Qt::Key_Right) {
                QModelIndex curr = currentIndex();
                if (!curr.isValid()) {
                    curr = model() ? model()->index(0, 0) : QModelIndex();
                } else if (curr.column() != 0) {
                    curr = curr.siblingAtColumn(0);
                }

                if (curr.isValid()) {
                    bool isDirWithChildren = model()->hasChildren(curr) &&
                                             (model()->rowCount(curr) > 0);
                    if (isDirWithChildren && !isExpanded(curr)) {
                        expand(curr);
                        return;
                    }

                    // If already expanded and has children, descend to first
                    // child
                    QModelIndex targetIdx;
                    if (isDirWithChildren && isExpanded(curr)) {
                        targetIdx = model()->index(0, 0, curr);
                    }
                    // Otherwise (leaf/file/empty dir/already expanded leaf),
                    // move down to the next visible row
                    if (!targetIdx.isValid()) {
                        targetIdx = indexBelow(curr);
                    }

                    if (targetIdx.isValid()) {
                        if (targetIdx.column() != 0) {
                            targetIdx = targetIdx.siblingAtColumn(0);
                        }
                        selectionModel()->setCurrentIndex(
                            targetIdx, QItemSelectionModel::ClearAndSelect |
                                           QItemSelectionModel::Rows);
                        scrollTo(targetIdx);
                        return;
                    }
                }
                return;
            } else if (event->key() == Qt::Key_Left) {
                QModelIndex curr = currentIndex();
                if (curr.isValid()) {
                    if (curr.column() != 0) {
                        curr = curr.siblingAtColumn(0);
                    }
                    if (isExpanded(curr)) {
                        collapse(curr);
                        return;
                    }
                    QModelIndex parentIdx = curr.parent();
                    if (parentIdx.isValid()) {
                        if (parentIdx.column() != 0) {
                            parentIdx = parentIdx.siblingAtColumn(0);
                        }
                        selectionModel()->setCurrentIndex(
                            parentIdx, QItemSelectionModel::ClearAndSelect |
                                           QItemSelectionModel::Rows);
                        scrollTo(parentIdx);
                        return;
                    }
                }
                return;
            } else if (event->key() == Qt::Key_Down ||
                       event->key() == Qt::Key_Up) {
                if (selectionModel() &&
                    selectionModel()->selectedRows().size() > 1) {
                    QModelIndex curr = currentIndex();
                    if (curr.isValid()) {
                        QModelIndex target = (event->key() == Qt::Key_Down)
                                                 ? indexBelow(curr)
                                                 : indexAbove(curr);
                        if (!target.isValid()) {
                            target = curr;
                        }
                        if (target.column() != 0) {
                            target = target.siblingAtColumn(0);
                        }
                        selectionModel()->setCurrentIndex(
                            target, QItemSelectionModel::ClearAndSelect |
                                        QItemSelectionModel::Rows);
                        scrollTo(target);
                        return;
                    }
                }
            }
        }

        QTreeView::keyPressEvent(event);
    }

    FileManagerView::FileManagerView(QWidget* parent) : QWidget(parent) {
        setAcceptDrops(true);
        SetupUi();
    }

    void FileManagerView::SetupUi() {
        auto* mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(6);

        m_breadcrumb = new LocationBreadcrumb(this);
        connect(m_breadcrumb, &LocationBreadcrumb::PathClicked, this,
                &FileManagerView::NavigateToPath);
        mainLayout->addWidget(m_breadcrumb);

        auto rootNode = std::make_shared<ArchiveNode>();
        rootNode->name = "/";
        rootNode->fullPath = "/";
        rootNode->isDirectory = true;

        // Models with Folders-First Sorting
        m_treeModel = new Models::ArchiveTreeModel(rootNode, this);
        m_treeProxyModel = new Models::ArchiveSortFilterProxyModel(this);
        m_treeProxyModel->setSourceModel(m_treeModel);
        m_treeProxyModel->setRecursiveFilteringEnabled(true);
        m_treeProxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);

        m_contentsModel = new FolderContentsModel(this);
        m_contentsProxyModel = new Models::ArchiveSortFilterProxyModel(this);
        m_contentsProxyModel->setSourceModel(m_contentsModel);
        m_contentsProxyModel->setRecursiveFilteringEnabled(true);
        m_contentsProxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);

        m_breadcrumb->SetSubfolderProvider(
            [this](const QString& path) -> QStringList {
                if (!m_treeModel || !m_treeModel->GetRootNode())
                    return {};
                const ArchiveNode* current = m_treeModel->GetRootNode().get();
                if (path != "/" && !path.isEmpty()) {
                    const QStringList parts =
                        path.split('/', Qt::SkipEmptyParts);
                    for (const auto& part : parts) {
                        bool found = false;
                        for (const auto& child : current->children) {
                            if (child->name.c_str() == part) {
                                current = child.get();
                                found = true;
                                break;
                            }
                        }
                        if (!found)
                            return {};
                    }
                }
                QStringList subfolders;
                for (const auto& child : current->children) {
                    if (child->isDirectory) {
                        subfolders.append(QString::fromStdString(child->name));
                    }
                }
                return subfolders;
            });

        auto* splitter = new QSplitter(Qt::Horizontal, this);

        m_treeView = new QTreeView(splitter);
        m_treeView->setModel(m_treeProxyModel);
        m_treeView->setHeaderHidden(true);
        m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
        m_treeView->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_treeView->setSelectionMode(QAbstractItemView::SingleSelection);
        connect(m_treeView->selectionModel(),
                &QItemSelectionModel::currentChanged, this,
                &FileManagerView::OnTreeSelectionChanged);
        connect(m_treeView, &QTreeView::customContextMenuRequested, this,
                &FileManagerView::OnTreeContextMenuRequested);

        m_contentsView = new ContentsTreeView(splitter);
        m_contentsView->setModel(m_contentsProxyModel);
        m_contentsView->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_contentsView->setSelectionMode(QAbstractItemView::ExtendedSelection);
        m_contentsView->setSortingEnabled(true);
        m_contentsView->header()->setSectionsClickable(true);
        m_contentsView->setContextMenuPolicy(Qt::CustomContextMenu);
        m_contentsView->setRootIsDecorated(true);
        m_contentsView->setItemsExpandable(true);
        m_contentsView->setExpandsOnDoubleClick(false);

        m_contentsView->setColumnWidth(FolderContentsModel::Name, 260);
        m_contentsView->setColumnWidth(FolderContentsModel::Type, 70);
        m_contentsView->setColumnWidth(FolderContentsModel::CompressedSize,
                                       110);
        m_contentsView->setColumnWidth(FolderContentsModel::UncompressedSize,
                                       110);
        m_contentsView->setColumnWidth(FolderContentsModel::Offset, 95);
        m_contentsView->setColumnWidth(FolderContentsModel::Compression, 85);
        m_contentsView->setColumnWidth(FolderContentsModel::Hash, 110);

        m_contentsProxyModel->sort(0, Qt::AscendingOrder);

        connect(m_contentsView, &QTreeView::doubleClicked, this,
                &FileManagerView::OnContentsDoubleClicked);
        connect(m_contentsView, &QTreeView::customContextMenuRequested, this,
                &FileManagerView::OnContextMenuRequested);
        connect(m_contentsView, &ContentsTreeView::RequestDragIndexes, this,
                &FileManagerView::OnContentsDragIndexes);
        connect(m_contentsView, &ContentsTreeView::RequestNavigateSelected,
                this, [this]() {
                    OnContentsDoubleClicked(m_contentsView->currentIndex());
                });
        connect(m_contentsView, &ContentsTreeView::RequestDeleteSelected, this,
                [this]() {
                    auto nodes = GetSelectedNodes();
                    if (!nodes.empty()) {
                        emit RequestDeleteNodes(nodes);
                    } else {
                        const ArchiveNode* node = GetSelectedNode();
                        if (node && node != m_currentFolderNode) {
                            emit RequestDeleteNode(node);
                        }
                    }
                });

        splitter->setSizes({240, 840});
        mainLayout->addWidget(splitter, 1);

        m_dropOverlayLabel =
            new QLabel("Drop files or archives to add / open", this);
        m_dropOverlayLabel->setAlignment(Qt::AlignCenter);
        m_dropOverlayLabel->setStyleSheet(
            "background-color: rgba(30, 144, 255, 180);"
            "color: white;"
            "font-size: 16px;"
            "font-weight: bold;"
            "border: 2px dashed white;"
            "border-radius: 8px;");
        m_dropOverlayLabel->hide();
    }

    void FileManagerView::SetArchiveRoot(std::shared_ptr<ArchiveNode> root) {
        m_treeModel->SetRootNode(root);
        m_currentFolderNode = m_treeModel->GetRootNode().get();
        m_contentsModel->SetFolderNode(m_currentFolderNode);
        m_breadcrumb->SetPath("/");
        m_treeProxyModel->sort(0, Qt::AscendingOrder);
        m_contentsProxyModel->sort(0, Qt::AscendingOrder);

        // Expand root "/" item in the sidebar so subfolders are immediately
        // visible
        QModelIndex rootProxyIdx = m_treeProxyModel->index(0, 0);
        if (rootProxyIdx.isValid()) {
            m_treeView->expand(rootProxyIdx);
            m_treeView->setCurrentIndex(rootProxyIdx);
        }
    }

    void FileManagerView::SetSearchFilter(const QString& filterText) {
        if (m_contentsProxyModel) {
            m_contentsProxyModel->setFilterWildcard(filterText);
        }
    }

    void FileManagerView::FocusLocationInput() {
        if (m_breadcrumb) {
            m_breadcrumb->FocusLocationInput();
        }
    }

    void FileManagerView::SetCurrentFolder(const ArchiveNode* folderNode) {
        if (!folderNode)
            return;
        m_currentFolderNode = folderNode;
        m_contentsModel->SetFolderNode(m_currentFolderNode);
        m_contentsProxyModel->sort(0, Qt::AscendingOrder);

        QString fullPath = QString::fromStdString(folderNode->fullPath);
        if (fullPath.isEmpty() ||
            folderNode == m_treeModel->GetRootNode().get()) {
            fullPath = "/";
        }
        m_breadcrumb->SetPath(fullPath);
        emit FolderChanged(fullPath);
    }

    void FileManagerView::NavigateToPath(const QString& path) {
        if (!m_treeModel || !m_treeModel->GetRootNode())
            return;
        const ArchiveNode* root = m_treeModel->GetRootNode().get();
        if (path == "/" || path.isEmpty()) {
            SetCurrentFolder(root);
            return;
        }
        const ArchiveNode* current = root;
        QStringList parts = path.split('/', Qt::SkipEmptyParts);
        for (const auto& part : parts) {
            bool found = false;
            for (const auto& child : current->children) {
                if (child->name.c_str() == part) {
                    current = child.get();
                    found = true;
                    break;
                }
            }
            if (!found)
                break;
        }
        SetCurrentFolder(current);
    }

    const ArchiveNode* FileManagerView::GetSelectedNode() const {
        QModelIndex proxyIdx = m_contentsView->currentIndex();
        if (!proxyIdx.isValid())
            return nullptr;
        QModelIndex srcIdx = m_contentsProxyModel->mapToSource(proxyIdx);
        return m_contentsModel->NodeFromIndex(srcIdx);
    }

    std::vector<const ArchiveNode*> FileManagerView::GetSelectedNodes() const {
        std::vector<const ArchiveNode*> nodes;
        if (!m_contentsView->selectionModel())
            return nodes;
        auto selectedRows = m_contentsView->selectionModel()->selectedRows();
        for (const auto& proxyIdx : selectedRows) {
            if (proxyIdx.isValid()) {
                QModelIndex srcIdx =
                    m_contentsProxyModel->mapToSource(proxyIdx);
                const ArchiveNode* node =
                    m_contentsModel->NodeFromIndex(srcIdx);
                if (node)
                    nodes.push_back(node);
            }
        }
        return nodes;
    }

    void
    FileManagerView::OnTreeSelectionChanged(const QModelIndex& current,
                                            const QModelIndex& /*previous*/) {
        if (m_isSyncingTreeSelection || !current.isValid())
            return;
        QModelIndex srcIdx = m_treeProxyModel->mapToSource(current);
        const ArchiveNode* node = m_treeModel->NodeFromIndex(srcIdx);
        if (node && node->isDirectory) {
            SetCurrentFolder(node);
            emit FolderChanged(QString::fromStdString(node->fullPath));
        }
    }

    void FileManagerView::OnContentsDoubleClicked(const QModelIndex& index) {
        if (!index.isValid())
            return;
        QModelIndex srcIdx = m_contentsProxyModel->mapToSource(index);
        const ArchiveNode* node = m_contentsModel->NodeFromIndex(srcIdx);
        if (!node)
            return;

        if (node->isDirectory) {
            if (m_contentsView->isExpanded(index)) {
                m_contentsView->collapse(index);
            } else {
                m_contentsView->expand(index);
            }
        } else {
            if (node->entryRef) {
                emit RequestOpenFile(node, false, false);
            }
        }
    }

    void FileManagerView::OnContentsDragStart(const QModelIndex& index) {
        if (!index.isValid())
            return;
        auto selectedNodes = GetSelectedNodes();
        if (selectedNodes.empty()) {
            QModelIndex srcIdx = m_contentsProxyModel->mapToSource(index);
            const ArchiveNode* node = m_contentsModel->NodeFromIndex(srcIdx);
            if (node) {
                emit RequestDragOutNodes({node});
                emit RequestDragOut(node);
            }
        } else {
            emit RequestDragOutNodes(selectedNodes);
        }
    }

    void
    FileManagerView::OnContentsDragIndexes(const QModelIndexList& indexes) {
        std::vector<const ArchiveNode*> nodes;
        for (const auto& proxyIdx : indexes) {
            if (proxyIdx.isValid()) {
                QModelIndex srcIdx =
                    m_contentsProxyModel->mapToSource(proxyIdx);
                const ArchiveNode* node =
                    m_contentsModel->NodeFromIndex(srcIdx);
                if (node)
                    nodes.push_back(node);
            }
        }
        if (!nodes.empty()) {
            emit RequestDragOutNodes(nodes);
        }
    }

    void FileManagerView::OnContextMenuRequested(const QPoint& pos) {
        auto selectedNodes = GetSelectedNodes();

        QMenu contextMenu(this);

        if (selectedNodes.size() > 1) {
            QAction* extractSelAct =
                contextMenu.addAction(QIcon::fromTheme("archive-extract"),
                                      QString("Extract %1 Selected Items...")
                                          .arg(selectedNodes.size()));
            connect(extractSelAct, &QAction::triggered, this,
                    [this, selectedNodes]() {
                        emit RequestExtractNodes(selectedNodes);
                    });

            contextMenu.addSeparator();

            QAction* deleteSelAct = contextMenu.addAction(
                QIcon::fromTheme("edit-delete"),
                QString("Delete %1 Selected Items").arg(selectedNodes.size()));
            deleteSelAct->setShortcut(QKeySequence::Delete);
            connect(deleteSelAct, &QAction::triggered, this,
                    [this, selectedNodes]() {
                        emit RequestDeleteNodes(selectedNodes);
                    });

            contextMenu.addSeparator();
        } else if (selectedNodes.size() == 1 ||
                   m_contentsView->indexAt(pos).isValid()) {
            const ArchiveNode* node =
                !selectedNodes.empty() ? selectedNodes[0] : nullptr;
            if (!node) {
                QModelIndex proxyIdx = m_contentsView->indexAt(pos);
                if (proxyIdx.isValid()) {
                    QModelIndex srcIdx =
                        m_contentsProxyModel->mapToSource(proxyIdx);
                    node = m_contentsModel->NodeFromIndex(srcIdx);
                }
            }

            if (node) {
                if (node->isDirectory) {
                    QAction* extractFolderAct = contextMenu.addAction(
                        QIcon::fromTheme("archive-extract"),
                        QString("Extract Folder '%1'...")
                            .arg(QString::fromStdString(node->name)));
                    connect(extractFolderAct, &QAction::triggered, this,
                            [this, node]() { emit RequestExtractNode(node); });
                } else {
                    QAction* openAct = contextMenu.addAction(
                        QIcon::fromTheme("document-open"),
                        QString("Open '%1'")
                            .arg(QString::fromStdString(node->name)));
                    connect(openAct, &QAction::triggered, this, [this, node]() {
                        emit RequestOpenFile(node, false, false);
                    });

                    QAction* openWithAct =
                        contextMenu.addAction(QIcon::fromTheme("document-open"),
                                              "Open with system app");
                    connect(openWithAct, &QAction::triggered, this,
                            [this, node]() {
                                emit RequestOpenFile(node, false, true);
                            });

                    contextMenu.addSeparator();

                    QAction* extractFileAct = contextMenu.addAction(
                        QIcon::fromTheme("archive-extract"), "Extract File...");
                    connect(extractFileAct, &QAction::triggered, this,
                            [this, node]() { emit RequestExtractNode(node); });
                }

                contextMenu.addSeparator();

                QAction* deleteAct = contextMenu.addAction(
                    QIcon::fromTheme("edit-delete"), "Delete");
                deleteAct->setShortcut(QKeySequence::Delete);
                connect(deleteAct, &QAction::triggered, this,
                        [this, node]() { emit RequestDeleteNode(node); });

                contextMenu.addSeparator();

                QAction* copyPathAct = contextMenu.addAction(
                    QIcon::fromTheme("edit-copy"), "Copy Path");
                connect(copyPathAct, &QAction::triggered, this, [node]() {
                    QApplication::clipboard()->setText(
                        QString::fromStdString(node->fullPath));
                });

                if (node->entryRef) {
                    QAction* copyHashAct = contextMenu.addAction(
                        QIcon::fromTheme("edit-copy"), "Copy Hash/Path ID");
                    connect(copyHashAct, &QAction::triggered, this, [node]() {
                        QApplication::clipboard()->setText(
                            QString::fromStdString(std::format(
                                "0x{:X}", node->entryRef->name_hash)));
                    });
                }
                contextMenu.addSeparator();
            }
        }

        QAction* extractAllAct = contextMenu.addAction(
            QIcon::fromTheme("archive-extract"), "Extract All from Archive...");
        connect(extractAllAct, &QAction::triggered, this,
                [this]() { emit RequestExtractAll(); });

        contextMenu.addSeparator();

        QAction* addFileAct = contextMenu.addAction(
            QIcon::fromTheme("document-new"), "Add File to Archive...");
        connect(addFileAct, &QAction::triggered, this,
                [this]() { emit RequestAddFile(); });

        QAction* addFolderAct = contextMenu.addAction(
            QIcon::fromTheme("folder-new"), "Add Folder to Archive...");
        connect(addFolderAct, &QAction::triggered, this,
                [this]() { emit RequestAddFolder(); });

        QAction* newFolderAct = contextMenu.addAction(
            QIcon::fromTheme("folder-new"), "New Folder...");
        connect(newFolderAct, &QAction::triggered, this,
                [this]() { emit RequestNewFolder(); });

        contextMenu.exec(m_contentsView->viewport()->mapToGlobal(pos));
    }

    void FileManagerView::OnTreeContextMenuRequested(const QPoint& pos) {
        QModelIndex proxyIdx = m_treeView->indexAt(pos);
        const ArchiveNode* node = nullptr;
        if (proxyIdx.isValid()) {
            QModelIndex srcIdx = m_treeProxyModel->mapToSource(proxyIdx);
            node = m_treeModel->NodeFromIndex(srcIdx);
        }

        QMenu contextMenu(this);

        if (node) {
            QAction* extractFolderAct = contextMenu.addAction(
                QIcon::fromTheme("archive-extract"),
                QString("Extract Folder '%1'...")
                    .arg(QString::fromStdString(node->name)));
            connect(extractFolderAct, &QAction::triggered, this,
                    [this, node]() { emit RequestExtractNode(node); });

            contextMenu.addSeparator();

            if (node != m_treeModel->GetRootNode().get()) {
                QAction* deleteAct = contextMenu.addAction(
                    QIcon::fromTheme("edit-delete"),
                    QString("Delete Folder '%1'")
                        .arg(QString::fromStdString(node->name)));
                connect(deleteAct, &QAction::triggered, this,
                        [this, node]() { emit RequestDeleteNode(node); });
                contextMenu.addSeparator();
            }

            QAction* copyPathAct = contextMenu.addAction(
                QIcon::fromTheme("edit-copy"), "Copy Relative Path");
            connect(copyPathAct, &QAction::triggered, this, [node]() {
                QApplication::clipboard()->setText(
                    QString::fromStdString(node->fullPath));
            });
            contextMenu.addSeparator();
        }

        QAction* addFileAct = contextMenu.addAction(
            QIcon::fromTheme("document-new"), "Add File to Archive...");
        connect(addFileAct, &QAction::triggered, this,
                [this]() { emit RequestAddFile(); });

        QAction* addFolderAct = contextMenu.addAction(
            QIcon::fromTheme("folder-new"), "Add Folder to Archive...");
        connect(addFolderAct, &QAction::triggered, this,
                [this]() { emit RequestAddFolder(); });

        QAction* newFolderAct = contextMenu.addAction(
            QIcon::fromTheme("folder-new"), "New Folder...");
        connect(newFolderAct, &QAction::triggered, this,
                [this]() { emit RequestNewFolder(); });

        contextMenu.exec(m_treeView->viewport()->mapToGlobal(pos));
    }

    void FileManagerView::dragEnterEvent(QDragEnterEvent* event) {
        if (event->mimeData()->hasUrls()) {
            event->acceptProposedAction();
            m_dropOverlayLabel->setGeometry(rect().adjusted(10, 10, -10, -10));
            m_dropOverlayLabel->show();
            m_dropOverlayLabel->raise();
        }
    }

    void FileManagerView::dragMoveEvent(QDragMoveEvent* event) {
        if (event->mimeData()->hasUrls()) {
            event->acceptProposedAction();
        }
    }

    void FileManagerView::dragLeaveEvent(QDragLeaveEvent* event) {
        m_dropOverlayLabel->hide();
        event->accept();
    }

    void FileManagerView::dropEvent(QDropEvent* event) {
        m_dropOverlayLabel->hide();
        if (!event->mimeData()->hasUrls())
            return;

        QStringList files;
        QString droppedArchive;

        for (const QUrl& url : event->mimeData()->urls()) {
            if (url.isLocalFile()) {
                QString path = url.toLocalFile();
                if (path.endsWith(".fat", Qt::CaseInsensitive)) {
                    droppedArchive = path;
                } else {
                    files.append(path);
                }
            }
        }

        if (!droppedArchive.isEmpty()) {
            emit ArchiveFileDropped(droppedArchive);
        } else if (!files.isEmpty()) {
            QString currentTargetRelPath =
                m_currentFolderNode
                    ? QString::fromStdString(m_currentFolderNode->fullPath)
                    : "/";

            // Check if dropped onto a specific directory item in contents view
            // or tree view
            QPoint p = event->position().toPoint();
            QPoint contentsP = m_contentsView->viewport()->mapFrom(this, p);
            QModelIndex cIdx = m_contentsView->indexAt(contentsP);
            if (cIdx.isValid()) {
                QModelIndex srcIdx = m_contentsProxyModel->mapToSource(cIdx);
                const ArchiveNode* targetNode =
                    m_contentsModel->NodeFromIndex(srcIdx);
                if (targetNode && targetNode->isDirectory) {
                    currentTargetRelPath =
                        QString::fromStdString(targetNode->fullPath);
                }
            } else {
                QPoint treeP = m_treeView->viewport()->mapFrom(this, p);
                QModelIndex tIdx = m_treeView->indexAt(treeP);
                if (tIdx.isValid()) {
                    QModelIndex srcIdx = m_treeProxyModel->mapToSource(tIdx);
                    const ArchiveNode* targetNode =
                        m_treeModel->NodeFromIndex(srcIdx);
                    if (targetNode && targetNode->isDirectory) {
                        currentTargetRelPath =
                            QString::fromStdString(targetNode->fullPath);
                    }
                }
            }

            emit FilesDropped(files, currentTargetRelPath);
        }
        event->acceptProposedAction();
    }

} // namespace BFView::Views
