//
// Created by troplo on 10/15/26.
//

#ifndef BFVIEW_THEMEMANAGER_H
#define BFVIEW_THEMEMANAGER_H

#include <QObject>
#include <QPalette>
#include <QString>

namespace BFView {
    enum class AppTheme { System = 0, BreezeLight = 1, BreezeDark = 2 };

    class ThemeManager : public QObject {
        Q_OBJECT
      public:
        static ThemeManager &GetInstance();

        AppTheme GetCurrentTheme() const { return m_currentTheme; }
        void ApplyTheme(AppTheme theme);
        void ApplyTheme(const QString &themeName);

        static QString ThemeToString(AppTheme theme);
        static AppTheme ThemeFromString(const QString &str);

      signals:
        void ThemeChanged(AppTheme newTheme);

      private:
        ThemeManager();
        ~ThemeManager() override = default;

        void SetSystemTheme();
        void SetBreezeTheme(bool dark);

        AppTheme m_currentTheme{AppTheme::System};
        QPalette m_originalPalette;
        QString m_originalStyleName;
        QString m_originalIconTheme;
        bool m_initialized{false};
    };

} // namespace BFView

#endif // BFVIEW_THEMEMANAGER_H
