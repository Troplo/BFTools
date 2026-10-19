//
// Created by troplo on 10/15/26.
//

#include "BFView/Theme/ThemeManager.hpp"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QIcon>
#include <QSettings>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleHints>

namespace BFView {
    namespace {

        QColor ParseColor(const QString& str) {
            if (str.isEmpty())
                return QColor();
            const QStringList parts = str.split(',', Qt::SkipEmptyParts);
            if (parts.size() >= 3) {
                return QColor(parts[0].trimmed().toInt(),
                              parts[1].trimmed().toInt(),
                              parts[2].trimmed().toInt());
            }
            return QColor(str.trimmed());
        }

        QPalette LoadPaletteFromColorScheme(const QString& filePath,
                                            bool darkFallback) {
            QPalette pal;
            QSettings s(filePath, QSettings::IniFormat);

            auto readCol = [&](const QString& group, const QString& key,
                               const QColor& def) -> QColor {
                s.beginGroup(group);
                const QString val = s.value(key).toString();
                s.endGroup();
                const QColor c = ParseColor(val);
                return c.isValid() ? c : def;
            };

            const QColor defWindowBg =
                darkFallback ? QColor("#202326") : QColor("#eff0f1");
            const QColor defWindowFg =
                darkFallback ? QColor("#fcfcfc") : QColor("#232629");
            const QColor defViewBg =
                darkFallback ? QColor("#141618") : QColor("#ffffff");
            const QColor defViewFg =
                darkFallback ? QColor("#fcfcfc") : QColor("#232629");
            const QColor defButtonBg =
                darkFallback ? QColor("#292c30") : QColor("#fcfcfc");
            const QColor defButtonFg =
                darkFallback ? QColor("#fcfcfc") : QColor("#232629");
            const QColor defSelectBg =
                darkFallback ? QColor("#3daee9") : QColor("#3daee9");
            const QColor defSelectFg =
                darkFallback ? QColor("#ffffff") : QColor("#ffffff");
            const QColor defMid =
                darkFallback ? QColor("#363b40") : QColor("#cbd0d4");
            const QColor defDark =
                darkFallback ? QColor("#1a1c1e") : QColor("#b0b5b9");

            const QColor windowBg =
                readCol("Colors:Window", "BackgroundNormal", defWindowBg);
            const QColor windowFg =
                readCol("Colors:Window", "ForegroundNormal", defWindowFg);
            const QColor viewBg =
                readCol("Colors:View", "BackgroundNormal", defViewBg);
            const QColor viewFg =
                readCol("Colors:View", "ForegroundNormal", defViewFg);
            const QColor viewAlt =
                readCol("Colors:View", "BackgroundAlternate",
                        darkFallback ? QColor("#1d1f22") : QColor("#f7f7f7"));
            const QColor buttonBg =
                readCol("Colors:Button", "BackgroundNormal", defButtonBg);
            const QColor buttonFg =
                readCol("Colors:Button", "ForegroundNormal", defButtonFg);
            const QColor selectBg =
                readCol("Colors:Selection", "BackgroundNormal", defSelectBg);
            const QColor selectFg =
                readCol("Colors:Selection", "ForegroundNormal", defSelectFg);
            const QColor windowFgInactive =
                readCol("Colors:Window", "ForegroundInactive",
                        darkFallback ? QColor("#7f8c8d") : QColor("#707d8a"));
            const QColor viewFgInactive =
                readCol("Colors:View", "ForegroundInactive",
                        darkFallback ? QColor("#7f8c8d") : QColor("#707d8a"));
            const QColor buttonFgInactive =
                readCol("Colors:Button", "ForegroundInactive",
                        darkFallback ? QColor("#7f8c8d") : QColor("#707d8a"));

            // Active
            pal.setColor(QPalette::Active, QPalette::Window, windowBg);
            pal.setColor(QPalette::Active, QPalette::WindowText, windowFg);
            pal.setColor(QPalette::Active, QPalette::Base, viewBg);
            pal.setColor(QPalette::Active, QPalette::AlternateBase, viewAlt);
            pal.setColor(QPalette::Active, QPalette::Text, viewFg);
            pal.setColor(QPalette::Active, QPalette::Button, buttonBg);
            pal.setColor(QPalette::Active, QPalette::ButtonText, buttonFg);
            pal.setColor(QPalette::Active, QPalette::Highlight, selectBg);
            pal.setColor(QPalette::Active, QPalette::HighlightedText, selectFg);
            pal.setColor(QPalette::Active, QPalette::Mid, defMid);
            pal.setColor(QPalette::Active, QPalette::Dark, defDark);
            pal.setColor(QPalette::Active, QPalette::ToolTipBase, windowBg);
            pal.setColor(QPalette::Active, QPalette::ToolTipText, windowFg);
            pal.setColor(QPalette::Active, QPalette::PlaceholderText,
                         windowFgInactive);

            // Inactive
            pal.setColor(QPalette::Inactive, QPalette::Window, windowBg);
            pal.setColor(QPalette::Inactive, QPalette::WindowText, windowFg);
            pal.setColor(QPalette::Inactive, QPalette::Base, viewBg);
            pal.setColor(QPalette::Inactive, QPalette::AlternateBase, viewAlt);
            pal.setColor(QPalette::Inactive, QPalette::Text, viewFg);
            pal.setColor(QPalette::Inactive, QPalette::Button, buttonBg);
            pal.setColor(QPalette::Inactive, QPalette::ButtonText, buttonFg);
            pal.setColor(QPalette::Inactive, QPalette::Highlight, selectBg);
            pal.setColor(QPalette::Inactive, QPalette::HighlightedText,
                         selectFg);

            // Disabled
            pal.setColor(QPalette::Disabled, QPalette::WindowText,
                         windowFgInactive);
            pal.setColor(QPalette::Disabled, QPalette::Text, viewFgInactive);
            pal.setColor(QPalette::Disabled, QPalette::ButtonText,
                         buttonFgInactive);
            pal.setColor(QPalette::Disabled, QPalette::HighlightedText,
                         windowFgInactive);

            return pal;
        }

        QString GenerateBreezeStylesheet(bool dark, const QPalette& pal) {
            const QString winHex = pal.color(QPalette::Window).name();
            const QString winTextHex = pal.color(QPalette::WindowText).name();
            const QString baseHex = pal.color(QPalette::Base).name();
            const QString baseAltHex =
                pal.color(QPalette::AlternateBase).name();
            const QString textHex = pal.color(QPalette::Text).name();
            const QString btnHex = pal.color(QPalette::Button).name();
            const QString btnTextHex = pal.color(QPalette::ButtonText).name();
            const QString highlightHex = pal.color(QPalette::Highlight).name();
            const QString highlightTextHex =
                pal.color(QPalette::HighlightedText).name();
            const QString disabledTextHex =
                pal.color(QPalette::Disabled, QPalette::Text).name();

            const QString borderHex = dark ? "#444b52" : "#c0c6ca";
            const QString subtleBorderHex = dark ? "#363b40" : "#cbd0d4";
            const QString btnHoverHex = dark ? "#353b40" : "#ffffff";
            const QString btnPressedHex = dark ? "#1e2124" : "#e5f1fb";
            const QString headerBgHex = dark ? "#292c30" : "#dee0e2";
            const QString scrollHandleHex = dark ? "#444b52" : "#c0c6ca";

            return QString(R"(
        QMainWindow, QDialog, QWidget#centralWidget {
            background-color: %4;
            color: %2;
        }
        QToolTip {
            background-color: %1;
            color: %2;
            border: 1px solid %3;
            padding: 4px 6px;
            border-radius: 4px;
        }
        QToolBar {
            background-color: %4;
            border-bottom: 1px solid %5;
            padding: 3px;
            spacing: 4px;
        }
        QToolBar::separator {
            background-color: %5;
            width: 1px;
            margin: 4px 6px;
        }
        QToolButton {
            background-color: transparent;
            border: 1px solid transparent;
            border-radius: 4px;
            padding: 4px 6px;
            color: %6;
        }
        QToolButton:hover {
            background-color: rgba(61, 174, 233, 0.2);
            border: 1px solid rgba(61, 174, 233, 0.45);
        }
        QToolButton:pressed {
            background-color: rgba(61, 174, 233, 0.35);
            border: 1px solid %7;
        }
        QFrame#locationBarContainer {
            background-color: %12;
            border: 1px solid %3;
            border-radius: 4px;
        }
        QFrame#locationBarContainer:hover,
        QFrame#locationBarContainer[editing="true"] {
            border: 1px solid %7;
        }
        QToolButton[isBreadcrumbSegment="true"] {
            border: none;
            border-radius: 3px;
            background: transparent;
            color: %6;
            padding: 2px 6px;
            font-size: 12px;
        }
        QToolButton[isBreadcrumbSegment="true"]:hover {
            background-color: rgba(61, 174, 233, 0.2);
        }
        QToolButton[isBreadcrumbSegment="true"]:pressed {
            background-color: rgba(61, 174, 233, 0.35);
        }
        QToolButton[isBreadcrumbArrow="true"] {
            border: none;
            border-radius: 3px;
            background: transparent;
            padding: 0px;
        }
        QToolButton[isBreadcrumbArrow="true"]:hover {
            background-color: rgba(61, 174, 233, 0.2);
        }
        QToolButton[isBreadcrumbArrow="true"]:pressed {
            background-color: rgba(61, 174, 233, 0.35);
        }
        QLineEdit#locationPathEdit {
            border: none;
            background: transparent;
            color: %6;
            padding: 1px 4px;
            font-size: 12px;
            selection-background-color: %7;
            selection-color: %13;
        }
        QScrollArea, QScrollArea #qt_scrollarea_viewport, QScrollArea > QWidget > QWidget {
            background: transparent;
            border: none;
        }
        QPushButton {
            background-color: %1;
            color: %8;
            border: 1px solid %3;
            border-radius: 4px;
            padding: 4px 14px;
            min-height: 20px;
        }
        QPushButton:hover {
            background-color: %9;
            border: 1px solid %7;
        }
        QPushButton:pressed {
            background-color: %10;
            border: 1px solid %7;
        }
        QPushButton:disabled {
            background-color: %4;
            color: %11;
            border: 1px solid %5;
        }
        QPushButton[flat="true"] {
            background: transparent;
            border: 1px solid transparent;
            padding: 2px 6px;
        }
        QPushButton[flat="true"]:hover {
            background-color: rgba(61, 174, 233, 0.2);
            border-radius: 4px;
        }
        QLineEdit, QTextEdit, QPlainTextEdit {
            background-color: %12;
            color: %6;
            border: 1px solid %3;
            border-radius: 4px;
            padding: 3px 6px;
            selection-background-color: %7;
            selection-color: %13;
        }
        QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus {
            border: 1px solid %7;
        }
        QComboBox {
            background-color: %1;
            color: %8;
            border: 1px solid %3;
            border-radius: 4px;
            padding: 3px 8px;
            min-height: 20px;
        }
        QComboBox:hover {
            border: 1px solid %7;
        }
        QComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 20px;
            border-left: none;
        }
        QComboBox QAbstractItemView {
            background-color: %12;
            color: %6;
            selection-background-color: %7;
            selection-color: %13;
            border: 1px solid %3;
            border-radius: 4px;
        }
        QMenuBar {
            background-color: %4;
            color: %2;
            border-bottom: 1px solid %5;
        }
        QMenuBar::item {
            padding: 4px 8px;
            background: transparent;
            border-radius: 4px;
        }
        QMenuBar::item:selected {
            background-color: rgba(61, 174, 233, 0.25);
            color: %2;
        }
        QMenu {
            background-color: %1;
            color: %8;
            border: 1px solid %3;
            border-radius: 4px;
            padding: 4px;
        }
        QMenu::item {
            padding: 5px 24px 5px 24px;
            border-radius: 3px;
        }
        QMenu::item:selected {
            background-color: %7;
            color: %13;
        }
        QMenu::separator {
            height: 1px;
            background-color: %5;
            margin: 3px 6px;
        }
        QHeaderView::section {
            background-color: %14;
            color: %2;
            border: none;
            border-right: 1px solid %5;
            border-bottom: 1px solid %5;
            padding: 4px 8px;
            font-weight: 500;
        }
        QHeaderView::section:hover {
            background-color: %9;
        }
        QTreeView, QTableView, QListView {
            background-color: %12;
            alternate-background-color: %15;
            color: %6;
            border: 1px solid %5;
            selection-background-color: %7;
            selection-color: %13;
            border-radius: 4px;
        }
        QTreeView::item, QTableView::item, QListView::item {
            padding: 3px;
        }
        QTreeView::item:hover, QTableView::item:hover, QListView::item:hover {
            background-color: rgba(61, 174, 233, 0.15);
        }
        QTreeView::item:selected, QTableView::item:selected, QListView::item:selected {
            background-color: %7;
            color: %13;
        }
        QStatusBar {
            background-color: %4;
            color: %2;
            border-top: 1px solid %5;
        }
        QScrollBar:vertical {
            background: %4;
            width: 12px;
            margin: 0;
            border: none;
        }
        QScrollBar::handle:vertical {
            background: %16;
            min-height: 24px;
            border-radius: 4px;
            margin: 2px;
        }
        QScrollBar::handle:vertical:hover {
            background: %7;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
            background: none;
        }
        QScrollBar::horizontal {
            background: %4;
            height: 12px;
            margin: 0;
            border: none;
        }
        QScrollBar::handle:horizontal {
            background: %16;
            min-width: 24px;
            border-radius: 4px;
            margin: 2px;
        }
        QScrollBar::handle:horizontal:hover {
            background: %7;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0px;
            background: none;
        }
        QTabWidget::pane {
            border: 1px solid %3;
            border-radius: 4px;
        }
        QTabBar::tab {
            background-color: %4;
            color: %2;
            border: 1px solid %3;
            border-bottom: none;
            padding: 5px 12px;
            border-top-left-radius: 4px;
            border-top-right-radius: 4px;
            margin-right: 2px;
        }
        QTabBar::tab:selected {
            background-color: %1;
            border-bottom: 2px solid %7;
        }
        QProgressBar {
            background-color: %12;
            border: 1px solid %3;
            border-radius: 4px;
            text-align: center;
            color: %6;
        }
        QProgressBar::chunk {
            background-color: %7;
            border-radius: 3px;
        }
        QGroupBox {
            border: 1px solid %3;
            border-radius: 4px;
            margin-top: 10px;
            padding-top: 10px;
            font-weight: 500;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 10px;
            padding: 0 4px;
        }
    )")
                .arg(btnHex)           // %1
                .arg(winTextHex)       // %2
                .arg(borderHex)        // %3
                .arg(winHex)           // %4
                .arg(subtleBorderHex)  // %5
                .arg(textHex)          // %6
                .arg(highlightHex)     // %7
                .arg(btnTextHex)       // %8
                .arg(btnHoverHex)      // %9
                .arg(btnPressedHex)    // %10
                .arg(disabledTextHex)  // %11
                .arg(baseHex)          // %12
                .arg(highlightTextHex) // %13
                .arg(headerBgHex)      // %14
                .arg(baseAltHex)       // %15
                .arg(scrollHandleHex); // %16
        }

    } // namespace

    ThemeManager& ThemeManager::GetInstance() {
        static ThemeManager instance;
        return instance;
    }

    ThemeManager::ThemeManager() {
        if (qApp) {
            m_originalPalette = qApp->palette();
            m_originalStyleName = qApp->style()->objectName();
            m_originalIconTheme = QIcon::themeName();
        }

        // Register embedded icons in Qt's icon theme search paths
        QStringList paths = QIcon::themeSearchPaths();
        if (!paths.contains(":/icons")) {
            paths.prepend(":/icons");
            QIcon::setThemeSearchPaths(paths);
        }
    }

    QString ThemeManager::ThemeToString(AppTheme theme) {
        switch (theme) {
        case AppTheme::BreezeLight:
            return "BreezeLight";
        case AppTheme::BreezeDark:
            return "BreezeDark";
        case AppTheme::System:
        default:
            return "System";
        }
    }

    AppTheme ThemeManager::ThemeFromString(const QString& str) {
        if (str.compare("BreezeLight", Qt::CaseInsensitive) == 0 ||
            str.compare("Light", Qt::CaseInsensitive) == 0 ||
            str.compare("Breeze-Light", Qt::CaseInsensitive) == 0) {
            return AppTheme::BreezeLight;
        }
        if (str.compare("BreezeDark", Qt::CaseInsensitive) == 0 ||
            str.compare("Dark", Qt::CaseInsensitive) == 0 ||
            str.compare("Breeze-Dark", Qt::CaseInsensitive) == 0) {
            return AppTheme::BreezeDark;
        }
        return AppTheme::System;
    }

    void ThemeManager::ApplyTheme(const QString& themeName) {
        ApplyTheme(ThemeFromString(themeName));
    }

    void ThemeManager::ApplyTheme(AppTheme theme) {
        if (!qApp)
            return;

        if (!m_initialized) {
            m_originalPalette = qApp->palette();
            m_originalStyleName = qApp->style()->objectName();
            m_originalIconTheme = QIcon::themeName();
            m_initialized = true;
        }

        m_currentTheme = theme;

        switch (theme) {
        case AppTheme::BreezeLight:
            SetBreezeTheme(false);
            break;
        case AppTheme::BreezeDark:
            SetBreezeTheme(true);
            break;
        case AppTheme::System:
        default:
            SetSystemTheme();
            break;
        }

        emit ThemeChanged(theme);
    }

    void ThemeManager::SetSystemTheme() {
        qApp->setStyleSheet(QString());
        qApp->setProperty("KDE_COLOR_SCHEME_PATH", QVariant());
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        if (auto* hints = QGuiApplication::styleHints()) {
            hints->setColorScheme(Qt::ColorScheme::Unknown);
        }
#endif
        if (!m_originalStyleName.isEmpty()) {
            const QString currentStyleName =
                qApp->style() ? qApp->style()->objectName() : QString();
            if (currentStyleName.compare(m_originalStyleName,
                                         Qt::CaseInsensitive) != 0) {
                if (auto* style = QStyleFactory::create(m_originalStyleName)) {
                    qApp->setStyle(style);
                }
            }
        }
        qApp->setPalette(m_originalPalette);
        if (!m_originalIconTheme.isEmpty()) {
            QIcon::setThemeName(m_originalIconTheme);
        }
    }

    void ThemeManager::SetBreezeTheme(bool dark) {
        const QString themeName = dark ? "breeze-dark" : "breeze";
        QIcon::setThemeName(themeName);

        const QString colorsPath =
            dark ? ":/themes/BreezeDark.colors" : ":/themes/BreezeLight.colors";
        const QPalette palette = LoadPaletteFromColorScheme(colorsPath, dark);

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        if (auto* hints = QGuiApplication::styleHints()) {
            hints->setColorScheme(dark ? Qt::ColorScheme::Dark
                                       : Qt::ColorScheme::Light);
        }
#endif

        const QString sysScheme =
            dark
                ? QStringLiteral("/usr/share/color-schemes/BreezeDark.colors")
                : QStringLiteral("/usr/share/color-schemes/BreezeLight.colors");
        if (QFile::exists(sysScheme)) {
            qApp->setProperty("KDE_COLOR_SCHEME_PATH", sysScheme);
        } else {
            qApp->setProperty("KDE_COLOR_SCHEME_PATH",
                              dark ? "BreezeDark" : "BreezeLight");
        }

        const QString currentStyleName =
            qApp->style() ? qApp->style()->objectName() : QString();
        if (currentStyleName.compare("fusion", Qt::CaseInsensitive) != 0) {
            if (auto* fusion = QStyleFactory::create("Fusion")) {
                qApp->setStyle(fusion);
            }
        }
        qApp->setPalette(palette);
        qApp->setStyleSheet(GenerateBreezeStylesheet(dark, palette));
    }

} // namespace BFView
