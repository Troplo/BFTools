//
// Created by troplo on 09/21/26.
//

#include "BFView/Views/LocationBreadcrumb.hpp"
#include "BFView/Theme/ThemeManager.hpp"
#include <QAction>
#include <QEvent>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QScrollBar>
#include <QSizePolicy>
#include <QStyle>
#include <QTimer>
#include <QToolButton>

namespace BFView::Views {
    LocationBreadcrumb::LocationBreadcrumb(QWidget* parent) : QWidget(parent) {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setFixedHeight(32);
        SetupUi();

        connect(&ThemeManager::GetInstance(), &ThemeManager::ThemeChanged, this,
                [this](AppTheme theme) {
                    if (theme == AppTheme::System) {
                        setStyleSheet(QStringLiteral(
                            "QFrame#locationBarContainer {"
                            "  background-color: palette(base);"
                            "  border: 1px solid palette(mid);"
                            "  border-radius: 4px;"
                            "}"
                            "QFrame#locationBarContainer:hover,"
                            "QFrame#locationBarContainer[editing=\"true\"] {"
                            "  border: 1px solid palette(highlight);"
                            "}"
                            "QToolButton[isBreadcrumbSegment=\"true\"], "
                            "QToolButton[isBreadcrumbArrow=\"true\"] {"
                            "  border: none;"
                            "  border-radius: 3px;"
                            "  background: transparent;"
                            "  color: palette(text);"
                            "  padding: 2px 6px;"
                            "  font-size: 12px;"
                            "}"
                            "QLineEdit#locationPathEdit {"
                            "  border: none;"
                            "  background: transparent;"
                            "  color: palette(text);"
                            "  padding: 1px 4px;"
                            "  font-size: 12px;"
                            "}"
                            "QScrollArea {"
                            "  background: transparent;"
                            "  border: none;"
                            "}"));
                    } else {
                        setStyleSheet(QString());
                    }
                    UpdateIcons();
                });
    }

    void LocationBreadcrumb::SetupUi() {
        auto* outerLayout = new QHBoxLayout(this);
        outerLayout->setContentsMargins(0, 0, 0, 0);
        outerLayout->setSpacing(0);

        m_containerFrame = new QFrame(this);
        m_containerFrame->setObjectName(QStringLiteral("locationBarContainer"));
        m_containerFrame->installEventFilter(this);

        auto* frameLayout = new QHBoxLayout(m_containerFrame);
        frameLayout->setContentsMargins(2, 1, 2, 1);
        frameLayout->setSpacing(0);

        m_stackedWidget = new QStackedWidget(m_containerFrame);

        m_scrollArea = new QScrollArea(m_stackedWidget);
        m_scrollArea->setWidgetResizable(true);
        m_scrollArea->setFrameShape(QFrame::NoFrame);
        m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_scrollArea->installEventFilter(this);
        if (m_scrollArea->viewport()) {
            m_scrollArea->viewport()->installEventFilter(this);
        }

        m_breadcrumbContainer = new QWidget(m_scrollArea);
        m_breadcrumbContainer->installEventFilter(this);
        m_breadcrumbContainer->setAttribute(Qt::WA_TranslucentBackground);

        m_breadcrumbLayout = new QHBoxLayout(m_breadcrumbContainer);
        m_breadcrumbLayout->setContentsMargins(2, 0, 2, 0);
        m_breadcrumbLayout->setSpacing(1);

        m_scrollArea->setWidget(m_breadcrumbContainer);
        m_stackedWidget->addWidget(m_scrollArea);

        m_pathEdit = new QLineEdit(m_stackedWidget);
        m_pathEdit->setObjectName(QStringLiteral("locationPathEdit"));
        m_pathEdit->installEventFilter(this);
        m_stackedWidget->addWidget(m_pathEdit);

        frameLayout->addWidget(m_stackedWidget);
        outerLayout->addWidget(m_containerFrame);

        if (ThemeManager::GetInstance().GetCurrentTheme() == AppTheme::System) {
            setStyleSheet(
                QStringLiteral("QFrame#locationBarContainer {"
                               "  background-color: palette(base);"
                               "  border: 1px solid palette(mid);"
                               "  border-radius: 4px;"
                               "}"
                               "QFrame#locationBarContainer:hover,"
                               "QFrame#locationBarContainer[editing=\"true\"] {"
                               "  border: 1px solid palette(highlight);"
                               "}"
                               "QToolButton[isBreadcrumbSegment=\"true\"], "
                               "QToolButton[isBreadcrumbArrow=\"true\"] {"
                               "  border: none;"
                               "  border-radius: 3px;"
                               "  background: transparent;"
                               "  color: palette(text);"
                               "  padding: 2px 6px;"
                               "  font-size: 12px;"
                               "}"
                               "QLineEdit#locationPathEdit {"
                               "  border: none;"
                               "  background: transparent;"
                               "  color: palette(text);"
                               "  padding: 1px 4px;"
                               "  font-size: 12px;"
                               "}"
                               "QScrollArea {"
                               "  background: transparent;"
                               "  border: none;"
                               "}"));
        }

        connect(m_pathEdit, &QLineEdit::returnPressed, this, [this]() {
            QString text = m_pathEdit->text().trimmed();
            if (!text.startsWith('/')) {
                text.prepend('/');
            }
            SwitchToBreadcrumbs();
            emit PathClicked(text);
        });

        SetPath(QStringLiteral("/"));
    }

    void LocationBreadcrumb::SetSubfolderProvider(SubfolderProvider provider) {
        m_subfolderProvider = std::move(provider);
    }

    QIcon LocationBreadcrumb::GetArrowIcon() const {
        const bool isDark =
            (ThemeManager::GetInstance().GetCurrentTheme() ==
             AppTheme::BreezeDark) ||
            (ThemeManager::GetInstance().GetCurrentTheme() ==
                 AppTheme::System &&
             palette().color(QPalette::WindowText).lightness() > 128);
        const QString fallback =
            isDark
                ? QStringLiteral(":/icons/breeze-dark/actions/22/go-next.svg")
                : QStringLiteral(":/icons/breeze/actions/22/go-next.svg");
        return QIcon::fromTheme(QStringLiteral("go-next"), QIcon(fallback));
    }

    QIcon LocationBreadcrumb::GetFolderIcon() const {
        const bool isDark =
            (ThemeManager::GetInstance().GetCurrentTheme() ==
             AppTheme::BreezeDark) ||
            (ThemeManager::GetInstance().GetCurrentTheme() ==
                 AppTheme::System &&
             palette().color(QPalette::WindowText).lightness() > 128);
        const QString fallback =
            isDark ? QStringLiteral(":/icons/breeze-dark/places/22/folder.svg")
                   : QStringLiteral(":/icons/breeze/places/22/folder.svg");
        return QIcon::fromTheme(QStringLiteral("folder"), QIcon(fallback));
    }

    QIcon LocationBreadcrumb::GetRootIcon() const {
        const bool isDark =
            (ThemeManager::GetInstance().GetCurrentTheme() ==
             AppTheme::BreezeDark) ||
            (ThemeManager::GetInstance().GetCurrentTheme() ==
                 AppTheme::System &&
             palette().color(QPalette::WindowText).lightness() > 128);
        const QString fallback =
            isDark ? QStringLiteral(
                         ":/icons/breeze-dark/places/22/folder-root.svg")
                   : QStringLiteral(":/icons/breeze/places/22/folder-root.svg");
        return QIcon::fromTheme(
            QStringLiteral("folder-root"),
            QIcon::fromTheme(QStringLiteral("drive-harddisk"),
                             QIcon(fallback)));
    }

    void LocationBreadcrumb::UpdateIcons() {
        const QIcon arrowIcon = GetArrowIcon();
        const QIcon folderIcon = GetFolderIcon();
        const QIcon rootIcon = GetRootIcon();

        bool isFirst = true;
        for (int i = 0; i < m_breadcrumbLayout->count(); ++i) {
            QLayoutItem* item = m_breadcrumbLayout->itemAt(i);
            if (!item)
                continue;
            auto* btn = qobject_cast<QToolButton*>(item->widget());
            if (!btn)
                continue;

            if (btn->property("isBreadcrumbArrow").toBool()) {
                btn->setIcon(arrowIcon);
            } else if (btn->property("isBreadcrumbSegment").toBool()) {
                if (isFirst) {
                    btn->setIcon(rootIcon);
                    isFirst = false;
                } else {
                    btn->setIcon(folderIcon);
                }
            }
        }
    }

    void LocationBreadcrumb::SetPath(const QString& path) {
        m_currentPath = path.isEmpty() ? QStringLiteral("/") : path;

        QLayoutItem* child;
        while ((child = m_breadcrumbLayout->takeAt(0)) != nullptr) {
            delete child->widget();
            delete child;
        }

        const QIcon arrowIcon = GetArrowIcon();
        const QIcon folderIcon = GetFolderIcon();
        const QIcon rootIcon = GetRootIcon();

        auto* rootBtn = new QToolButton(m_breadcrumbContainer);
        rootBtn->setProperty("isBreadcrumbSegment", true);
        rootBtn->setIcon(rootIcon);
        rootBtn->setIconSize(QSize(16, 16));
        rootBtn->setText(QStringLiteral("/"));
        rootBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
        rootBtn->setToolTip(QStringLiteral("Root"));
        rootBtn->setAutoRaise(true);
        connect(rootBtn, &QToolButton::clicked, this,
                [this]() { emit PathClicked(QStringLiteral("/")); });
        m_breadcrumbLayout->addWidget(rootBtn);

        QString cleanPath = m_currentPath;
        while (cleanPath.startsWith('/') && cleanPath.length() > 1) {
            cleanPath.remove(0, 1);
        }
        while (cleanPath.endsWith('/') && cleanPath.length() > 1) {
            cleanPath.chop(1);
        }

        QStringList segments;
        if (cleanPath != QStringLiteral("/")) {
            segments = cleanPath.split('/', Qt::SkipEmptyParts);
        }

        QString accumulatedPath = "";

        for (int i = 0; i < segments.size(); ++i) {
            const QString& segment = segments[i];
            accumulatedPath += "/" + segment;
            const QString segmentFullPath = accumulatedPath;

            auto* arrowBtn = new QToolButton(m_breadcrumbContainer);
            arrowBtn->setProperty("isBreadcrumbArrow", true);
            arrowBtn->setIcon(arrowIcon);
            arrowBtn->setIconSize(QSize(10, 10));
            arrowBtn->setAutoRaise(true);
            arrowBtn->setPopupMode(QToolButton::InstantPopup);

            if (m_subfolderProvider) {
                const QString parentPathForSubfolders =
                    (i == 0) ? QStringLiteral("/")
                             : segments.mid(0, i).join('/');
                const QString normalizedParent =
                    parentPathForSubfolders.startsWith('/')
                        ? parentPathForSubfolders
                        : "/" + parentPathForSubfolders;

                const QStringList subfolders =
                    m_subfolderProvider(normalizedParent);
                if (!subfolders.isEmpty()) {
                    auto* menu = new QMenu(arrowBtn);
                    for (const QString& sub : subfolders) {
                        QString targetPath = normalizedParent;
                        if (!targetPath.endsWith('/'))
                            targetPath += '/';
                        targetPath += sub;

                        QAction* act = menu->addAction(folderIcon, sub);
                        if (sub.compare(segment, Qt::CaseInsensitive) == 0) {
                            QFont font = act->font();
                            font.setBold(true);
                            act->setFont(font);
                        }
                        connect(act, &QAction::triggered, this,
                                [this, targetPath]() {
                                    emit PathClicked(targetPath);
                                });
                    }
                    arrowBtn->setMenu(menu);
                }
            }

            m_breadcrumbLayout->addWidget(arrowBtn);

            auto* btn = new QToolButton(m_breadcrumbContainer);
            btn->setProperty("isBreadcrumbSegment", true);
            btn->setIcon(folderIcon);
            btn->setIconSize(QSize(14, 14));
            btn->setText(segment);
            btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
            btn->setAutoRaise(true);
            connect(btn, &QToolButton::clicked, this,
                    [this, segmentFullPath]() {
                        emit PathClicked(segmentFullPath);
                    });
            m_breadcrumbLayout->addWidget(btn);
        }

        m_breadcrumbLayout->addStretch(1);

        m_pathEdit->setText(m_currentPath);

        QTimer::singleShot(10, this, [this]() {
            if (m_scrollArea && m_scrollArea->horizontalScrollBar()) {
                m_scrollArea->horizontalScrollBar()->setValue(
                    m_scrollArea->horizontalScrollBar()->maximum());
            }
        });
    }

    void LocationBreadcrumb::FocusLocationInput() { SwitchToEditablePath(); }

    void LocationBreadcrumb::SwitchToEditablePath() {
        m_stackedWidget->setCurrentWidget(m_pathEdit);
        m_containerFrame->setProperty("editing", true);
        m_containerFrame->style()->unpolish(m_containerFrame);
        m_containerFrame->style()->polish(m_containerFrame);
        m_pathEdit->selectAll();
        m_pathEdit->setFocus();
    }

    void LocationBreadcrumb::SwitchToBreadcrumbs() {
        m_stackedWidget->setCurrentWidget(m_scrollArea);
        m_containerFrame->setProperty("editing", false);
        m_containerFrame->style()->unpolish(m_containerFrame);
        m_containerFrame->style()->polish(m_containerFrame);
    }

    bool LocationBreadcrumb::eventFilter(QObject* watched, QEvent* event) {
        if (watched == m_containerFrame || watched == m_scrollArea ||
            watched == m_breadcrumbContainer ||
            (m_scrollArea && watched == m_scrollArea->viewport())) {
            if (event->type() == QEvent::MouseButtonPress) {
                auto* mouseEvent = static_cast<QMouseEvent*>(event);
                if (mouseEvent->button() == Qt::LeftButton) {
                    QWidget* child = childAt(
                        mapFromGlobal(mouseEvent->globalPosition().toPoint()));
                    if (!child || !qobject_cast<QToolButton*>(child)) {
                        SwitchToEditablePath();
                        return true;
                    }
                }
            }
        } else if (watched == m_pathEdit && event->type() == QEvent::KeyPress) {
            auto* keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->key() == Qt::Key_Escape) {
                SwitchToBreadcrumbs();
                return true;
            }
        } else if (watched == m_pathEdit && event->type() == QEvent::FocusOut) {
            SwitchToBreadcrumbs();
        }
        return QWidget::eventFilter(watched, event);
    }

    void LocationBreadcrumb::changeEvent(QEvent* event) {
        if (event->type() == QEvent::PaletteChange ||
            event->type() == QEvent::ThemeChange) {
            UpdateIcons();
        }
        QWidget::changeEvent(event);
    }

} // namespace BFView::Views
