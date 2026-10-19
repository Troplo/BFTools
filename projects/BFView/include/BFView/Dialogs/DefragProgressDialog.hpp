//
// Created by troplo on 10/15/26.
//

#ifndef BFVIEW_DEFRAGPROGRESSDIALOG_H
#define BFVIEW_DEFRAGPROGRESSDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <atomic>
#include <memory>

namespace BFView::Dialogs {
    class DefragProgressDialog : public QDialog {
        Q_OBJECT

      public:
        explicit DefragProgressDialog(QWidget* parent = nullptr);
        ~DefragProgressDialog() override = default;

        void SetStatus(int current, int total,
                       const QString& details = QString());
        [[nodiscard]] std::shared_ptr<std::atomic<bool>> GetCancelFlag() const {
            return m_cancelFlag;
        }

      private:
        QProgressBar* m_progressBar{nullptr};
        QLabel* m_lblStatus{nullptr};
        QLabel* m_lblDetails{nullptr};
        QPushButton* m_btnCancel{nullptr};
        std::shared_ptr<std::atomic<bool>> m_cancelFlag;
    };

} // namespace BFView::Dialogs

#endif // BFVIEW_DEFRAGPROGRESSDIALOG_H
