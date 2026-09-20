//
// Created by troplo on 10/17/26.
//

#include "BFView/Dialogs/LoadArchiveProgressDialog.hpp"
#include <QVBoxLayout>

namespace BFView::Dialogs {
    LoadArchiveProgressDialog::LoadArchiveProgressDialog(QWidget *parent)
        : QDialog(parent) {
        setWindowTitle(tr("Opening Archive..."));
        setModal(true);
        setWindowModality(Qt::ApplicationModal);
        setFixedSize(440, 130);
        setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint |
                       Qt::WindowTitleHint);

        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(20, 20, 20, 20);
        layout->setSpacing(10);

        m_lblTitle = new QLabel(tr("Loading archive..."), this);
        QFont titleFont = m_lblTitle->font();
        titleFont.setBold(true);
        m_lblTitle->setFont(titleFont);

        m_lblStatus = new QLabel(tr("Reading file entries..."), this);

        m_progressBar = new QProgressBar(this);
        m_progressBar->setRange(0, 0); // Indeterminate marquee animation
        m_progressBar->setTextVisible(false);

        layout->addWidget(m_lblTitle);
        layout->addWidget(m_lblStatus);
        layout->addWidget(m_progressBar);
    }

    void LoadArchiveProgressDialog::SetArchiveName(const QString &name) {
        m_lblTitle->setText(tr("Opening: %1").arg(name));
    }

    void LoadArchiveProgressDialog::SetStatus(const QString &status) {
        m_lblStatus->setText(status);
    }

} // namespace BFView::Dialogs
