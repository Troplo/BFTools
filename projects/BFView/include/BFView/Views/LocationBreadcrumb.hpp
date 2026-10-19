//
// Created by troplo on 09/21/26.
//

#ifndef BFVIEW_LOCATIONBREADCRUMB_H
#define BFVIEW_LOCATIONBREADCRUMB_H

#include <QFrame>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QScrollArea>
#include <QStackedWidget>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <functional>

namespace BFView::Views {
    class LocationBreadcrumb : public QWidget {
        Q_OBJECT

      public:
        using SubfolderProvider =
            std::function<QStringList(const QString& path)>;

        explicit LocationBreadcrumb(QWidget* parent = nullptr);
        ~LocationBreadcrumb() override = default;

        void SetPath(const QString& path);
        void FocusLocationInput();
        void SetSubfolderProvider(SubfolderProvider provider);
        void UpdateIcons();

      signals:
        void PathClicked(const QString& fullPath);

      protected:
        bool eventFilter(QObject* watched, QEvent* event) override;
        void changeEvent(QEvent* event) override;

      private:
        void SetupUi();
        void SwitchToEditablePath();
        void SwitchToBreadcrumbs();
        [[nodiscard]] QIcon GetArrowIcon() const;
        [[nodiscard]] QIcon GetFolderIcon() const;
        [[nodiscard]] QIcon GetRootIcon() const;

        QFrame* m_containerFrame{nullptr};
        QStackedWidget* m_stackedWidget{nullptr};
        QScrollArea* m_scrollArea{nullptr};
        QWidget* m_breadcrumbContainer{nullptr};
        QHBoxLayout* m_breadcrumbLayout{nullptr};
        QLineEdit* m_pathEdit{nullptr};
        QString m_currentPath{"/"};
        SubfolderProvider m_subfolderProvider;
    };

} // namespace BFView::Views

#endif // BFVIEW_LOCATIONBREADCRUMB_H
