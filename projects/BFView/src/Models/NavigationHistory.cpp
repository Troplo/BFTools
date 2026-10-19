//
// Created by troplo on 09/21/26.
//

#include "BFView/Models/NavigationHistory.hpp"
#include <QStringList>

namespace BFView::Models {
    static QString NormalizeNavPath(const QString& path) {
        if (path.isEmpty() || path == "/")
            return "/";
        QString p = path;
        while (p.startsWith('/'))
            p.remove(0, 1);
        while (p.endsWith('/'))
            p.chop(1);
        return p.isEmpty() ? "/" : p;
    }

    NavigationHistory::NavigationHistory(QObject* parent) : QObject(parent) {}

    void NavigationHistory::Clear() {
        m_history.clear();
        m_historyIndex = -1;
        emit StateChanged(CanGoBack(), CanGoForward(), CanGoUp());
    }

    void NavigationHistory::NavigateTo(const QString& path) {
        QString normPath = NormalizeNavPath(path);
        if (m_historyIndex >= 0 &&
            m_historyIndex < static_cast<int>(m_history.size()) &&
            m_history[m_historyIndex] == normPath) {
            return;
        }

        if (m_historyIndex >= 0 &&
            m_historyIndex + 1 < static_cast<int>(m_history.size())) {
            m_history.erase(m_history.begin() + m_historyIndex + 1,
                            m_history.end());
        }

        m_history.push_back(normPath);
        m_historyIndex = static_cast<int>(m_history.size()) - 1;
        emit StateChanged(CanGoBack(), CanGoForward(), CanGoUp());
    }

    QString NavigationHistory::GoBack() {
        if (CanGoBack()) {
            m_historyIndex--;
            emit StateChanged(CanGoBack(), CanGoForward(), CanGoUp());
            return m_history[m_historyIndex];
        }
        return GetCurrentPath();
    }

    QString NavigationHistory::GoForward() {
        if (CanGoForward()) {
            m_historyIndex++;
            emit StateChanged(CanGoBack(), CanGoForward(), CanGoUp());
            return m_history[m_historyIndex];
        }
        return GetCurrentPath();
    }

    QString NavigationHistory::GetParentPath() const {
        QString current = GetCurrentPath();
        if (current == "/" || current.isEmpty())
            return "/";

        QStringList parts = current.split('/', Qt::SkipEmptyParts);
        if (parts.size() <= 1)
            return "/";

        parts.removeLast();
        return parts.join('/');
    }

    bool NavigationHistory::CanGoBack() const { return m_historyIndex > 0; }

    bool NavigationHistory::CanGoForward() const {
        return m_historyIndex >= 0 &&
               m_historyIndex + 1 < static_cast<int>(m_history.size());
    }

    bool NavigationHistory::CanGoUp() const {
        QString current = GetCurrentPath();
        return !current.isEmpty() && current != "/";
    }

    QString NavigationHistory::GetCurrentPath() const {
        if (m_historyIndex >= 0 &&
            m_historyIndex < static_cast<int>(m_history.size())) {
            return m_history[m_historyIndex];
        }
        return "/";
    }

} // namespace BFView::Models
