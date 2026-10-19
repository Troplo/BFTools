//
// Created by troplo on 09/21/26.
//

#ifndef BFVIEW_EXTRACTPROGRESSDIALOG_H
#define BFVIEW_EXTRACTPROGRESSDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>

namespace BFView::Dialogs {
    class ExtractProgressDialog : public QDialog {
        Q_OBJECT

      public:
        explicit ExtractProgressDialog(QWidget* parent = nullptr);
        ~ExtractProgressDialog() override = default;

        void SetStatus(int current, int total, const QString& currentFile);
        [[nodiscard]] bool IsCancelled() const { return m_isCancelled; }

      private:
        QProgressBar* m_progressBar{nullptr};
        QLabel* m_lblStatus{nullptr};
        QLabel* m_lblFile{nullptr};
        QPushButton* m_btnCancel{nullptr};
        bool m_isCancelled{false};
    };

} // namespace BFView::Dialogs

#endif // BFVIEW_EXTRACTPROGRESSDIALOG_H
