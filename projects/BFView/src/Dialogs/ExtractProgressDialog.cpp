//
// Created by troplo on 09/21/26.
//

#include "BFView/Dialogs/ExtractProgressDialog.hpp"
#include <QVBoxLayout>

namespace BFView::Dialogs {
    ExtractProgressDialog::ExtractProgressDialog(QWidget *parent)
        : QDialog(parent) {
        setWindowTitle("Extracting Archive Items...");
        setModal(true);
        setFixedSize(500, 160);

        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(16, 16, 16, 16);
        layout->setSpacing(10);

        m_lblStatus = new QLabel("Preparing extraction...", this);
        m_lblFile = new QLabel("", this);
        m_lblFile->setWordWrap(true);

        m_progressBar = new QProgressBar(this);
        m_progressBar->setRange(0, 100);

        m_btnCancel = new QPushButton("Cancel", this);
        connect(m_btnCancel, &QPushButton::clicked, this, [this]() {
            m_isCancelled = true;
            reject();
        });

        layout->addWidget(m_lblStatus);
        layout->addWidget(m_lblFile);
        layout->addWidget(m_progressBar);
        layout->addWidget(m_btnCancel, 0, Qt::AlignRight);
    }

    void ExtractProgressDialog::SetStatus(int current, int total,
                                          const QString &currentFile) {
        if (total > 0) {
            int percent = static_cast<int>(
                (static_cast<double>(current) / total) * 100.0);
            m_progressBar->setValue(percent);
        }
        m_lblStatus->setText(
            QString("Extracting files: %1 of %2").arg(current).arg(total));
        m_lblFile->setText(currentFile);
    }

} // namespace BFView::Dialogs
