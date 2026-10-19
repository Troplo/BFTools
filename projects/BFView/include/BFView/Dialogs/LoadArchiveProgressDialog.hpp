//
// Created by troplo on 10/17/26.
//

#ifndef BFVIEW_LOADARCHIVEPROGRESSDIALOG_H
#define BFVIEW_LOADARCHIVEPROGRESSDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QProgressBar>

namespace BFView::Dialogs {
    class LoadArchiveProgressDialog : public QDialog {
        Q_OBJECT

      public:
        explicit LoadArchiveProgressDialog(QWidget* parent = nullptr);
        ~LoadArchiveProgressDialog() override = default;

        void SetArchiveName(const QString& name);
        void SetStatus(const QString& status);

      private:
        QLabel* m_lblTitle{nullptr};
        QLabel* m_lblStatus{nullptr};
        QProgressBar* m_progressBar{nullptr};
    };

} // namespace BFView::Dialogs

#endif // BFVIEW_LOADARCHIVEPROGRESSDIALOG_H
