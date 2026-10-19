//
// Created by troplo on 09/21/26.
//

#ifndef BFVIEW_NAVIGATIONHISTORY_H
#define BFVIEW_NAVIGATIONHISTORY_H

#include <QObject>
#include <QString>
#include <vector>

namespace BFView::Models {
    class NavigationHistory : public QObject {
        Q_OBJECT

      public:
        explicit NavigationHistory(QObject* parent = nullptr);
        ~NavigationHistory() override = default;

        void Clear();
        void NavigateTo(const QString& path);
        [[nodiscard]] QString GoBack();
        [[nodiscard]] QString GoForward();
        [[nodiscard]] QString GetParentPath() const;

        [[nodiscard]] bool CanGoBack() const;
        [[nodiscard]] bool CanGoForward() const;
        [[nodiscard]] bool CanGoUp() const;
        [[nodiscard]] QString GetCurrentPath() const;

      signals:
        void StateChanged(bool canGoBack, bool canGoForward, bool canGoUp);

      private:
        std::vector<QString> m_history;
        int m_historyIndex{-1};
    };

} // namespace BFView::Models

#endif // BFVIEW_NAVIGATIONHISTORY_H
