//
// Created by troplo on 10/15/26.
//

#include "BFView/Dialogs/DefragProgressDialog.hpp"
#include <QVBoxLayout>

namespace BFView::Dialogs {
    DefragProgressDialog::DefragProgressDialog(QWidget* parent)
        : QDialog(parent),
          m_cancelFlag(std::make_shared<std::atomic<bool>>(false)) {
        setWindowTitle(tr("Defragmenting Archive..."));
        setModal(true);
        setFixedSize(500, 160);

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(16, 16, 16, 16);
        layout->setSpacing(10);

        m_lblStatus = new QLabel(tr("Preparing defragmentation..."), this);
        m_lblDetails = new QLabel(QString(), this);
        m_lblDetails->setWordWrap(true);

        m_progressBar = new QProgressBar(this);
        m_progressBar->setRange(0, 100);
        m_progressBar->setValue(0);

        m_btnCancel = new QPushButton(tr("Cancel"), this);
        connect(m_btnCancel, &QPushButton::clicked, this, [this]() {
            m_cancelFlag->store(true);
            m_btnCancel->setEnabled(false);
            m_lblStatus->setText(tr("Cancelling defragmentation..."));
            reject();
        });

        layout->addWidget(m_lblStatus);
        layout->addWidget(m_lblDetails);
        layout->addWidget(m_progressBar);
        layout->addWidget(m_btnCancel, 0, Qt::AlignRight);
    }

    void DefragProgressDialog::SetStatus(int current, int total,
                                         const QString& details) {
        if (total > 0) {
            int percent = static_cast<int>(
                (static_cast<double>(current) / static_cast<double>(total)) *
                100.0);
            m_progressBar->setValue(percent);
            m_lblStatus->setText(
                QString(tr("Defragmenting block %1 of %2 (%3%)"))
                    .arg(current)
                    .arg(total)
                    .arg(percent));
        } else {
            m_lblStatus->setText(tr("Defragmenting archive..."));
        }

        if (!details.isEmpty()) {
            m_lblDetails->setText(details);
        }
    }
} // namespace BFView::Dialogs
