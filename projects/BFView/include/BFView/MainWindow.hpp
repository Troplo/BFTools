#pragma once

#include "BFView/Models/ArchiveTreeModel.hpp"
#include "BFView/Models/NavigationHistory.hpp"
#include "BFView/Views/FileManagerView.hpp"
#include "Nomad/Archives/DirectoryTree.hpp"
#include "Nomad/Archives/IArchive.hpp"
#include <QMainWindow>
#include <memory>
#include <string>
#include <vector>

class QLineEdit;
class QToolBar;
class QAction;

namespace BFView {
    class MainWindow : public QMainWindow {
        Q_OBJECT

      public:
        explicit MainWindow(QWidget *parent = nullptr);
        ~MainWindow() override = default;

        void OpenArchiveFile(const QString &path,
                             const QString &explicitProject = QString());
        void CloseArchiveFile();

      private slots:
        void onNewAction();
        void onOpenAction();
        void onCloseAction();
        void onExtractAllAction();
        void onExtractSelectedAction();
        void onAddFileAction();
        void onAddFolderAction();
        void onNewFolderAction();
        void onDeleteSelectedAction();
        void onShowArchiveInfoAction();
        void onFilterTextChanged(const QString &text);
        void onFilesDropped(const QStringList &filePaths,
                            const QString &targetFolderRelPath);
        void onArchiveFileDropped(const QString &fatPath);
        void onRequestDragOut(const Nomad::Archives::ArchiveNode *node);
        void onRequestDragOutNodes(
            const std::vector<const Nomad::Archives::ArchiveNode *> &nodes);
        void onRequestExtractNode(const Nomad::Archives::ArchiveNode *node);
        void onRequestExtractNodes(
            const std::vector<const Nomad::Archives::ArchiveNode *> &nodes);
        void onRequestDeleteNode(const Nomad::Archives::ArchiveNode *node);
        void onRequestDeleteNodes(
            const std::vector<const Nomad::Archives::ArchiveNode *> &nodes);
        void onRequestOpenFile(const Nomad::Archives::ArchiveNode *node,
                               bool bypassCustomHandler, bool openWithChooser);
        void onSyncGenerateNfoAction();
        void onConfigureSettingsAction();
        void onChangeProjectAction();
        void onDefragmentAction();

        void onBackAction();
        void onForwardAction();
        void onUpAction();
        void onFolderChanged(const QString &fullPath);

      protected:
        void closeEvent(QCloseEvent *event) override;

      private:
        void setupUi();
        void performExtraction(
            const std::vector<const Nomad::Archives::ArchiveEntry *>
                &entriesToExtract,
            const QString &destinationDir);
        void saveArchive();
        void updateNavigationButtons();
        void updateWindowTitle();
        void checkDefaultFileAssociation();

        std::unique_ptr<Nomad::Archives::IArchive> m_currentArchive;
        QString m_currentFatPath;
        QString m_currentProjectName;

        Models::NavigationHistory m_navHistory;
        bool m_isNavigatingHistory{false};

        QToolBar *m_toolBar{nullptr};
        QAction *m_actBack{nullptr};
        QAction *m_actForward{nullptr};
        QAction *m_actUp{nullptr};

        QAction *m_actThemeSystem{nullptr};
        QAction *m_actThemeBreezeLight{nullptr};
        QAction *m_actThemeBreezeDark{nullptr};

        QLineEdit *m_searchEdit{nullptr};
        Views::FileManagerView *m_fileManagerView{nullptr};
        bool m_isLoadingArchive{false};
    };

} // namespace BFView
