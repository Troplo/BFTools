//
// Created by troplo on 10/14/26.
//

#include "BFView/Dialogs/ProjectSelectDialog.hpp"
#include "Nomad/Archives/ProjectManager.hpp"
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <string>

namespace BFView::Dialogs {
    ProjectSelectDialog::ProjectSelectDialog(
        Nomad::Archives::ArchiveVersion version, uint32_t rawVersion,
        const QString &archivePath, QWidget *parent)
        : QDialog(parent), m_version(version), m_rawVersion(rawVersion) {
        setWindowTitle(tr("Select Project Configuration"));
        setMinimumWidth(420);

        auto *mainLayout = new QVBoxLayout(this);

        auto *formLayout = new QFormLayout();

        QFileInfo fileInfo(archivePath);
        auto *fileLabel = new QLabel(fileInfo.fileName(), this);
        fileLabel->setStyleSheet("font-weight: bold;");
        formLayout->addRow(tr("Archive:"), fileLabel);

        QString verBase = QString::fromStdString(
            std::string(Nomad::Archives::VersionToString(version)));
        QString verStr = (rawVersion > 0)
                             ? QString("%1 (v%2)").arg(verBase).arg(rawVersion)
                             : verBase;
        auto *verLabel = new QLabel(verStr, this);
        formLayout->addRow(tr("Detected Format:"), verLabel);

        m_projectComboBox = new QComboBox(this);
        m_projectComboBox->addItem(tr("(None - Raw Hashes)"), QString(""));

        auto availableProjects = Nomad::Archives::ProjectManager::GetInstance()
                                     .GetAvailableProjects();
        for (const auto &proj : availableProjects) {
            QString qProj = QString::fromStdString(proj);
            m_projectComboBox->addItem(qProj, qProj);
        }

        formLayout->addRow(tr("Project:"), m_projectComboBox);
        mainLayout->addLayout(formLayout);

        m_alwaysCheckBox = new QCheckBox(
            tr("Always use this project for %1 archives").arg(verStr), this);
        mainLayout->addWidget(m_alwaysCheckBox);

        auto *buttonBox = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
        mainLayout->addWidget(buttonBox);
    }

    QString ProjectSelectDialog::GetSelectedProject() const {
        return m_projectComboBox->currentData().toString();
    }

    bool ProjectSelectDialog::IsAlwaysUseChecked() const {
        return m_alwaysCheckBox->isChecked();
    }

} // namespace BFView::Dialogs
