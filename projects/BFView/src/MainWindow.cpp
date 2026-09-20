#include "BFView/MainWindow.hpp"
#include "BFView/Dialogs/DefragProgressDialog.hpp"
#include "BFView/Dialogs/ExtractProgressDialog.hpp"
#include "BFView/Dialogs/NewArchiveDialog.hpp"
#include "BFView/Dialogs/ProjectSelectDialog.hpp"
#include "BFView/Dialogs/SettingsDialog.hpp"
#include "BFView/Handlers/FileHandlerManager.hpp"
#include "BFView/Settings/SettingsManager.hpp"
#include "BFView/Theme/ThemeManager.hpp"
#include "Nomad/Archives/ArchiveFactory.hpp"
#include "Nomad/Archives/ArchiveModifier.hpp"
#include "Nomad/Archives/Defragmenter.hpp"
#include "Nomad/Archives/DirectoryTree.hpp"
#include "Nomad/Archives/Extractor.hpp"
#include "Nomad/Archives/Logger.hpp"
#include "Nomad/Archives/ProjectManager.hpp"
#include "Nomad/Archives/ThreadPool.hpp"
#include <QActionGroup>
#include <QApplication>
#include <QDrag>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressDialog>
#include <QPushButton>
#include <QShortcut>
#include <QStatusBar>
#include <QToolBar>
#include <QUrl>
#include <QVBoxLayout>
#include <atomic>
#include <chrono>
#include <fstream>
#include <future>
#include <thread>

using namespace Nomad::Archives;

namespace BFView {
    MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
        setupUi();
        updateWindowTitle();

        auto geometry = SettingsManager::GetInstance().GetGeometry();
        if (!geometry.isEmpty()) {
            restoreGeometry(geometry);
        }

        // Set initial checked theme state
        AppTheme currentTheme = ThemeManager::GetInstance().GetCurrentTheme();
        if (m_actThemeSystem)
            m_actThemeSystem->setChecked(currentTheme == AppTheme::System);
        if (m_actThemeBreezeLight)
            m_actThemeBreezeLight->setChecked(currentTheme ==
                                              AppTheme::BreezeLight);
        if (m_actThemeBreezeDark)
            m_actThemeBreezeDark->setChecked(currentTheme ==
                                             AppTheme::BreezeDark);

        connect(&ThemeManager::GetInstance(), &ThemeManager::ThemeChanged, this,
                [this](AppTheme theme) {
                    if (m_actThemeSystem)
                        m_actThemeSystem->setChecked(theme == AppTheme::System);
                    if (m_actThemeBreezeLight)
                        m_actThemeBreezeLight->setChecked(
                            theme == AppTheme::BreezeLight);
                    if (m_actThemeBreezeDark)
                        m_actThemeBreezeDark->setChecked(theme ==
                                                         AppTheme::BreezeDark);
                });
    }

    void MainWindow::setupUi() {
        // Menu Bar
        QMenu *fileMenu = menuBar()->addMenu("&File");

        QAction *newAct = fileMenu->addAction(QIcon::fromTheme("document-new"),
                                              "&New Archive...");
        newAct->setShortcut(QKeySequence::New);
        connect(newAct, &QAction::triggered, this, &MainWindow::onNewAction);

        QAction *openAct = fileMenu->addAction(
            QIcon::fromTheme("document-open"), "&Open Archive...");
        openAct->setShortcut(QKeySequence::Open);
        connect(openAct, &QAction::triggered, this, &MainWindow::onOpenAction);

        QAction *closeAct = fileMenu->addAction(
            QIcon::fromTheme("window-close"), "&Close Archive");
        closeAct->setShortcut(QKeySequence::Close);
        connect(closeAct, &QAction::triggered, this,
                &MainWindow::onCloseAction);

        fileMenu->addSeparator();

        QAction *changeProjectAct = fileMenu->addAction(
            QIcon::fromTheme("project-development"), "Change &Project...");
        connect(changeProjectAct, &QAction::triggered, this,
                &MainWindow::onChangeProjectAction);

        fileMenu->addSeparator();

        QAction *exitAct =
            fileMenu->addAction(QIcon::fromTheme("application-exit"), "E&xit");
        exitAct->setShortcut(QKeySequence::Quit);
        connect(exitAct, &QAction::triggered, this, &QWidget::close);

        QMenu *actionsMenu = menuBar()->addMenu("&Actions");

        QAction *extractAllAct = actionsMenu->addAction(
            QIcon::fromTheme("archive-extract"), "Extract &All...");
        extractAllAct->setShortcut(
            QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E));
        connect(extractAllAct, &QAction::triggered, this,
                &MainWindow::onExtractAllAction);

        QAction *extractSelAct = actionsMenu->addAction(
            QIcon::fromTheme("archive-extract"), "Extract &Selected...");
        extractSelAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
        connect(extractSelAct, &QAction::triggered, this,
                &MainWindow::onExtractSelectedAction);

        actionsMenu->addSeparator();

        QAction *addFileAct = actionsMenu->addAction(
            QIcon::fromTheme("document-new"), "&Add File...");
        addFileAct->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_A));
        connect(addFileAct, &QAction::triggered, this,
                &MainWindow::onAddFileAction);

        QAction *addFolderAct = actionsMenu->addAction(
            QIcon::fromTheme("folder-new"), "Add &Folder...");
        addFolderAct->setShortcut(
            QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F));
        connect(addFolderAct, &QAction::triggered, this,
                &MainWindow::onAddFolderAction);

        QAction *newFolderAct = actionsMenu->addAction(
            QIcon::fromTheme("folder-new"), "&New Folder...");
        newFolderAct->setShortcut(
            QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N));
        connect(newFolderAct, &QAction::triggered, this,
                &MainWindow::onNewFolderAction);

        QAction *deleteAct = actionsMenu->addAction(
            QIcon::fromTheme("edit-delete"), "&Delete Selected");
        deleteAct->setShortcut(QKeySequence::Delete);
        connect(deleteAct, &QAction::triggered, this,
                &MainWindow::onDeleteSelectedAction);

        actionsMenu->addSeparator();

        QAction *syncNfoAct = actionsMenu->addAction(
            QIcon::fromTheme("document-save"), "&Sync / Generate NFO");
        syncNfoAct->setShortcut(QKeySequence::Save);
        connect(syncNfoAct, &QAction::triggered, this,
                &MainWindow::onSyncGenerateNfoAction);

        QAction *defragAct = actionsMenu->addAction(
            QIcon::fromTheme("drive-harddisk"), "&Defragment Archive...");
        defragAct->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D));
        connect(defragAct, &QAction::triggered, this,
                &MainWindow::onDefragmentAction);

        // View Menu
        QMenu *viewMenu = menuBar()->addMenu("&View");
        QMenu *themeMenu =
            viewMenu->addMenu(QIcon::fromTheme("preferences-system"), "&Theme");

        auto *themeGroup = new QActionGroup(this);
        themeGroup->setExclusive(true);

        m_actThemeSystem = themeMenu->addAction("&System Default");
        m_actThemeSystem->setCheckable(true);
        themeGroup->addAction(m_actThemeSystem);
        connect(m_actThemeSystem, &QAction::triggered, this, []() {
            ThemeManager::GetInstance().ApplyTheme(AppTheme::System);
            SettingsManager::GetInstance().SetAppTheme("System");
            SettingsManager::GetInstance().Save();
        });

        m_actThemeBreezeLight =
            themeMenu->addAction("Breeze &Light (KDE Plasma)");
        m_actThemeBreezeLight->setCheckable(true);
        themeGroup->addAction(m_actThemeBreezeLight);
        connect(m_actThemeBreezeLight, &QAction::triggered, this, []() {
            ThemeManager::GetInstance().ApplyTheme(AppTheme::BreezeLight);
            SettingsManager::GetInstance().SetAppTheme("BreezeLight");
            SettingsManager::GetInstance().Save();
        });

        m_actThemeBreezeDark =
            themeMenu->addAction("Breeze &Dark (KDE Plasma)");
        m_actThemeBreezeDark->setCheckable(true);
        themeGroup->addAction(m_actThemeBreezeDark);
        connect(m_actThemeBreezeDark, &QAction::triggered, this, []() {
            ThemeManager::GetInstance().ApplyTheme(AppTheme::BreezeDark);
            SettingsManager::GetInstance().SetAppTheme("BreezeDark");
            SettingsManager::GetInstance().Save();
        });

        QMenu *settingsMenu = menuBar()->addMenu("&Settings");
        QAction *configAct = settingsMenu->addAction(
            QIcon::fromTheme("preferences-system"), "&Configure Settings...");
        configAct->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_P));
        connect(configAct, &QAction::triggered, this,
                &MainWindow::onConfigureSettingsAction);

        QMenu *helpMenu = menuBar()->addMenu("&Help");
        QAction *infoAct = helpMenu->addAction(QIcon::fromTheme("help-about"),
                                               "Archive &Info...");
        infoAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_I));
        connect(infoAct, &QAction::triggered, this,
                &MainWindow::onShowArchiveInfoAction);

        helpMenu->addSeparator();
        QAction *aboutAct =
            helpMenu->addAction(QIcon::fromTheme("help-about"), "&About");
        connect(aboutAct, &QAction::triggered, this, [this]() {
            QMessageBox::about(
                this, "About BFView",
                "<h3>BFTools (NexusTools)</h3>"
                "<p>Unpack, modify, and create Watch_Dogs bigfile archives.</p>"
                "<p>"
                "<a href=\"https://github.com/Troplo/BFTools\">GitHub</a> - <a "
                "href=\"https://wd.troplo.com\">NexusTools Documentation</a>"
                "<h4>Credits</h4>"
                "<ul>"
                "<li>Developer: Troplo</li>"
                "<li>Thanks to Gibbed for creating the Gibbed Disrupt/Dunia "
                "project, "
                "and documenting the archive format</li>"
                "<li>Thanks: miru, hardVatsuki, The Silver, Parallellines</li>"
                "</ul>"
                "</p>");
        });

        // Global Shortcuts
        auto *focusLocationShortcut =
            new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_L), this);
        connect(focusLocationShortcut, &QShortcut::activated, this, [this]() {
            if (m_fileManagerView) {
                m_fileManagerView->FocusLocationInput();
            }
        });

        // Top Toolbar (Dolphin Style Navigation)
        m_toolBar = addToolBar("Navigation");
        m_toolBar->setMovable(false);

        m_actBack =
            m_toolBar->addAction(QIcon::fromTheme("go-previous"), "< Back");
        m_actBack->setToolTip("Back (Alt+Left)");
        m_actBack->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Left));
        m_actBack->setEnabled(false);
        connect(m_actBack, &QAction::triggered, this,
                &MainWindow::onBackAction);

        m_actForward =
            m_toolBar->addAction(QIcon::fromTheme("go-next"), "Forward >");
        m_actForward->setToolTip("Forward (Alt+Right)");
        m_actForward->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Right));
        m_actForward->setEnabled(false);
        connect(m_actForward, &QAction::triggered, this,
                &MainWindow::onForwardAction);

        m_actUp = m_toolBar->addAction(QIcon::fromTheme("go-up"), "^ Up");
        m_actUp->setToolTip("Up (Alt+Up)");
        m_actUp->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Up));
        m_actUp->setEnabled(false);
        connect(m_actUp, &QAction::triggered, this, &MainWindow::onUpAction);

        connect(&m_navHistory, &Models::NavigationHistory::StateChanged, this,
                [this](bool canBack, bool canForward, bool canUp) {
                    if (m_actBack)
                        m_actBack->setEnabled(canBack);
                    if (m_actForward)
                        m_actForward->setEnabled(canForward);
                    if (m_actUp)
                        m_actUp->setEnabled(canUp);
                });

        m_toolBar->addSeparator();

        QAction *tbOpen = m_toolBar->addAction(
            QIcon::fromTheme("document-open"), "Open Archive");
        connect(tbOpen, &QAction::triggered, this, &MainWindow::onOpenAction);

        m_toolBar->addSeparator();

        QAction *tbExtractAll = m_toolBar->addAction(
            QIcon::fromTheme("archive-extract"), "Extract All");
        connect(tbExtractAll, &QAction::triggered, this,
                &MainWindow::onExtractAllAction);

        QAction *tbExtractSel = m_toolBar->addAction(
            QIcon::fromTheme("archive-extract"), "Extract Selected");
        connect(tbExtractSel, &QAction::triggered, this,
                &MainWindow::onExtractSelectedAction);

        m_toolBar->addSeparator();

        QAction *tbAddFile =
            m_toolBar->addAction(QIcon::fromTheme("document-new"), "Add File");
        connect(tbAddFile, &QAction::triggered, this,
                &MainWindow::onAddFileAction);

        QAction *tbAddFolder =
            m_toolBar->addAction(QIcon::fromTheme("folder-new"), "Add Folder");
        connect(tbAddFolder, &QAction::triggered, this,
                &MainWindow::onAddFolderAction);

        QAction *tbDelete =
            m_toolBar->addAction(QIcon::fromTheme("edit-delete"), "Delete");
        connect(tbDelete, &QAction::triggered, this,
                &MainWindow::onDeleteSelectedAction);

        m_toolBar->addSeparator();

        QAction *tbDefrag =
            m_toolBar->addAction(QIcon::fromTheme("drive-harddisk"), "Defrag");
        tbDefrag->setToolTip("Defragment Archive (Reclaim Slack Space)");
        connect(tbDefrag, &QAction::triggered, this,
                &MainWindow::onDefragmentAction);

        m_toolBar->addSeparator();

        QAction *tbInfo = m_toolBar->addAction(QIcon::fromTheme("help-about"),
                                               "Archive Info");
        connect(tbInfo, &QAction::triggered, this,
                &MainWindow::onShowArchiveInfoAction);

        m_toolBar->addSeparator();

        auto *searchWidget = new QWidget(this);
        auto *searchLayout = new QHBoxLayout(searchWidget);
        searchLayout->setContentsMargins(5, 0, 5, 0);

        m_searchEdit = new QLineEdit(searchWidget);
        m_searchEdit->setPlaceholderText("Search");
        connect(m_searchEdit, &QLineEdit::textChanged, this,
                &MainWindow::onFilterTextChanged);

        searchLayout->addWidget(m_searchEdit);
        m_toolBar->addWidget(searchWidget);

        // Central Widget Layout - 100% Viewport File Manager View
        auto *centralWidget = new QWidget(this);
        auto *mainLayout = new QVBoxLayout(centralWidget);
        mainLayout->setContentsMargins(6, 6, 6, 6);
        mainLayout->setSpacing(0);

        m_fileManagerView = new Views::FileManagerView(centralWidget);
        connect(m_fileManagerView, &Views::FileManagerView::FolderChanged, this,
                &MainWindow::onFolderChanged);
        connect(m_fileManagerView, &Views::FileManagerView::RequestExtractNode,
                this, &MainWindow::onRequestExtractNode);
        connect(m_fileManagerView, &Views::FileManagerView::RequestExtractNodes,
                this, &MainWindow::onRequestExtractNodes);
        connect(m_fileManagerView, &Views::FileManagerView::RequestOpenFile,
                this, &MainWindow::onRequestOpenFile);
        connect(m_fileManagerView, &Views::FileManagerView::RequestExtractAll,
                this, &MainWindow::onExtractAllAction);
        connect(m_fileManagerView, &Views::FileManagerView::RequestAddFile,
                this, &MainWindow::onAddFileAction);
        connect(m_fileManagerView, &Views::FileManagerView::RequestAddFolder,
                this, &MainWindow::onAddFolderAction);
        connect(m_fileManagerView, &Views::FileManagerView::RequestNewFolder,
                this, &MainWindow::onNewFolderAction);
        connect(m_fileManagerView, &Views::FileManagerView::RequestDeleteNode,
                this, &MainWindow::onRequestDeleteNode);
        connect(m_fileManagerView, &Views::FileManagerView::RequestDeleteNodes,
                this, &MainWindow::onRequestDeleteNodes);
        connect(m_fileManagerView, &Views::FileManagerView::FilesDropped, this,
                &MainWindow::onFilesDropped);
        connect(m_fileManagerView, &Views::FileManagerView::ArchiveFileDropped,
                this, &MainWindow::onArchiveFileDropped);
        connect(m_fileManagerView, &Views::FileManagerView::RequestDragOut,
                this, &MainWindow::onRequestDragOut);
        connect(m_fileManagerView, &Views::FileManagerView::RequestDragOutNodes,
                this, &MainWindow::onRequestDragOutNodes);

        mainLayout->addWidget(m_fileManagerView);
        setCentralWidget(centralWidget);

        statusBar()->showMessage("Ready. Open a .fat archive to begin.");
    }

    void MainWindow::onFilterTextChanged(const QString &text) {
        if (m_fileManagerView) {
            m_fileManagerView->SetSearchFilter(text);
        }
    }

    void MainWindow::onNewAction() {
        Dialogs::NewArchiveDialog dialog(this);
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }

        QString fatPathStr = dialog.GetFatPath();
        std::filesystem::path fatPath = fatPathStr.toStdString();
        if (fatPath.extension() != ".fat") {
            fatPath.replace_extension(".fat");
        }

        std::error_code ec;
        if (fatPath.has_parent_path()) {
            std::filesystem::create_directories(fatPath.parent_path(), ec);
        }

        auto archive = ArchiveFactory::Create(dialog.GetArchiveVersion());
        if (!archive) {
            QMessageBox::critical(this, tr("Error"),
                                  tr("Failed to create archive instance."));
            return;
        }

        archive->SetPlatform(dialog.GetPlatform());
        archive->SetRawVersion(dialog.GetRawVersion());

        QString selectedProject = dialog.GetSelectedProject();
        if (!selectedProject.isEmpty() &&
            selectedProject != SettingsManager::NONE_PROJECT) {
            auto projectConfig = ProjectManager::GetInstance().GetProject(
                selectedProject.toStdString());
            if (projectConfig) {
                archive->SetProjectConfig(projectConfig);
            }
        }

        std::ofstream fatFile(fatPath, std::ios::binary | std::ios::trunc);
        if (!fatFile.is_open() || !archive->Write(fatFile)) {
            QMessageBox::critical(
                this, tr("Error"),
                tr("Failed to write initial FAT file:\n%1").arg(fatPathStr));
            return;
        }
        fatFile.close();

        std::filesystem::path datPath = fatPath;
        datPath.replace_extension(".dat");
        std::ofstream datFile(datPath, std::ios::binary | std::ios::trunc);
        datFile.close();

        std::filesystem::path nfoPath = fatPath;
        nfoPath.replace_extension(".nfo");
        std::ofstream nfoFile(nfoPath, std::ios::trunc);
        if (nfoFile.is_open()) {
            archive->SerializeNfo(nfoFile);
            nfoFile.close();
        }

        SettingsManager::GetInstance().SetDefaultCompression(
            dialog.GetArchiveVersion(), dialog.GetRawVersion(),
            dialog.GetDefaultCompression());
        SettingsManager::GetInstance().Save();

        OpenArchiveFile(QString::fromStdString(fatPath.string()),
                        selectedProject.isEmpty()
                            ? SettingsManager::NONE_PROJECT
                            : selectedProject);
        statusBar()->showMessage(
            tr("Created new archive '%1'")
                .arg(QString::fromStdString(fatPath.filename().string())),
            5000);
    }

    void MainWindow::onOpenAction() {
        QString path = QFileDialog::getOpenFileName(
            this, "Open Archive FAT", QString(),
            "FAT Archives (*.fat);;All Files (*.*)");

        if (path.isEmpty())
            return;

        OpenArchiveFile(path);
    }

    void MainWindow::OpenArchiveFile(const QString &path,
                                     const QString &explicitProject) {
        std::filesystem::path fatP = path.toStdString();
        if (fatP.extension() != ".fat") {
            fatP.replace_extension(".fat");
        }

        ArchiveVersion version = ArchiveFactory::DetectVersion(fatP);
        uint32_t rawVersion = ArchiveFactory::DetectRawVersion(fatP);

        QString projectToUse = explicitProject;
        if (projectToUse == SettingsManager::NONE_PROJECT) {
            projectToUse = "";
        } else if (projectToUse.isEmpty()) {
            auto &settings = SettingsManager::GetInstance();
            QString defaultProj =
                settings.GetDefaultProject(version, rawVersion);
            if (!defaultProj.isEmpty() &&
                defaultProj != SettingsManager::ALWAYS_ASK) {
                if (defaultProj == SettingsManager::NONE_PROJECT) {
                    projectToUse = "";
                } else {
                    projectToUse = defaultProj;
                }
            } else {
                Dialogs::ProjectSelectDialog dialog(version, rawVersion, path,
                                                    this);
                if (dialog.exec() != QDialog::Accepted) {
                    return;
                }
                projectToUse = dialog.GetSelectedProject();
                if (dialog.IsAlwaysUseChecked()) {
                    settings.SetDefaultProject(
                        version, rawVersion,
                        projectToUse.isEmpty() ? SettingsManager::NONE_PROJECT
                                               : projectToUse);
                    settings.Save();
                }
            }
        }

        Logger::Info("BFView", "Opening archive '{}' with project '{}'...",
                     fatP.string(), projectToUse.toStdString());
        std::error_code ec;
        auto archive =
            ArchiveFactory::Open(fatP, ec, projectToUse.toStdString());
        if (ec) {
            QMessageBox::critical(
                this, "Error Opening Archive",
                QString("Failed to open FAT archive file:\n%1\n\nError: %2")
                    .arg(path)
                    .arg(QString::fromStdString(ec.message())));
            return;
        }

        m_currentArchive = std::move(archive);
        m_currentFatPath = QString::fromStdString(fatP.string());
        m_currentProjectName = projectToUse;

        m_navHistory.Clear();
        m_navHistory.NavigateTo("/");
        updateNavigationButtons();

        auto root = DirectoryTreeBuilder::Build(m_currentArchive->GetEntries());
        m_fileManagerView->SetArchiveRoot(root);

        QString projectLabel =
            m_currentProjectName.isEmpty() ? "None" : m_currentProjectName;
        statusBar()->showMessage(
            QString("Loaded archive: %1 (%2 entries) [Project: %3]")
                .arg(path)
                .arg(m_currentArchive->GetEntries().size())
                .arg(projectLabel));
        updateWindowTitle();
    }

    void MainWindow::onChangeProjectAction() {
        if (!m_currentArchive) {
            QMessageBox::information(this, tr("No Archive Open"),
                                     tr("Please open an archive first."));
            return;
        }

        Dialogs::ProjectSelectDialog dialog(m_currentArchive->GetVersion(),
                                            m_currentArchive->GetRawVersion(),
                                            m_currentFatPath, this);
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }

        QString newProj = dialog.GetSelectedProject();
        if (dialog.IsAlwaysUseChecked()) {
            auto &settings = SettingsManager::GetInstance();
            settings.SetDefaultProject(
                m_currentArchive->GetVersion(),
                m_currentArchive->GetRawVersion(),
                newProj.isEmpty() ? SettingsManager::NONE_PROJECT : newProj);
            settings.Save();
        }

        auto projConfig =
            ProjectManager::GetInstance().GetProject(newProj.toStdString());
        ArchiveFactory::ApplyProject(*m_currentArchive, projConfig);

        std::filesystem::path nfoPath = m_currentFatPath.toStdString();
        nfoPath.replace_extension(".nfo");
        if (std::filesystem::exists(nfoPath)) {
            ArchiveFactory::LoadNfoNames(nfoPath, *m_currentArchive);
        }

        if (projConfig) {
            bool is32 =
                (m_currentArchive->GetVersion() != ArchiveVersion::FATv5);
            for (auto &entry : m_currentArchive->GetEntries()) {
                auto overridePath =
                    projConfig->FindPathByHash(entry.name_hash, is32);
                if (overridePath.has_value()) {
                    entry.path = *overridePath;
                }
            }
        }

        for (auto &entry : m_currentArchive->GetEntries()) {
            entry.path = NormalizePath(entry.path);
        }

        m_currentProjectName = newProj;
        m_navHistory.Clear();
        m_navHistory.NavigateTo("/");
        updateNavigationButtons();

        auto root = DirectoryTreeBuilder::Build(m_currentArchive->GetEntries());
        m_fileManagerView->SetArchiveRoot(root);

        QString projectLabel =
            m_currentProjectName.isEmpty() ? "None" : m_currentProjectName;
        statusBar()->showMessage(
            QString("Project set to: %1").arg(projectLabel));
        updateWindowTitle();
    }

    void MainWindow::CloseArchiveFile() {
        m_currentArchive.reset();
        m_currentFatPath.clear();
        m_currentProjectName.clear();
        m_navHistory.Clear();
        updateNavigationButtons();

        auto emptyRoot = std::make_shared<ArchiveNode>();
        emptyRoot->name = "/";
        emptyRoot->fullPath = "/";
        emptyRoot->isDirectory = true;
        m_fileManagerView->SetArchiveRoot(emptyRoot);

        statusBar()->showMessage("Closed archive.");
        updateWindowTitle();
    }

    void MainWindow::onCloseAction() { CloseArchiveFile(); }

    void MainWindow::onBackAction() {
        if (!m_fileManagerView || !m_navHistory.CanGoBack())
            return;
        m_isNavigatingHistory = true;
        QString path = m_navHistory.GoBack();
        m_fileManagerView->NavigateToPath(path);
        m_fileManagerView->SetSearchFilter("");
        m_isNavigatingHistory = false;
        updateNavigationButtons();
    }

    void MainWindow::onForwardAction() {
        if (!m_fileManagerView || !m_navHistory.CanGoForward())
            return;
        m_isNavigatingHistory = true;
        QString path = m_navHistory.GoForward();
        m_fileManagerView->NavigateToPath(path);
        m_fileManagerView->SetSearchFilter("");
        m_isNavigatingHistory = false;
        updateNavigationButtons();
    }

    void MainWindow::onUpAction() {
        if (!m_fileManagerView || !m_currentArchive)
            return;
        const ArchiveNode *current = m_fileManagerView->GetCurrentFolderNode();
        if (current && current->parent) {
            m_fileManagerView->SetCurrentFolder(current->parent);
            m_fileManagerView->SetSearchFilter("");
        } else if (m_navHistory.CanGoUp()) {
            QString parentPath = m_navHistory.GetParentPath();
            m_fileManagerView->NavigateToPath(parentPath);
            m_fileManagerView->SetSearchFilter("");
        }
        updateNavigationButtons();
    }

    void MainWindow::onFolderChanged(const QString &fullPath) {
        if (!m_isNavigatingHistory) {
            m_navHistory.NavigateTo(fullPath);
        }
        updateNavigationButtons();
    }

    void MainWindow::updateNavigationButtons() {
        if (m_actBack)
            m_actBack->setEnabled(m_navHistory.CanGoBack());
        if (m_actForward)
            m_actForward->setEnabled(m_navHistory.CanGoForward());
        if (m_actUp) {
            const ArchiveNode *current =
                m_fileManagerView ? m_fileManagerView->GetCurrentFolderNode()
                                  : nullptr;
            bool canUp = (current && current->parent) || m_navHistory.CanGoUp();
            m_actUp->setEnabled(canUp);
        }
    }

    void MainWindow::onShowArchiveInfoAction() {
        if (!m_currentArchive) {
            QMessageBox::information(this, "Archive Properties",
                                     "No active archive loaded.");
            return;
        }

        uint64_t totalUncomp = 0;
        uint64_t totalComp = 0;
        for (const auto &entry : m_currentArchive->GetEntries()) {
            totalUncomp += entry.uncompressed_size;
            totalComp += entry.compressed_size;
        }

        auto stats = Defragmenter::Analyze(m_currentFatPath.toStdString(),
                                           *m_currentArchive);

        auto formatBytes = [](uint64_t bytes) -> QString {
            if (bytes >= 1024ULL * 1024ULL * 1024ULL) {
                return QString("%1 GB (%2 bytes)")
                    .arg(QString::number(static_cast<double>(bytes) /
                                             (1024.0 * 1024.0 * 1024.0),
                                         'f', 2))
                    .arg(bytes);
            } else if (bytes >= 1024ULL * 1024ULL) {
                return QString("%1 MB (%2 bytes)")
                    .arg(QString::number(
                        static_cast<double>(bytes) / (1024.0 * 1024.0), 'f', 2))
                    .arg(bytes);
            } else if (bytes >= 1024ULL) {
                return QString("%1 KB (%2 bytes)")
                    .arg(QString::number(static_cast<double>(bytes) / 1024.0,
                                         'f', 2))
                    .arg(bytes);
            }
            return QString("%1 bytes").arg(bytes);
        };

        QString infoStr =
            QString("<h3>BF Properties</h3>"
                    "<table>"
                    "<tr><td><b>Archive Path:</b></td><td>%1</td></tr>"
                    "<tr><td><b>Format Version:</b></td><td>%2 (v%3)</td></tr>"
                    "<tr><td><b>Target Platform:</b></td><td>%4</td></tr>"
                    "<tr><td><b>Compression Version:</b></td><td>%5</td></tr>"
                    "<tr><td><b>Name Hash Version:</b></td><td>%6</td></tr>"
                    "<tr><td><b>Total File Entries:</b></td><td>%7</td></tr>"
                    "<tr><td><b>Total Uncompressed:</b></td><td>%8 MB (%9 "
                    "bytes)</td></tr>"
                    "<tr><td><b>Total Compressed:</b></td><td>%10 MB (%11 "
                    "bytes)</td></tr>"
                    "<tr><td><b>DAT File Size:</b></td><td>%12</td></tr>"
                    "<tr><td><b>Reclaimable Space:</b></td><td>%13</td></tr>"
                    "<tr><td><b>Fragmentation:</b></td><td>%14%</td></tr>"
                    "<tr><td><b>Active Project:</b></td><td>%15</td></tr>"
                    "</table>")
                .arg(m_currentFatPath)
                .arg(QString::fromStdString(std::string(
                    VersionToString(m_currentArchive->GetVersion()))))
                .arg(m_currentArchive->GetRawVersion())
                .arg(QString::fromStdString(std::string(
                    PlatformToString(m_currentArchive->GetPlatform()))))
                .arg(m_currentArchive->GetCompressionVersion())
                .arg(m_currentArchive->GetNameHashVersion())
                .arg(m_currentArchive->GetEntries().size())
                .arg(static_cast<double>(totalUncomp) / (1024.0 * 1024.0), 0,
                     'f', 2)
                .arg(totalUncomp)
                .arg(static_cast<double>(totalComp) / (1024.0 * 1024.0), 0, 'f',
                     2)
                .arg(totalComp)
                .arg(formatBytes(stats.currentDatSize))
                .arg(formatBytes(stats.reclaimableBytes))
                .arg(QString::number(stats.fragmentationPercent, 'f', 2))
                .arg(m_currentProjectName.isEmpty() ? "None"
                                                    : m_currentProjectName);

        QMessageBox::information(this, "Archive Properties", infoStr);
    }

    void MainWindow::onExtractAllAction() {
        if (!m_currentArchive || m_currentArchive->GetEntries().empty()) {
            QMessageBox::information(
                this, "Extract All",
                "No active archive loaded or archive is empty.");
            return;
        }

        QString destDir = QFileDialog::getExistingDirectory(
            this, "Select Directory for Extracting All Files");
        if (destDir.isEmpty())
            return;

        std::vector<const ArchiveEntry *> entries;
        for (const auto &entry : m_currentArchive->GetEntries()) {
            entries.push_back(&entry);
        }

        performExtraction(entries, destDir);
    }

    void MainWindow::onExtractSelectedAction() {
        if (!m_currentArchive || !m_fileManagerView)
            return;

        auto selectedNodes = m_fileManagerView->GetSelectedNodes();
        if (!selectedNodes.empty()) {
            onRequestExtractNodes(selectedNodes);
        } else {
            const ArchiveNode *selected = m_fileManagerView->GetSelectedNode();
            if (selected) {
                onRequestExtractNode(selected);
            } else {
                QMessageBox::information(
                    this, "Extract Selected",
                    "Please select an item in the file manager view first.");
            }
        }
    }

    void MainWindow::onRequestExtractNode(const ArchiveNode *node) {
        if (!node)
            return;
        onRequestExtractNodes({node});
    }

    void MainWindow::onRequestExtractNodes(
        const std::vector<const ArchiveNode *> &nodes) {
        if (nodes.empty() || !m_currentArchive || m_currentFatPath.isEmpty())
            return;

        QString destDir = QFileDialog::getExistingDirectory(
            this, "Select Directory to Extract Items");
        if (destDir.isEmpty())
            return;

        std::filesystem::path destDirPath = destDir.toStdString();
        std::filesystem::path datPath = m_currentFatPath.toStdString();
        datPath.replace_extension(".dat");

        std::error_code ec;
        if (!std::filesystem::exists(datPath, ec)) {
            QMessageBox::critical(
                this, tr("Extraction Error"),
                QString("Could not open DAT payload stream at:\n%1")
                    .arg(QString::fromStdString(datPath.string())));
            return;
        }

        std::vector<std::pair<const ArchiveEntry *, std::filesystem::path>>
            tasks;

        for (const auto *node : nodes) {
            if (!node)
                continue;
            if (node->isDirectory) {
                std::string folderName = node->name;
                if (folderName.empty() || folderName == "/")
                    folderName = "archive_root";
                std::filesystem::path folderOutDir = destDirPath / folderName;
                std::string nodePrefix = NormalizePath(node->fullPath);

                std::function<void(const ArchiveNode *)> collectDir =
                    [&](const ArchiveNode *n) {
                        if (!n)
                            return;
                        if (n->entryRef) {
                            std::string entryNorm =
                                NormalizePath(n->entryRef->path);
                            std::string relPath = entryNorm;
                            if (!nodePrefix.empty() && nodePrefix != "/" &&
                                entryNorm.starts_with(nodePrefix + "/")) {
                                relPath =
                                    entryNorm.substr(nodePrefix.length() + 1);
                            }
                            tasks.push_back(
                                {n->entryRef, folderOutDir / relPath});
                        }
                        for (const auto &child : n->children) {
                            collectDir(child.get());
                        }
                    };
                collectDir(node);
            } else if (node->entryRef) {
                tasks.push_back({node->entryRef, destDirPath / node->name});
            }
        }

        if (tasks.empty())
            return;

        auto *progressDialog = new Dialogs::ExtractProgressDialog(this);
        progressDialog->setWindowModality(Qt::WindowModal);
        progressDialog->show();

        auto cancelFlag = std::make_shared<std::atomic<bool>>(false);
        connect(progressDialog, &QDialog::rejected, this,
                [cancelFlag]() { cancelFlag->store(true); });

        size_t totalEntries = tasks.size();

        std::thread workerThread([this, progressDialog, cancelFlag, tasks,
                                  datPath, totalEntries, destDir]() {
            ThreadPool pool(ThreadPool::DefaultThreadCount());
            std::atomic<size_t> currentCount{0};
            std::mutex progressMutex;
            std::vector<std::future<bool>> futures;
            futures.reserve(tasks.size());

            for (const auto &[entry, outputPath] : tasks) {
                futures.push_back(pool.Enqueue(
                    [entry, outputPath, &datPath, &currentCount, totalEntries,
                     &progressMutex, progressDialog, cancelFlag]() -> bool {
                        if (cancelFlag && cancelFlag->load())
                            return false;

                        std::ifstream datStream(datPath, std::ios::binary);
                        if (!datStream.is_open())
                            return false;

                        std::error_code ec;
                        std::filesystem::create_directories(
                            outputPath.parent_path(), ec);

                        bool success = Extractor::ExtractEntry(
                            *entry, datStream, outputPath);
                        size_t c = ++currentCount;
                        {
                            std::lock_guard<std::mutex> lock(progressMutex);
                            QMetaObject::invokeMethod(
                                progressDialog,
                                [progressDialog, c, totalEntries,
                                 fileName = outputPath.filename().string()]() {
                                    progressDialog->SetStatus(
                                        static_cast<int>(c),
                                        static_cast<int>(totalEntries),
                                        QString::fromStdString(fileName));
                                },
                                Qt::QueuedConnection);
                        }
                        return success;
                    }));
            }

            bool allSuccess = true;
            for (auto &f : futures) {
                if (!f.get())
                    allSuccess = false;
            }

            QMetaObject::invokeMethod(
                this,
                [this, progressDialog, allSuccess, totalEntries, destDir,
                 cancelFlag]() {
                    bool cancelled = cancelFlag->load();
                    if (cancelled) {
                        progressDialog->reject();
                    } else {
                        progressDialog->accept();
                    }
                    progressDialog->deleteLater();

                    if (cancelled) {
                        Logger::Warn("BFView", "Extraction cancelled by user.");
                        statusBar()->showMessage(tr("Extraction cancelled."),
                                                 5000);
                    } else if (allSuccess) {
                        Logger::Info("BFView",
                                     "Successfully extracted {} items to '{}'",
                                     totalEntries, destDir.toStdString());
                        statusBar()->showMessage(
                            QString("Successfully extracted %1 items to %2")
                                .arg(totalEntries)
                                .arg(destDir),
                            5000);
                    } else {
                        Logger::Error("BFView",
                                      "Extraction finished with some errors.");
                        statusBar()->showMessage(
                            QString("Extracted with some errors to %1")
                                .arg(destDir),
                            5000);
                    }
                },
                Qt::QueuedConnection);
        });
        workerThread.detach();
    }

    void MainWindow::performExtraction(
        const std::vector<const ArchiveEntry *> &entriesToExtract,
        const QString &destinationDir) {
        if (entriesToExtract.empty() || m_currentFatPath.isEmpty())
            return;

        std::filesystem::path datPath = m_currentFatPath.toStdString();
        datPath.replace_extension(".dat");

        std::error_code ec;
        if (!std::filesystem::exists(datPath, ec)) {
            QMessageBox::critical(
                this, tr("Extraction Error"),
                QString("Could not open DAT payload stream at:\n%1")
                    .arg(QString::fromStdString(datPath.string())));
            return;
        }

        auto *progressDialog = new Dialogs::ExtractProgressDialog(this);
        progressDialog->setWindowModality(Qt::WindowModal);
        progressDialog->show();

        auto cancelFlag = std::make_shared<std::atomic<bool>>(false);
        connect(progressDialog, &QDialog::rejected, this,
                [cancelFlag]() { cancelFlag->store(true); });

        size_t totalEntries = entriesToExtract.size();
        std::filesystem::path destDirPath = destinationDir.toStdString();

        std::thread workerThread([this, progressDialog, cancelFlag,
                                  entriesToExtract, datPath, destDirPath,
                                  totalEntries, destinationDir]() {
            bool allSuccess = Extractor::ExtractEntries(
                entriesToExtract, datPath, destDirPath,
                [progressDialog, cancelFlag](size_t current, size_t total,
                                             const std::string &currentFile) {
                    if (cancelFlag->load())
                        return;
                    QMetaObject::invokeMethod(
                        progressDialog,
                        [progressDialog, current, total,
                         fileStr = QString::fromStdString(currentFile)]() {
                            progressDialog->SetStatus(static_cast<int>(current),
                                                      static_cast<int>(total),
                                                      fileStr);
                        },
                        Qt::QueuedConnection);
                },
                0, // all threads
                cancelFlag.get());

            QMetaObject::invokeMethod(
                this,
                [this, progressDialog, allSuccess, totalEntries, destinationDir,
                 cancelFlag]() {
                    bool cancelled = cancelFlag->load();
                    if (cancelled) {
                        progressDialog->reject();
                    } else {
                        progressDialog->accept();
                    }
                    progressDialog->deleteLater();

                    if (cancelled) {
                        Logger::Warn("BFView", "Extraction cancelled by user.");
                        statusBar()->showMessage(tr("Extraction cancelled."),
                                                 5000);
                    } else if (allSuccess) {
                        Logger::Info(
                            "BFView", "Successfully extracted {} items to '{}'",
                            totalEntries, destinationDir.toStdString());
                        statusBar()->showMessage(
                            QString("Successfully extracted %1 items to %2")
                                .arg(totalEntries)
                                .arg(destinationDir),
                            5000);
                    } else {
                        Logger::Error("BFView",
                                      "Extraction finished with some errors.");
                        statusBar()->showMessage(
                            QString("Extracted with some errors to %2")
                                .arg(destinationDir),
                            5000);
                    }
                },
                Qt::QueuedConnection);
        });
        workerThread.detach();
    }

    void MainWindow::onRequestDragOut(const ArchiveNode *node) {
        if (!node)
            return;
        onRequestDragOutNodes({node});
    }

    void MainWindow::onRequestDragOutNodes(
        const std::vector<const ArchiveNode *> &nodes) {
        if (nodes.empty() || !m_currentArchive || m_currentFatPath.isEmpty())
            return;

        std::filesystem::path datPath = m_currentFatPath.toStdString();
        datPath.replace_extension(".dat");

        std::ifstream datStream(datPath, std::ios::binary);
        if (!datStream.is_open())
            return;

        QString tempSessionDir =
            QDir::tempPath() +
            QString("/bfview_drag_%1").arg(QDateTime::currentMSecsSinceEpoch());
        QDir().mkpath(tempSessionDir);
        std::filesystem::path sessionDirPath = tempSessionDir.toStdString();

        QList<QUrl> urls;

        for (const auto *node : nodes) {
            if (!node)
                continue;
            if (node->isDirectory) {
                std::string folderName = node->name;
                if (folderName.empty() || folderName == "/")
                    folderName = "archive_root";
                std::filesystem::path folderOutDir =
                    sessionDirPath / folderName;
                std::filesystem::create_directories(folderOutDir);
                std::string nodePrefix = NormalizePath(node->fullPath);

                std::function<void(const ArchiveNode *)> collectAndExtract =
                    [&](const ArchiveNode *n) {
                        if (!n)
                            return;
                        if (n->entryRef) {
                            std::string entryNorm =
                                NormalizePath(n->entryRef->path);
                            std::string relPath = entryNorm;
                            if (!nodePrefix.empty() && nodePrefix != "/" &&
                                entryNorm.starts_with(nodePrefix + "/")) {
                                relPath =
                                    entryNorm.substr(nodePrefix.length() + 1);
                            }
                            std::filesystem::path fileOutPath =
                                folderOutDir / relPath;
                            std::filesystem::create_directories(
                                fileOutPath.parent_path());
                            Extractor::ExtractEntry(*n->entryRef, datStream,
                                                    fileOutPath);
                        }
                        for (const auto &child : n->children) {
                            collectAndExtract(child.get());
                        }
                    };
                collectAndExtract(node);
                urls.append(QUrl::fromLocalFile(
                    QString::fromStdString(folderOutDir.string())));
            } else if (node->entryRef) {
                std::filesystem::path fileOutPath = sessionDirPath / node->name;
                Extractor::ExtractEntry(*node->entryRef, datStream,
                                        fileOutPath);
                urls.append(QUrl::fromLocalFile(
                    QString::fromStdString(fileOutPath.string())));
            }
        }

        if (urls.isEmpty())
            return;

        auto *drag = new QDrag(this);
        auto *mimeData = new QMimeData();
        mimeData->setUrls(urls);

        drag->setMimeData(mimeData);
        drag->exec(Qt::CopyAction);
    }

    void MainWindow::onRequestOpenFile(const ArchiveNode *node,
                                       bool bypassCustomHandler,
                                       bool openWithChooser) {
        if (!node || !node->entryRef || m_currentFatPath.isEmpty())
            return;
        FileHandlerManager::GetInstance().OpenFile(
            m_currentFatPath, *node->entryRef, bypassCustomHandler,
            openWithChooser);
    }

    void MainWindow::onSyncGenerateNfoAction() {
        if (!m_currentArchive || m_currentFatPath.isEmpty()) {
            QMessageBox::warning(this, "No Archive Open",
                                 "Please open a .fat archive first.");
            return;
        }

        QFileInfo fatInfo(m_currentFatPath);
        QString nfoPath =
            fatInfo.path() + "/" + fatInfo.completeBaseName() + ".nfo";

        std::ofstream nfoFile(nfoPath.toStdString());
        if (!nfoFile.is_open()) {
            QMessageBox::critical(
                this, "Error",
                QString("Failed to create NFO file at:\n%1").arg(nfoPath));
            return;
        }

        if (m_currentArchive->SerializeNfo(nfoFile)) {
            nfoFile.close();
            QMessageBox::information(
                this, "NFO Synced / Generated",
                QString("Successfully generated/synced NFO file at:\n%1")
                    .arg(nfoPath));
            statusBar()->showMessage("NFO file updated.");
        } else {
            QMessageBox::critical(this, "Error",
                                  "Failed to serialize NFO data.");
        }
    }

    void MainWindow::onConfigureSettingsAction() {
        SettingsDialog dialog(this);
        dialog.exec();
    }

    void MainWindow::onDeleteSelectedAction() {
        if (!m_fileManagerView)
            return;
        auto selectedNodes = m_fileManagerView->GetSelectedNodes();
        if (!selectedNodes.empty()) {
            onRequestDeleteNodes(selectedNodes);
        } else {
            const ArchiveNode *selected = m_fileManagerView->GetSelectedNode();
            if (selected) {
                onRequestDeleteNode(selected);
            }
        }
    }

    void MainWindow::onRequestDeleteNode(const ArchiveNode *node) {
        if (!node)
            return;
        onRequestDeleteNodes({node});
    }

    void MainWindow::onRequestDeleteNodes(
        const std::vector<const ArchiveNode *> &nodes) {
        if (nodes.empty() || !m_currentArchive)
            return;

        if (SettingsManager::GetInstance().GetConfirmOnDelete()) {
            QString itemDesc;
            if (nodes.size() == 1) {
                itemDesc = QString("'%1' (%2)")
                               .arg(QString::fromStdString(nodes[0]->name))
                               .arg(nodes[0]->isDirectory ? "Folder" : "File");
            } else {
                itemDesc = QString("%1 selected items").arg(nodes.size());
            }

            QMessageBox::StandardButton reply = QMessageBox::question(
                this, "Confirm Deletion",
                QString("Are you sure you want to delete %1 from the archive?")
                    .arg(itemDesc),
                QMessageBox::Yes | QMessageBox::No);

            if (reply != QMessageBox::Yes)
                return;
        }

        // Collect hashes to remove
        std::vector<uint64_t> hashesToRemove;
        std::function<void(const ArchiveNode *)> collectHashes =
            [&](const ArchiveNode *n) {
                if (!n)
                    return;
                if (n->entryRef) {
                    hashesToRemove.push_back(n->entryRef->name_hash);
                }
                for (const auto &child : n->children) {
                    collectHashes(child.get());
                }
            };
        for (const auto *n : nodes) {
            collectHashes(n);
        }

        if (hashesToRemove.empty())
            return;

        auto &entries = m_currentArchive->GetEntries();
        entries.erase(std::remove_if(entries.begin(), entries.end(),
                                     [&](const ArchiveEntry &e) {
                                         return std::find(
                                                    hashesToRemove.begin(),
                                                    hashesToRemove.end(),
                                                    e.name_hash) !=
                                                hashesToRemove.end();
                                     }),
                      entries.end());

        saveArchive();

        auto root = DirectoryTreeBuilder::Build(m_currentArchive->GetEntries());
        m_fileManagerView->SetArchiveRoot(root);

        statusBar()->showMessage(QString("Deleted %1 item(s) from archive (run "
                                         "Defrag to reclaim DAT space).")
                                     .arg(nodes.size()),
                                 5000);
    }

    void MainWindow::onAddFileAction() {
        if (!m_currentArchive || m_currentFatPath.isEmpty()) {
            QMessageBox::information(
                this, "Add File",
                "Please open an archive first before adding files.");
            return;
        }

        QString srcFile =
            QFileDialog::getOpenFileName(this, "Select File to Add to Archive");
        if (srcFile.isEmpty())
            return;

        const ArchiveNode *currentFolder =
            m_fileManagerView->GetCurrentFolderNode();
        std::string currentRel =
            (currentFolder && currentFolder->fullPath != "/")
                ? NormalizePath(currentFolder->fullPath)
                : "";
        std::string defaultTarget =
            currentRel.empty() ? QFileInfo(srcFile).fileName().toStdString()
                               : (currentRel + "/" +
                                  QFileInfo(srcFile).fileName().toStdString());

        bool ok = false;
        QString archiveRelPath = QInputDialog::getText(
            this, "Archive Target Relative Path",
            "Enter relative path inside archive (e.g. "
            "generated/db/my_file.xml):",
            QLineEdit::Normal, QString::fromStdString(defaultTarget), &ok);

        if (!ok || archiveRelPath.trimmed().isEmpty())
            return;
        std::string cleanRelPath =
            NormalizePath(archiveRelPath.trimmed().toStdString());

        if (SettingsManager::GetInstance().GetConfirmOnAdd()) {
            QMessageBox::StandardButton reply = QMessageBox::question(
                this, "Confirm Add File",
                QString("Are you sure you want to add '%1' to "
                        "the archive at path:\n'%2'?")
                    .arg(QFileInfo(srcFile).fileName())
                    .arg(QString::fromStdString(cleanRelPath)),
                QMessageBox::Yes | QMessageBox::No);

            if (reply != QMessageBox::Yes)
                return;
        }

        CompressionScheme scheme =
            SettingsManager::GetInstance().GetDefaultCompression(
                m_currentArchive->GetVersion(),
                m_currentArchive->GetRawVersion());
        std::string errorMsg;
        bool success = ArchiveModifier::AppendFile(
            m_currentFatPath.toStdString(), *m_currentArchive, cleanRelPath,
            srcFile.toStdString(), scheme, errorMsg);

        if (!success) {
            QMessageBox::critical(
                this, "Add File Error",
                QString("Failed to append file to archive DAT:\n%1")
                    .arg(QString::fromStdString(errorMsg)));
            return;
        }

        auto root = DirectoryTreeBuilder::Build(m_currentArchive->GetEntries());
        m_fileManagerView->SetArchiveRoot(root);

        QMessageBox::information(
            this, "File Added",
            QString("File '%1' appended to archive DAT and "
                    "FAT/NFO metadata updated successfully.")
                .arg(QString::fromStdString(cleanRelPath)));
    }

    void MainWindow::onAddFolderAction() {
        if (!m_currentArchive || m_currentFatPath.isEmpty()) {
            QMessageBox::information(
                this, "Add Folder",
                "Please open an archive first before adding folders.");
            return;
        }

        QString srcFolder = QFileDialog::getExistingDirectory(
            this, "Select Folder to Add to Archive");
        if (srcFolder.isEmpty())
            return;

        std::filesystem::path folderPath = srcFolder.toStdString();
        std::string folderName = folderPath.filename().string();
        if (folderName.empty())
            folderName = folderPath.parent_path().filename().string();

        const ArchiveNode *currentFolder =
            m_fileManagerView->GetCurrentFolderNode();
        std::string currentRel =
            (currentFolder && currentFolder->fullPath != "/")
                ? NormalizePath(currentFolder->fullPath)
                : "";
        std::string defaultTarget =
            currentRel.empty() ? folderName : (currentRel + "/" + folderName);

        bool ok = false;
        QString archiveTargetRelPath = QInputDialog::getText(
            this, "Archive Target Relative Path",
            "Enter target folder path inside archive:", QLineEdit::Normal,
            QString::fromStdString(defaultTarget), &ok);

        if (!ok || archiveTargetRelPath.trimmed().isEmpty())
            return;

        std::string targetBase =
            NormalizePath(archiveTargetRelPath.trimmed().toStdString());
        if (!targetBase.empty() && !targetBase.ends_with('/'))
            targetBase += "/";

        std::vector<FileToAdd> filesToAdd;
        std::error_code ec;
        for (const auto &dirEntry :
             std::filesystem::recursive_directory_iterator(folderPath, ec)) {
            if (dirEntry.is_regular_file()) {
                auto subRel =
                    std::filesystem::relative(dirEntry.path(), folderPath);
                std::string relPath = targetBase + subRel.generic_string();
                filesToAdd.push_back({NormalizePath(relPath), dirEntry.path()});
            }
        }

        if (filesToAdd.empty()) {
            QMessageBox::warning(
                this, "Empty Folder",
                "Selected folder does not contain any regular files.");
            return;
        }

        if (SettingsManager::GetInstance().GetConfirmOnAdd()) {
            QMessageBox::StandardButton reply = QMessageBox::question(
                this, "Confirm Add Folder",
                QString("Are you sure you want to add folder '%1' (%2 file(s)) "
                        "to the "
                        "archive at path:\n'%3'?")
                    .arg(QString::fromStdString(folderName))
                    .arg(filesToAdd.size())
                    .arg(QString::fromStdString(targetBase)),
                QMessageBox::Yes | QMessageBox::No);

            if (reply != QMessageBox::Yes)
                return;
        }

        CompressionScheme scheme =
            SettingsManager::GetInstance().GetDefaultCompression(
                m_currentArchive->GetVersion(),
                m_currentArchive->GetRawVersion());
        std::string errorMsg;

        auto *progressDialog =
            new QProgressDialog("Adding files to archive...", "Cancel", 0,
                                static_cast<int>(filesToAdd.size()), this);
        progressDialog->setWindowModality(Qt::WindowModal);
        progressDialog->show();

        bool success = ArchiveModifier::AppendFiles(
            m_currentFatPath.toStdString(), *m_currentArchive, filesToAdd,
            scheme, errorMsg, [&](size_t current, size_t total) {
                (void)total;
                QMetaObject::invokeMethod(
                    progressDialog,
                    [progressDialog, current]() {
                        progressDialog->setValue(static_cast<int>(current));
                    },
                    Qt::QueuedConnection);
            });
        progressDialog->deleteLater();

        if (!success) {
            QMessageBox::critical(
                this, "Add Folder Error",
                QString("Failed to add folder to archive DAT:\n%1")
                    .arg(QString::fromStdString(errorMsg)));
            return;
        }

        auto root = DirectoryTreeBuilder::Build(m_currentArchive->GetEntries());
        m_fileManagerView->SetArchiveRoot(root);

        QMessageBox::information(
            this, "Folder Added",
            QString("Successfully added folder '%1' (%2 files) to archive.")
                .arg(QString::fromStdString(folderName))
                .arg(filesToAdd.size()));
    }

    void MainWindow::onNewFolderAction() {
        if (!m_currentArchive || m_currentFatPath.isEmpty()) {
            QMessageBox::information(
                this, "New Folder",
                "Please open an archive first before creating folders.");
            return;
        }

        const ArchiveNode *currentFolder =
            m_fileManagerView->GetCurrentFolderNode();
        std::string currentRel =
            (currentFolder && currentFolder->fullPath != "/")
                ? NormalizePath(currentFolder->fullPath)
                : "";

        bool ok = false;
        QString folderName = QInputDialog::getText(
            this, "New Folder", "Enter folder name:", QLineEdit::Normal,
            "New Folder", &ok);

        if (!ok || folderName.trimmed().isEmpty())
            return;
        std::string cleanFolderName = folderName.trimmed().toStdString();
        std::string newFolderPath = currentRel.empty()
                                        ? cleanFolderName
                                        : (currentRel + "/" + cleanFolderName);
        std::string placeholderPath = newFolderPath + "/.keep";

        std::string tempDir =
            QDir::tempPath().toStdString() + "/bfview_placeholder_" +
            std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count());
        std::filesystem::create_directories(tempDir);
        std::filesystem::path keepFile =
            std::filesystem::path(tempDir) / ".keep";
        {
            std::ofstream out(keepFile, std::ios::binary);
        }

        std::string errorMsg;
        bool success = ArchiveModifier::AppendFile(
            m_currentFatPath.toStdString(), *m_currentArchive, placeholderPath,
            keepFile, CompressionScheme::None, errorMsg);
        std::filesystem::remove_all(tempDir);

        if (!success) {
            QMessageBox::critical(
                this, "New Folder Error",
                QString("Failed to create folder in archive:\n%1")
                    .arg(QString::fromStdString(errorMsg)));
            return;
        }

        auto root = DirectoryTreeBuilder::Build(m_currentArchive->GetEntries());
        m_fileManagerView->SetArchiveRoot(root);
        m_fileManagerView->NavigateToPath(
            QString::fromStdString(newFolderPath));

        statusBar()->showMessage(
            QString("Created folder '%1' in archive.")
                .arg(QString::fromStdString(newFolderPath)),
            5000);
    }

    void MainWindow::onFilesDropped(const QStringList &filePaths,
                                    const QString &targetFolderRelPath) {
        if (!m_currentArchive || m_currentFatPath.isEmpty() ||
            filePaths.isEmpty())
            return;

        std::string baseTarget =
            (targetFolderRelPath == "/" || targetFolderRelPath.isEmpty())
                ? ""
                : NormalizePath(targetFolderRelPath.toStdString());
        if (!baseTarget.empty() && !baseTarget.ends_with('/'))
            baseTarget += "/";

        std::vector<FileToAdd> filesToAdd;
        for (const QString &srcItem : filePaths) {
            std::filesystem::path p = srcItem.toStdString();
            std::error_code ec;
            if (!std::filesystem::exists(p, ec))
                continue;

            if (std::filesystem::is_regular_file(p, ec)) {
                std::string relPath = baseTarget + p.filename().string();
                filesToAdd.push_back({NormalizePath(relPath), p});
            } else if (std::filesystem::is_directory(p, ec)) {
                std::string folderName = p.filename().string();
                if (folderName.empty())
                    folderName = p.parent_path().filename().string();
                for (const auto &dirEntry :
                     std::filesystem::recursive_directory_iterator(p, ec)) {
                    if (dirEntry.is_regular_file()) {
                        auto subRel =
                            std::filesystem::relative(dirEntry.path(), p);
                        std::string relPath = baseTarget + folderName + "/" +
                                              subRel.generic_string();
                        filesToAdd.push_back(
                            {NormalizePath(relPath), dirEntry.path()});
                    }
                }
            }
        }

        if (filesToAdd.empty())
            return;

        if (SettingsManager::GetInstance().GetConfirmOnAdd()) {
            QMessageBox::StandardButton reply = QMessageBox::question(
                this, "Confirm Drop Files & Folders",
                QString("Are you sure you want to add %1 "
                        "file(s) into archive folder '%2'?")
                    .arg(filesToAdd.size())
                    .arg(targetFolderRelPath),
                QMessageBox::Yes | QMessageBox::No);

            if (reply != QMessageBox::Yes)
                return;
        }

        CompressionScheme scheme =
            SettingsManager::GetInstance().GetDefaultCompression(
                m_currentArchive->GetVersion(),
                m_currentArchive->GetRawVersion());
        std::string errorMsg;
        bool success = ArchiveModifier::AppendFiles(
            m_currentFatPath.toStdString(), *m_currentArchive, filesToAdd,
            scheme, errorMsg);

        if (!success) {
            QMessageBox::critical(
                this, "Drop Files Error",
                QString("Failed to append dropped items to archive DAT:\n%1")
                    .arg(QString::fromStdString(errorMsg)));
            return;
        }

        auto root = DirectoryTreeBuilder::Build(m_currentArchive->GetEntries());
        m_fileManagerView->SetArchiveRoot(root);

        statusBar()->showMessage(
            QString("Appended %1 dropped file(s) to archive DAT.")
                .arg(filesToAdd.size()),
            5000);
    }

    void MainWindow::onArchiveFileDropped(const QString &fatPath) {
        if (fatPath.isEmpty())
            return;

        if (!m_currentArchive) {
            OpenArchiveFile(fatPath);
            return;
        }

        QMessageBox msgBox(this);
        msgBox.setWindowTitle("Open Archive");
        msgBox.setText("An archive is currently loaded.");
        msgBox.setInformativeText(
            QString("What would you like to do with '%1'?")
                .arg(QFileInfo(fatPath).fileName()));

        QPushButton *loadButton =
            msgBox.addButton("Load Archive", QMessageBox::AcceptRole);
        QPushButton *addButton =
            msgBox.addButton("Add to Current Archive", QMessageBox::ActionRole);
        QPushButton *cancelButton =
            msgBox.addButton("Cancel", QMessageBox::RejectRole);

        msgBox.setDefaultButton(loadButton);
        msgBox.exec();

        if (msgBox.clickedButton() == loadButton) {
            OpenArchiveFile(fatPath);
        } else if (msgBox.clickedButton() == addButton) {
            std::error_code ec;
            auto incomingArchive = ArchiveFactory::Open(
                fatPath.toStdString(), ec, m_currentProjectName.toStdString());
            if (!incomingArchive || ec) {
                QMessageBox::critical(
                    this, "Error Opening Archive",
                    QString("Failed to open dropped archive:\n%1")
                        .arg(fatPath));
                return;
            }

            size_t mergedCount = 0;
            std::string errorMsg;
            bool success = ArchiveModifier::MergeArchive(
                m_currentFatPath.toStdString(), *m_currentArchive,
                fatPath.toStdString(), *incomingArchive, mergedCount, errorMsg);

            if (!success) {
                QMessageBox::critical(
                    this, "Merge Error",
                    QString("Failed to merge archive entries into DAT:\n%1")
                        .arg(QString::fromStdString(errorMsg)));
                return;
            }

            auto root =
                DirectoryTreeBuilder::Build(m_currentArchive->GetEntries());
            m_fileManagerView->SetArchiveRoot(root);

            QMessageBox::information(
                this, "Archive Merged",
                QString("Successfully merged %1 entries from '%2' "
                        "into the current archive DAT.")
                    .arg(mergedCount)
                    .arg(QFileInfo(fatPath).fileName()));
        }
    }

    void MainWindow::onDefragmentAction() {
        if (!m_currentArchive || m_currentFatPath.isEmpty()) {
            QMessageBox::information(
                this, "Defragment Archive",
                "Please open an archive first before defragmenting.");
            return;
        }

        CompressionScheme scheme =
            SettingsManager::GetInstance().GetDefaultCompression(
                m_currentArchive->GetVersion(),
                m_currentArchive->GetRawVersion());
        auto stats = Defragmenter::Analyze(m_currentFatPath.toStdString(),
                                           *m_currentArchive, scheme);

        auto formatBytes = [](uint64_t bytes) -> QString {
            if (bytes >= 1024ULL * 1024ULL * 1024ULL) {
                return QString("%1 GB (%2 bytes)")
                    .arg(QString::number(static_cast<double>(bytes) /
                                             (1024.0 * 1024.0 * 1024.0),
                                         'f', 2))
                    .arg(bytes);
            } else if (bytes >= 1024ULL * 1024ULL) {
                return QString("%1 MB (%2 bytes)")
                    .arg(QString::number(
                        static_cast<double>(bytes) / (1024.0 * 1024.0), 'f', 2))
                    .arg(bytes);
            } else if (bytes >= 1024ULL) {
                return QString("%1 KB (%2 bytes)")
                    .arg(QString::number(static_cast<double>(bytes) / 1024.0,
                                         'f', 2))
                    .arg(bytes);
            }
            return QString("%1 bytes").arg(bytes);
        };

        auto schemeName = QString::fromUtf8(CompressionToString(scheme).data(),
                                            CompressionToString(scheme).size());

        QString statsText =
            QString("<b>Current DAT Size:</b> %1<br>"
                    "<b>Active Payload Size:</b> %2<br>"
                    "<b>Estimated Compact Size:</b> %3<br>"
                    "<b>Reclaimable Space:</b> %4<br>"
                    "<b>Fragmentation:</b> %5%<br>"
                    "<b>Total File Entries:</b> %6<br>"
                    "<b>Target Compression:</b> %7")
                .arg(formatBytes(stats.currentDatSize))
                .arg(formatBytes(stats.activePayloadSize))
                .arg(formatBytes(stats.estimatedNewDatSize))
                .arg(formatBytes(stats.reclaimableBytes))
                .arg(QString::number(stats.fragmentationPercent, 'f', 2))
                .arg(stats.entryCount)
                .arg(schemeName);

        if (stats.reclaimableBytes <= 0) {
            QMessageBox msg(this);
            msg.setWindowTitle("Defragment Archive");
            msg.setIcon(QMessageBox::Question);
            msg.setText("<b>The archive has no detected dead space.</b>");
            msg.setInformativeText(
                statsText +
                "<br><br>Would you like to run defragmentation "
                "anyway to ensure files are compressed with <b>" +
                schemeName + "</b>?");
            QPushButton *defragBtn =
                msg.addButton("Defragment / Compress", QMessageBox::AcceptRole);
            msg.addButton("Cancel", QMessageBox::RejectRole);
            msg.exec();
            if (msg.clickedButton() != defragBtn) {
                return;
            }
        } else {
            QMessageBox msgBox(this);
            msgBox.setWindowTitle("Defragment Archive");
            msgBox.setIcon(QMessageBox::Question);
            msgBox.setText("<b>Defragment Archive to Reclaim Space?</b>");
            msgBox.setInformativeText(
                statsText +
                "<br><br>"
                "Defragmenting will rebuild the DAT payload file "
                "sequentially, compress eligible files (> 31 bytes), "
                "and update FAT offsets.<br><br>"
                "Would you like to defragment now?");
            QPushButton *defragBtn =
                msgBox.addButton("Defragment Now", QMessageBox::AcceptRole);
            msgBox.addButton("Cancel", QMessageBox::RejectRole);
            msgBox.setDefaultButton(defragBtn);
            msgBox.exec();

            if (msgBox.clickedButton() != defragBtn) {
                return;
            }
        }

        auto *progress = new Dialogs::DefragProgressDialog(this);
        progress->setWindowModality(Qt::WindowModal);
        progress->show();

        auto cancelFlag = progress->GetCancelFlag();

        Logger::Info("BFView",
                     "Spawning background defragmentation thread for '{}'...",
                     m_currentFatPath.toStdString());

        std::thread workerThread([this, progress, cancelFlag, scheme, stats,
                                  formatBytes,
                                  fatPath = m_currentFatPath.toStdString()]() {
            std::string errorMsg;
            bool success = Defragmenter::Defragment(
                fatPath, *m_currentArchive, errorMsg,
                [progress, cancelFlag](size_t current, size_t total) {
                    if (cancelFlag->load())
                        return;
                    QMetaObject::invokeMethod(
                        progress,
                        [progress, current, total]() {
                            progress->SetStatus(static_cast<int>(current),
                                                static_cast<int>(total));
                        },
                        Qt::QueuedConnection);
                },
                scheme,
                0, // all threads
                cancelFlag.get());

            Logger::Info("BFView",
                         "Background defragmentation thread finished: "
                         "success={}, cancel={}",
                         success, cancelFlag->load());

            QMetaObject::invokeMethod(
                this,
                [this, progress, success, errorMsg, stats, formatBytes,
                 cancelFlag]() {
                    bool cancelled = cancelFlag->load();
                    if (cancelled) {
                        progress->reject();
                    } else {
                        progress->accept();
                    }
                    progress->deleteLater();

                    if (cancelled) {
                        Logger::Warn("BFView",
                                     "Defragmentation was cancelled by user.");
                        statusBar()->showMessage(
                            tr("Defragmentation cancelled."), 5000);
                        return;
                    }

                    if (!success) {
                        Logger::Error("BFView",
                                      "Defragmentation failed with error: {}",
                                      errorMsg);
                        QMessageBox::critical(
                            this, tr("Defragmentation Error"),
                            QString("Defragmentation failed:\n%1")
                                .arg(QString::fromStdString(errorMsg)));
                        return;
                    }

                    Logger::Info(
                        "BFView",
                        "Updating file manager tree after defragmentation...");
                    auto root = DirectoryTreeBuilder::Build(
                        m_currentArchive->GetEntries());
                    m_fileManagerView->SetArchiveRoot(root);

                    statusBar()->showMessage(
                        QString("Defragmentation complete. Reclaimed %1.")
                            .arg(formatBytes(stats.reclaimableBytes)),
                        5000);

                    QMessageBox::information(
                        this, tr("Defragmentation Complete"),
                        QString("Successfully defragmented "
                                "archive!\n\nReclaimed: %1 of "
                                "wasted space.")
                            .arg(formatBytes(stats.reclaimableBytes)));
                },
                Qt::QueuedConnection);
        });
        workerThread.detach();
    }

    void MainWindow::saveArchive() {
        if (!m_currentArchive || m_currentFatPath.isEmpty())
            return;

        std::string errorMsg;
        if (!ArchiveModifier::SaveMetadata(m_currentFatPath.toStdString(),
                                           *m_currentArchive, errorMsg)) {
            QMessageBox::critical(this, tr("Save Archive Failed"),
                                  tr("Failed to save archive metadata:\n%1")
                                      .arg(QString::fromStdString(errorMsg)));
        }
    }

    void MainWindow::updateWindowTitle() {
        if (m_currentArchive && !m_currentFatPath.isEmpty()) {
            QString archiveName = QFileInfo(m_currentFatPath).fileName();
            QString projName = m_currentProjectName.isEmpty()
                                   ? tr("No Project")
                                   : m_currentProjectName;
            setWindowTitle(QString("%1 [%2] - BFView (BFTools)")
                               .arg(archiveName, projName));
        } else {
            setWindowTitle("BFView (NexusTools)");
        }
    }

    void MainWindow::closeEvent(QCloseEvent *event) {
        SettingsManager::GetInstance().SetGeometry(saveGeometry());
        SettingsManager::GetInstance().Save();
        QMainWindow::closeEvent(event);
    }

} // namespace BFView
