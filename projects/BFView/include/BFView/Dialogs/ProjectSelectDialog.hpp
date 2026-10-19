//
// Created by troplo on 10/14/26.
//

#ifndef BFVIEW_PROJECTSELECTDIALOG_H
#define BFVIEW_PROJECTSELECTDIALOG_H

#include "Nomad/Archives/Types.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QString>

namespace BFView::Dialogs {
    class ProjectSelectDialog : public QDialog {
        Q_OBJECT
      public:
        explicit ProjectSelectDialog(Nomad::Archives::ArchiveVersion version,
                                     uint32_t rawVersion,
                                     const QString& archivePath,
                                     QWidget* parent = nullptr);
        explicit ProjectSelectDialog(Nomad::Archives::ArchiveVersion version,
                                     const QString& archivePath,
                                     QWidget* parent = nullptr)
            : ProjectSelectDialog(version, 0, archivePath, parent) {}

        [[nodiscard]] QString GetSelectedProject() const;
        [[nodiscard]] bool IsAlwaysUseChecked() const;
        [[nodiscard]] uint32_t GetRawVersion() const { return m_rawVersion; }

      private:
        Nomad::Archives::ArchiveVersion m_version;
        uint32_t m_rawVersion{0};
        QComboBox* m_projectComboBox{nullptr};
        QCheckBox* m_alwaysCheckBox{nullptr};
    };

} // namespace BFView::Dialogs

#endif // BFVIEW_PROJECTSELECTDIALOG_H
