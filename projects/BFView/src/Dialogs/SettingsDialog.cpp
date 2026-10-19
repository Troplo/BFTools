//
// Created by troplo on 09/21/26.
//

#include "BFView/Dialogs/SettingsDialog.hpp"
#include "BFView/Platform/FileAssociation.hpp"
#include "BFView/Settings/SettingsManager.hpp"
#include "BFView/Theme/ThemeManager.hpp"
#include "Nomad/Archives/ProjectManager.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace BFView {
    SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent) {
        setWindowTitle(tr("Settings"));
        setMinimumWidth(520);

        auto* mainLayout = new QVBoxLayout(this);

        // Appearance Group
        {
            auto appGroup = new QGroupBox(tr("Appearance"), this);
            auto appLayout = new QFormLayout(appGroup);

            m_themeCombo = new QComboBox(this);
            m_themeCombo->addItem(tr("Follow System"), "System");
            m_themeCombo->addItem(tr("Breeze Light"), "BreezeLight");
            m_themeCombo->addItem(tr("Breeze Dark"), "BreezeDark");
            appLayout->addRow(tr("Application Theme:"), m_themeCombo);

            mainLayout->addWidget(appGroup);
        }

        // General Behavior Group
        {
            auto genGroup = new QGroupBox(tr("General Behavior"), this);
            auto genLayout = new QVBoxLayout(genGroup);

            m_confirmDeleteCheckBox = new QCheckBox(
                tr("Confirm before deleting files from archive"), this);
            m_confirmAddCheckBox = new QCheckBox(
                tr("Confirm before adding/overwriting files"), this);
            m_customHandlersCheckBox = new QCheckBox(
                tr("Use custom external handlers for opening known "
                   "formats (e.g. XBT converter)"),
                this);
            m_autoRegisterAssocCheckBox = new QCheckBox(
                tr("Check and prompt for archive file association on startup"),
                this);

            genLayout->addWidget(m_confirmDeleteCheckBox);
            genLayout->addWidget(m_confirmAddCheckBox);
            genLayout->addWidget(m_customHandlersCheckBox);
            genLayout->addWidget(m_autoRegisterAssocCheckBox);

            auto assocLayout = new QHBoxLayout();
            m_registerAssocNowBtn =
                new QPushButton(tr("Register Default"), this);
            assocLayout->addWidget(m_registerAssocNowBtn);
            assocLayout->addStretch();
            genLayout->addLayout(assocLayout);

            mainLayout->addWidget(genGroup);
        }

        // Paths Group
        {
            auto pathsGroup = new QGroupBox(tr("Paths and Extraction"), this);
            auto pathsLayout = new QFormLayout(pathsGroup);

            // Temp path
            auto tempLayout = new QHBoxLayout();
            m_tempPathEdit = new QLineEdit(this);
            m_browseTempButton = new QPushButton(tr("Browse..."), this);
            tempLayout->addWidget(m_tempPathEdit);
            tempLayout->addWidget(m_browseTempButton);
            pathsLayout->addRow(tr("Temporary Directory (Preview):"),
                                tempLayout);

            // Default extract path
            auto extractLayout = new QHBoxLayout();
            m_extractPathEdit = new QLineEdit(this);
            m_browseExtractButton = new QPushButton(tr("Browse..."), this);
            extractLayout->addWidget(m_extractPathEdit);
            extractLayout->addWidget(m_browseExtractButton);
            pathsLayout->addRow(tr("Default Extract Location:"), extractLayout);

            mainLayout->addWidget(pathsGroup);
        }

        // Projects Group
        {
            auto projectsGroup =
                new QGroupBox(tr("Project Configuration"), this);
            auto projectsLayout = new QFormLayout(projectsGroup);

            auto projDirLayout = new QHBoxLayout();
            m_projectsDirEdit = new QLineEdit(this);
            m_browseProjectsDirButton = new QPushButton(tr("Browse..."), this);
            projDirLayout->addWidget(m_projectsDirEdit);
            projDirLayout->addWidget(m_browseProjectsDirButton);
            projectsLayout->addRow(tr("Projects Directory:"), projDirLayout);

            m_fatv2_v7ProjectCombo = new QComboBox(this);
            projectsLayout->addRow(tr("Default Project for FATv2 (v7):"),
                                   m_fatv2_v7ProjectCombo);

            m_fatv2_v8ProjectCombo = new QComboBox(this);
            projectsLayout->addRow(tr("Default Project for FATv2 (v8):"),
                                   m_fatv2_v8ProjectCombo);

            m_fatv3_v7ProjectCombo = new QComboBox(this);
            projectsLayout->addRow(tr("Default Project for FATv3 (v7):"),
                                   m_fatv3_v7ProjectCombo);

            m_fatv3_v8ProjectCombo = new QComboBox(this);
            projectsLayout->addRow(tr("Default Project for FATv3 (v8):"),
                                   m_fatv3_v8ProjectCombo);

            m_fatv5_v11ProjectCombo = new QComboBox(this);
            projectsLayout->addRow(tr("Default Project for FATv5 (v11):"),
                                   m_fatv5_v11ProjectCombo);

            m_fatv5_v12ProjectCombo = new QComboBox(this);
            projectsLayout->addRow(tr("Default Project for FATv5 (v12):"),
                                   m_fatv5_v12ProjectCombo);

            m_fatv5_v13ProjectCombo = new QComboBox(this);
            projectsLayout->addRow(tr("Default Project for FATv5 (v13):"),
                                   m_fatv5_v13ProjectCombo);

            mainLayout->addWidget(projectsGroup);
        }

        // Compression Group
        {
            auto compressionGroup =
                new QGroupBox(tr("Default Compression"), this);
            auto compressionLayout = new QFormLayout(compressionGroup);

            auto compressionNoteLabel = new QLabel(
                tr("Note: Only None and XMemCompress are supported for PC "
                   "(WD1). Only LZ4LW are supported for PC (WD2/WDL)."),
                this);
            compressionNoteLabel->setWordWrap(true);
            compressionLayout->addRow(compressionNoteLabel);

            m_fatv2_v7CompressionCombo = new QComboBox(this);
            compressionLayout->addRow(tr("Default Compression for FATv2 (v7):"),
                                      m_fatv2_v7CompressionCombo);

            m_fatv2_v8CompressionCombo = new QComboBox(this);
            compressionLayout->addRow(tr("Default Compression for FATv2 (v8):"),
                                      m_fatv2_v8CompressionCombo);

            m_fatv3_v7CompressionCombo = new QComboBox(this);
            compressionLayout->addRow(tr("Default Compression for FATv3 (v7):"),
                                      m_fatv3_v7CompressionCombo);

            m_fatv3_v8CompressionCombo = new QComboBox(this);
            compressionLayout->addRow(tr("Default Compression for FATv3 (v8):"),
                                      m_fatv3_v8CompressionCombo);

            m_fatv5_v11CompressionCombo = new QComboBox(this);
            compressionLayout->addRow(
                tr("Default Compression for FATv5 (v11):"),
                m_fatv5_v11CompressionCombo);

            m_fatv5_v12CompressionCombo = new QComboBox(this);
            compressionLayout->addRow(
                tr("Default Compression for FATv5 (v12):"),
                m_fatv5_v12CompressionCombo);

            m_fatv5_v13CompressionCombo = new QComboBox(this);
            compressionLayout->addRow(
                tr("Default Compression for FATv5 (v13):"),
                m_fatv5_v13CompressionCombo);

            mainLayout->addWidget(compressionGroup);
        }

        // Button Box
        {
            auto buttonBox = new QDialogButtonBox(
                QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
            connect(buttonBox, &QDialogButtonBox::accepted, this,
                    &SettingsDialog::OnAccept);
            connect(buttonBox, &QDialogButtonBox::rejected, this,
                    &QDialog::reject);
            mainLayout->addWidget(buttonBox);
        }

        // Connections
        connect(m_browseTempButton, &QPushButton::clicked, this,
                &SettingsDialog::OnBrowseTempPath);
        connect(m_browseExtractButton, &QPushButton::clicked, this,
                &SettingsDialog::OnBrowseExtractPath);
        connect(m_browseProjectsDirButton, &QPushButton::clicked, this,
                &SettingsDialog::OnBrowseProjectsDir);
        connect(m_projectsDirEdit, &QLineEdit::editingFinished, this,
                &SettingsDialog::OnProjectsDirChanged);
        connect(m_registerAssocNowBtn, &QPushButton::clicked, this,
                &SettingsDialog::OnRegisterAssociationNow);

        // Initial Load
        const auto& settings = SettingsManager::GetInstance();
        int themeIdx = m_themeCombo->findData(settings.GetAppTheme());
        if (themeIdx >= 0)
            m_themeCombo->setCurrentIndex(themeIdx);

        m_confirmDeleteCheckBox->setChecked(settings.GetConfirmOnDelete());
        m_confirmAddCheckBox->setChecked(settings.GetConfirmOnAdd());
        m_customHandlersCheckBox->setChecked(settings.GetUseCustomHandlers());
        m_autoRegisterAssocCheckBox->setChecked(
            settings.GetCheckAssociationOnLaunch());
        m_tempPathEdit->setText(settings.GetTempExtractPath());
        m_extractPathEdit->setText(settings.GetDefaultExtractPath());
        m_projectsDirEdit->setText(settings.GetCustomProjectsDirectory());
        m_projectsDirEdit->setPlaceholderText(
            QDir::toNativeSeparators(QString::fromStdString(
                Nomad::Archives::ProjectManager::GetInstance()
                    .GetProjectsRoot()
                    .string())));

        PopulateProjectCombos();
        PopulateCompressionCombos();
    }

    void SettingsDialog::PopulateProjectCombos() {
        auto setupCombo = [](QComboBox* combo, const QString& selectedVal,
                             const std::vector<std::string>& projs) {
            combo->clear();
            combo->addItem(tr("Always Ask"), SettingsManager::ALWAYS_ASK);
            combo->addItem(tr("None (Raw Hashes)"),
                           SettingsManager::NONE_PROJECT);
            int selectIdx = 0;
            if (selectedVal == SettingsManager::NONE_PROJECT)
                selectIdx = 1;

            int idx = 2;
            for (const auto& p : projs) {
                QString qp = QString::fromStdString(p);
                combo->addItem(qp, qp);
                if (qp == selectedVal) {
                    selectIdx = idx;
                }
                ++idx;
            }
            combo->setCurrentIndex(selectIdx);
        };

        QString customDir = m_projectsDirEdit->text().trimmed();
        Nomad::Archives::ProjectManager::GetInstance().SetProjectsRoot(
            customDir.toStdString());

        auto projects = Nomad::Archives::ProjectManager::GetInstance()
                            .GetAvailableProjects();
        const auto& settings = SettingsManager::GetInstance();

        setupCombo(m_fatv2_v7ProjectCombo,
                   settings.GetDefaultProject(
                       Nomad::Archives::ArchiveVersion::FATv2, 7),
                   projects);
        setupCombo(m_fatv2_v8ProjectCombo,
                   settings.GetDefaultProject(
                       Nomad::Archives::ArchiveVersion::FATv2, 8),
                   projects);
        setupCombo(m_fatv3_v7ProjectCombo,
                   settings.GetDefaultProject(
                       Nomad::Archives::ArchiveVersion::FATv3, 7),
                   projects);
        setupCombo(m_fatv3_v8ProjectCombo,
                   settings.GetDefaultProject(
                       Nomad::Archives::ArchiveVersion::FATv3, 8),
                   projects);
        setupCombo(m_fatv5_v11ProjectCombo,
                   settings.GetDefaultProject(
                       Nomad::Archives::ArchiveVersion::FATv5, 11),
                   projects);
        setupCombo(m_fatv5_v12ProjectCombo,
                   settings.GetDefaultProject(
                       Nomad::Archives::ArchiveVersion::FATv5, 12),
                   projects);
        setupCombo(m_fatv5_v13ProjectCombo,
                   settings.GetDefaultProject(
                       Nomad::Archives::ArchiveVersion::FATv5, 13),
                   projects);
    }

    void SettingsDialog::PopulateCompressionCombos() {
        auto setupCombo = [](QComboBox* combo,
                             Nomad::Archives::CompressionScheme selected) {
            combo->clear();
            combo->addItem(
                "None",
                static_cast<int>(Nomad::Archives::CompressionScheme::None));
            combo->addItem(
                "LZO1x",
                static_cast<int>(Nomad::Archives::CompressionScheme::LZO1x));
            combo->addItem(
                "XMemCompress",
                static_cast<int>(
                    Nomad::Archives::CompressionScheme::XMemCompress));
            combo->addItem(
                "LZ4LW",
                static_cast<int>(Nomad::Archives::CompressionScheme::LZ4LW));

            int idx = combo->findData(static_cast<int>(selected));
            if (idx >= 0)
                combo->setCurrentIndex(idx);
        };

        const auto& settings = SettingsManager::GetInstance();
        setupCombo(m_fatv2_v7CompressionCombo,
                   settings.GetDefaultCompression(
                       Nomad::Archives::ArchiveVersion::FATv2, 7));
        setupCombo(m_fatv2_v8CompressionCombo,
                   settings.GetDefaultCompression(
                       Nomad::Archives::ArchiveVersion::FATv2, 8));
        setupCombo(m_fatv3_v7CompressionCombo,
                   settings.GetDefaultCompression(
                       Nomad::Archives::ArchiveVersion::FATv3, 7));
        setupCombo(m_fatv3_v8CompressionCombo,
                   settings.GetDefaultCompression(
                       Nomad::Archives::ArchiveVersion::FATv3, 8));
        setupCombo(m_fatv5_v11CompressionCombo,
                   settings.GetDefaultCompression(
                       Nomad::Archives::ArchiveVersion::FATv5, 11));
        setupCombo(m_fatv5_v12CompressionCombo,
                   settings.GetDefaultCompression(
                       Nomad::Archives::ArchiveVersion::FATv5, 12));
        setupCombo(m_fatv5_v13CompressionCombo,
                   settings.GetDefaultCompression(
                       Nomad::Archives::ArchiveVersion::FATv5, 13));
    }

    void SettingsDialog::OnBrowseTempPath() {
        QString dir = QFileDialog::getExistingDirectory(
            this, tr("Select Temporary Directory"), m_tempPathEdit->text());
        if (!dir.isEmpty()) {
            m_tempPathEdit->setText(dir);
        }
    }

    void SettingsDialog::OnBrowseExtractPath() {
        QString dir = QFileDialog::getExistingDirectory(
            this, tr("Select Default Extract Directory"),
            m_extractPathEdit->text());
        if (!dir.isEmpty()) {
            m_extractPathEdit->setText(dir);
        }
    }

    void SettingsDialog::OnBrowseProjectsDir() {
        QString initialDir = m_projectsDirEdit->text().trimmed();
        if (initialDir.isEmpty()) {
            initialDir = QString::fromStdString(
                Nomad::Archives::ProjectManager::GetInstance()
                    .GetProjectsRoot()
                    .string());
        }
        QString dir = QFileDialog::getExistingDirectory(
            this, tr("Select Projects Directory"), initialDir);
        if (!dir.isEmpty()) {
            m_projectsDirEdit->setText(QDir::toNativeSeparators(dir));
            OnProjectsDirChanged();
        }
    }

    void SettingsDialog::OnRegisterAssociationNow() {
        if (Platform::FileAssociation::RegisterFatAssociation(true)) {
            QMessageBox::information(
                this, tr("File Association"),
                tr("BFView has been registered as the default "
                   "application for .fat files."));
        } else {
            QMessageBox::warning(this, tr("File Association"),
                                 tr("Failed to register file association."));
        }
    }

    void SettingsDialog::OnProjectsDirChanged() { PopulateProjectCombos(); }

    void SettingsDialog::OnAccept() {
        auto& settings = SettingsManager::GetInstance();

        QString chosenTheme = m_themeCombo->currentData().toString();
        settings.SetAppTheme(chosenTheme);
        ThemeManager::GetInstance().ApplyTheme(chosenTheme);

        settings.SetConfirmOnDelete(m_confirmDeleteCheckBox->isChecked());
        settings.SetConfirmOnAdd(m_confirmAddCheckBox->isChecked());
        settings.SetUseCustomHandlers(m_customHandlersCheckBox->isChecked());
        settings.SetCheckAssociationOnLaunch(
            m_autoRegisterAssocCheckBox->isChecked());
        settings.SetTempExtractPath(m_tempPathEdit->text().trimmed());
        settings.SetDefaultExtractPath(m_extractPathEdit->text().trimmed());

        settings.SetProjectsDirectory(m_projectsDirEdit->text().trimmed());

        settings.SetDefaultProject(
            Nomad::Archives::ArchiveVersion::FATv2, 7,
            m_fatv2_v7ProjectCombo->currentData().toString());
        settings.SetDefaultProject(
            Nomad::Archives::ArchiveVersion::FATv2, 8,
            m_fatv2_v8ProjectCombo->currentData().toString());
        settings.SetDefaultProject(
            Nomad::Archives::ArchiveVersion::FATv3, 7,
            m_fatv3_v7ProjectCombo->currentData().toString());
        settings.SetDefaultProject(
            Nomad::Archives::ArchiveVersion::FATv3, 8,
            m_fatv3_v8ProjectCombo->currentData().toString());
        settings.SetDefaultProject(
            Nomad::Archives::ArchiveVersion::FATv5, 11,
            m_fatv5_v11ProjectCombo->currentData().toString());
        settings.SetDefaultProject(
            Nomad::Archives::ArchiveVersion::FATv5, 12,
            m_fatv5_v12ProjectCombo->currentData().toString());
        settings.SetDefaultProject(
            Nomad::Archives::ArchiveVersion::FATv5, 13,
            m_fatv5_v13ProjectCombo->currentData().toString());

        settings.SetDefaultCompression(
            Nomad::Archives::ArchiveVersion::FATv2, 7,
            static_cast<Nomad::Archives::CompressionScheme>(
                m_fatv2_v7CompressionCombo->currentData().toInt()));
        settings.SetDefaultCompression(
            Nomad::Archives::ArchiveVersion::FATv2, 8,
            static_cast<Nomad::Archives::CompressionScheme>(
                m_fatv2_v8CompressionCombo->currentData().toInt()));
        settings.SetDefaultCompression(
            Nomad::Archives::ArchiveVersion::FATv3, 7,
            static_cast<Nomad::Archives::CompressionScheme>(
                m_fatv3_v7CompressionCombo->currentData().toInt()));
        settings.SetDefaultCompression(
            Nomad::Archives::ArchiveVersion::FATv3, 8,
            static_cast<Nomad::Archives::CompressionScheme>(
                m_fatv3_v8CompressionCombo->currentData().toInt()));
        settings.SetDefaultCompression(
            Nomad::Archives::ArchiveVersion::FATv5, 11,
            static_cast<Nomad::Archives::CompressionScheme>(
                m_fatv5_v11CompressionCombo->currentData().toInt()));
        settings.SetDefaultCompression(
            Nomad::Archives::ArchiveVersion::FATv5, 12,
            static_cast<Nomad::Archives::CompressionScheme>(
                m_fatv5_v12CompressionCombo->currentData().toInt()));
        settings.SetDefaultCompression(
            Nomad::Archives::ArchiveVersion::FATv5, 13,
            static_cast<Nomad::Archives::CompressionScheme>(
                m_fatv5_v13CompressionCombo->currentData().toInt()));

        settings.Save();
        accept();
    }

} // namespace BFView
