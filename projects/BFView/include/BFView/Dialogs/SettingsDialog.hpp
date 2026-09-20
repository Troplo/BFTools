//
// Created by troplo on 09/21/26.
//

#ifndef BFVIEW_SETTINGSDIALOG_H
#define BFVIEW_SETTINGSDIALOG_H

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPushButton;

namespace BFView {
    class SettingsDialog : public QDialog {
        Q_OBJECT

      public:
        explicit SettingsDialog(QWidget *parent = nullptr);
        ~SettingsDialog() override = default;

      private slots:
        void OnBrowseTempPath();
        void OnBrowseExtractPath();
        void OnBrowseProjectsDir();
        void OnProjectsDirChanged();
        void OnRegisterAssociationNow();
        void OnAccept();

      private:
        void PopulateProjectCombos();
        void PopulateCompressionCombos();

        QComboBox *m_themeCombo{nullptr};
        QCheckBox *m_confirmDeleteCheckBox{nullptr};
        QCheckBox *m_confirmAddCheckBox{nullptr};
        QCheckBox *m_customHandlersCheckBox{nullptr};
        QCheckBox *m_autoRegisterAssocCheckBox{nullptr};
        QLineEdit *m_tempPathEdit{nullptr};
        QLineEdit *m_extractPathEdit{nullptr};
        QPushButton *m_browseTempButton{nullptr};
        QPushButton *m_browseExtractButton{nullptr};
        QPushButton *m_registerAssocNowBtn{nullptr};

        QLineEdit *m_projectsDirEdit{nullptr};
        QPushButton *m_browseProjectsDirButton{nullptr};

        QComboBox *m_fatv2_v7ProjectCombo{nullptr};
        QComboBox *m_fatv2_v8ProjectCombo{nullptr};
        QComboBox *m_fatv3_v7ProjectCombo{nullptr};
        QComboBox *m_fatv3_v8ProjectCombo{nullptr};
        QComboBox *m_fatv5_v11ProjectCombo{nullptr};
        QComboBox *m_fatv5_v12ProjectCombo{nullptr};
        QComboBox *m_fatv5_v13ProjectCombo{nullptr};

        QComboBox *m_fatv2_v7CompressionCombo{nullptr};
        QComboBox *m_fatv2_v8CompressionCombo{nullptr};
        QComboBox *m_fatv3_v7CompressionCombo{nullptr};
        QComboBox *m_fatv3_v8CompressionCombo{nullptr};
        QComboBox *m_fatv5_v11CompressionCombo{nullptr};
        QComboBox *m_fatv5_v12CompressionCombo{nullptr};
        QComboBox *m_fatv5_v13CompressionCombo{nullptr};
    };

} // namespace BFView

#endif // BFVIEW_SETTINGSDIALOG_H
