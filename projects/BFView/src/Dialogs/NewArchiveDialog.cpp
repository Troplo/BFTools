//
// Created by troplo on 10/15/26.
//

#include "BFView/Dialogs/NewArchiveDialog.hpp"
#include "BFView/Settings/SettingsManager.hpp"
#include "Nomad/Archives/ProjectManager.hpp"
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace BFView::Dialogs {
    static constexpr int RawVersionRole = Qt::UserRole + 1;

    NewArchiveDialog::NewArchiveDialog(QWidget *parent) : QDialog(parent) {
        setWindowTitle(tr("Create New BigFile Archive"));
        setMinimumWidth(500);

        auto *mainLayout = new QVBoxLayout(this);
        auto *formLayout = new QFormLayout();

        // File path row
        auto *pathLayout = new QHBoxLayout();
        m_pathEdit = new QLineEdit(this);
        m_pathEdit->setPlaceholderText(tr("Select path for new .fat file..."));
        m_browseButton = new QPushButton(tr("Browse..."), this);
        pathLayout->addWidget(m_pathEdit);
        pathLayout->addWidget(m_browseButton);
        connect(m_browseButton, &QPushButton::clicked, this,
                &NewArchiveDialog::OnBrowseClicked);
        formLayout->addRow(tr("Archive Path:"), pathLayout);

        // FAT version
        m_versionCombo = new QComboBox(this);
        m_versionCombo->addItem(
            tr("FATv2 (Watch Dogs 1 Beta)"),
            static_cast<int>(Nomad::Archives::ArchiveVersion::FATv2));
        m_versionCombo->setItemData(0, 8, RawVersionRole);

        m_versionCombo->addItem(
            tr("FATv3 (Watch Dogs 1)"),
            static_cast<int>(Nomad::Archives::ArchiveVersion::FATv3));
        m_versionCombo->setItemData(1, 8, RawVersionRole);

        m_versionCombo->addItem(
            tr("FATv5 (Watch Dogs 2)"),
            static_cast<int>(Nomad::Archives::ArchiveVersion::FATv5));
        m_versionCombo->setItemData(2, 11, RawVersionRole);

        m_versionCombo->addItem(
            tr("FATv5 (Watch Dogs: Legion)"),
            static_cast<int>(Nomad::Archives::ArchiveVersion::FATv5));
        m_versionCombo->setItemData(3, 13, RawVersionRole);

        m_versionCombo->setCurrentIndex(1); // Default to FATv3
        connect(m_versionCombo,
                QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                &NewArchiveDialog::OnVersionChanged);
        formLayout->addRow(tr("FAT Version:"), m_versionCombo);

        // Raw version
        m_rawVersionSpin = new QSpinBox(this);
        m_rawVersionSpin->setRange(7, 8);
        m_rawVersionSpin->setValue(8);
        formLayout->addRow(tr("Raw Version:"), m_rawVersionSpin);

        // Platform
        m_platformCombo = new QComboBox(this);
        m_platformCombo->addItem(
            tr("Win64"), static_cast<int>(Nomad::Archives::Platform::Win64));
        m_platformCombo->addItem(
            tr("Win32 (Unused)"),
            static_cast<int>(Nomad::Archives::Platform::Win32));
        m_platformCombo->addItem(
            tr("Xbox 360 (Xenon)"),
            static_cast<int>(Nomad::Archives::Platform::Xenon));
        m_platformCombo->addItem(
            tr("PS4 (Orbis)"),
            static_cast<int>(Nomad::Archives::Platform::Orbis));
        m_platformCombo->addItem(
            tr("Xbox One (Durango)"),
            static_cast<int>(Nomad::Archives::Platform::Durango));
        m_platformCombo->addItem(
            tr("PS3"), static_cast<int>(Nomad::Archives::Platform::PS3));
        m_platformCombo->addItem(
            tr("Wii U"), static_cast<int>(Nomad::Archives::Platform::WiiU));
        m_platformCombo->addItem(
            tr("Generic"),
            static_cast<int>(Nomad::Archives::Platform::Unknown));
        formLayout->addRow(tr("Platform:"), m_platformCombo);

        // Default compression
        m_compressionCombo = new QComboBox(this);
        m_compressionCombo->addItem(
            tr("LZ4LW"),
            static_cast<int>(Nomad::Archives::CompressionScheme::LZ4LW));
        m_compressionCombo->addItem(
            tr("XMemCompress (PC/X360)"),
            static_cast<int>(Nomad::Archives::CompressionScheme::XMemCompress));
        m_compressionCombo->addItem(
            tr("LZO1x"),
            static_cast<int>(Nomad::Archives::CompressionScheme::LZO1x));
        m_compressionCombo->addItem(
            tr("Zlib"),
            static_cast<int>(Nomad::Archives::CompressionScheme::Zlib));
        m_compressionCombo->addItem(
            tr("None (Uncompressed)"),
            static_cast<int>(Nomad::Archives::CompressionScheme::None));
        formLayout->addRow(tr("Default Compression:"), m_compressionCombo);

        // Associated project
        m_projectCombo = new QComboBox(this);
        m_projectCombo->addItem(tr("(None - Raw Hashes)"),
                                SettingsManager::NONE_PROJECT);
        auto availableProjects = Nomad::Archives::ProjectManager::GetInstance()
                                     .GetAvailableProjects();
        for (const auto &proj : availableProjects) {
            QString qProj = QString::fromStdString(proj);
            m_projectCombo->addItem(qProj, qProj);
        }
        formLayout->addRow(tr("Project:"), m_projectCombo);

        mainLayout->addLayout(formLayout);

        // Buttons
        auto *buttonBox = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        connect(buttonBox, &QDialogButtonBox::accepted, this,
                &NewArchiveDialog::OnAccept);
        connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
        mainLayout->addWidget(buttonBox);

        // Trigger initial version defaults
        OnVersionChanged(m_versionCombo->currentIndex());
    }

    void NewArchiveDialog::OnBrowseClicked() {
        QString selected = QFileDialog::getSaveFileName(
            this, tr("Create New BigFile"),
            m_pathEdit->text().trimmed().isEmpty()
                ? QString("patch.fat")
                : m_pathEdit->text().trimmed(),
            tr("FAT Archives (*.fat);;All Files (*.*)"));

        if (!selected.isEmpty()) {
            if (!selected.endsWith(".fat", Qt::CaseInsensitive)) {
                selected += ".fat";
            }
            m_pathEdit->setText(selected);
        }
    }

    void NewArchiveDialog::OnVersionChanged(int index) {
        if (index < 0)
            return;
        auto version = GetArchiveVersion();
        int defaultRaw =
            m_versionCombo->itemData(index, RawVersionRole).toInt();

        switch (version) {
        case Nomad::Archives::ArchiveVersion::FATv2:
            m_rawVersionSpin->setRange(7, 8);
            m_rawVersionSpin->setValue(defaultRaw == 7 ? 7 : 8);
            for (int i = 0; i < m_platformCombo->count(); ++i) {
                if (m_platformCombo->itemData(i).toInt() ==
                    static_cast<int>(Nomad::Archives::Platform::Unknown)) {
                    m_platformCombo->setCurrentIndex(i);
                    break;
                }
            }
            for (int i = 0; i < m_compressionCombo->count(); ++i) {
                if (m_compressionCombo->itemData(i).toInt() ==
                    static_cast<int>(
                        Nomad::Archives::CompressionScheme::XMemCompress)) {
                    m_compressionCombo->setCurrentIndex(i);
                    break;
                }
            }
            break;
        case Nomad::Archives::ArchiveVersion::FATv3:
            m_rawVersionSpin->setRange(7, 8);
            m_rawVersionSpin->setValue(defaultRaw == 7 ? 7 : 8);
            for (int i = 0; i < m_platformCombo->count(); ++i) {
                if (m_platformCombo->itemData(i).toInt() ==
                    static_cast<int>(Nomad::Archives::Platform::Win64)) {
                    m_platformCombo->setCurrentIndex(i);
                    break;
                }
            }
            for (int i = 0; i < m_compressionCombo->count(); ++i) {
                if (m_compressionCombo->itemData(i).toInt() ==
                    static_cast<int>(
                        Nomad::Archives::CompressionScheme::XMemCompress)) {
                    m_compressionCombo->setCurrentIndex(i);
                    break;
                }
            }
            break;
        case Nomad::Archives::ArchiveVersion::FATv5:
            if (defaultRaw == 11) {
                m_rawVersionSpin->setRange(11, 11);
                m_rawVersionSpin->setValue(11);
            } else {
                m_rawVersionSpin->setRange(13, 13);
                m_rawVersionSpin->setValue(13);
            }
            for (int i = 0; i < m_platformCombo->count(); ++i) {
                if (m_platformCombo->itemData(i).toInt() ==
                    static_cast<int>(Nomad::Archives::Platform::Win64)) {
                    m_platformCombo->setCurrentIndex(i);
                    break;
                }
            }
            for (int i = 0; i < m_compressionCombo->count(); ++i) {
                if (m_compressionCombo->itemData(i).toInt() ==
                    static_cast<int>(
                        Nomad::Archives::CompressionScheme::LZ4LW)) {
                    m_compressionCombo->setCurrentIndex(i);
                    break;
                }
            }
            break;
        }
    }

    void NewArchiveDialog::OnAccept() {
        QString path = m_pathEdit->text().trimmed();
        if (path.isEmpty()) {
            QMessageBox::warning(
                this, tr("Invalid Path"),
                tr("Please specify a valid path for the new archive."));
            return;
        }

        if (!path.endsWith(".fat", Qt::CaseInsensitive)) {
            path += ".fat";
            m_pathEdit->setText(path);
        }

        QFileInfo fileInfo(path);
        if (fileInfo.exists()) {
            auto reply =
                QMessageBox::question(this, tr("File Exists"),
                                      tr("The file '%1' already exists.\nDo "
                                         "you want to overwrite it?")
                                          .arg(fileInfo.fileName()),
                                      QMessageBox::Yes | QMessageBox::No);
            if (reply != QMessageBox::Yes) {
                return;
            }
        }

        accept();
    }

    QString NewArchiveDialog::GetFatPath() const {
        QString path = m_pathEdit->text().trimmed();
        if (!path.endsWith(".fat", Qt::CaseInsensitive)) {
            path += ".fat";
        }
        return path;
    }

    Nomad::Archives::ArchiveVersion
    NewArchiveDialog::GetArchiveVersion() const {
        return static_cast<Nomad::Archives::ArchiveVersion>(
            m_versionCombo->currentData().toInt());
    }

    uint32_t NewArchiveDialog::GetRawVersion() const {
        return static_cast<uint32_t>(m_rawVersionSpin->value());
    }

    Nomad::Archives::Platform NewArchiveDialog::GetPlatform() const {
        return static_cast<Nomad::Archives::Platform>(
            m_platformCombo->currentData().toInt());
    }

    Nomad::Archives::CompressionScheme
    NewArchiveDialog::GetDefaultCompression() const {
        return static_cast<Nomad::Archives::CompressionScheme>(
            m_compressionCombo->currentData().toInt());
    }

    QString NewArchiveDialog::GetSelectedProject() const {
        return m_projectCombo->currentData().toString();
    }

} // namespace BFView::Dialogs
