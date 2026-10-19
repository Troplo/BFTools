//
// Created by troplo on 10/15/26.
//

#ifndef BFVIEW_NEWARCHIVEDIALOG_H
#define BFVIEW_NEWARCHIVEDIALOG_H

#include "Nomad/Archives/Types.hpp"
#include <QDialog>

class QLineEdit;
class QPushButton;
class QComboBox;
class QSpinBox;

namespace BFView::Dialogs {
    class NewArchiveDialog : public QDialog {
        Q_OBJECT

      public:
        explicit NewArchiveDialog(QWidget* parent = nullptr);
        ~NewArchiveDialog() override = default;

        [[nodiscard]] QString GetFatPath() const;
        [[nodiscard]] Nomad::Archives::ArchiveVersion GetArchiveVersion() const;
        [[nodiscard]] uint32_t GetRawVersion() const;
        [[nodiscard]] Nomad::Archives::Platform GetPlatform() const;
        [[nodiscard]] Nomad::Archives::CompressionScheme
        GetDefaultCompression() const;
        [[nodiscard]] QString GetSelectedProject() const;

      private slots:
        void OnBrowseClicked();
        void OnVersionChanged(int index);
        void OnAccept();

      private:
        QLineEdit* m_pathEdit{nullptr};
        QPushButton* m_browseButton{nullptr};
        QComboBox* m_versionCombo{nullptr};
        QSpinBox* m_rawVersionSpin{nullptr};
        QComboBox* m_platformCombo{nullptr};
        QComboBox* m_compressionCombo{nullptr};
        QComboBox* m_projectCombo{nullptr};
    };

} // namespace BFView::Dialogs

#endif // BFVIEW_NEWARCHIVEDIALOG_H
