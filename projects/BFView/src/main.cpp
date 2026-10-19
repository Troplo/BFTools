//
// Created by troplo on 09/21/26.
//

#include "BFView/MainWindow.hpp"
#include "BFView/Platform/FileAssociation.hpp"
#include "BFView/Settings/SettingsManager.hpp"
#include "BFView/Theme/ThemeManager.hpp"
#include "Nomad/Archives/MemoryManager.hpp"
#include <QApplication>
#include <cstdio>
#include <tracy/Tracy.hpp>

int main(int argc, char* argv[]) {
    ZoneScopedN("BFView::Main");
    Nexus::MemoryManager::SetReady(true);
    // Quick CLI help check
    for (int i = 1; i < argc; ++i) {
        QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == "-h" || arg == "--help") {
            std::printf(
                "Usage: BFView [options] [archive_path]\n\n"
                "Options:\n"
                "  -h, --help                 Display this help information\n"
                "  -p, --project <name>       Specify project name\n"
                "  --project=<name>           Specify project name\n");
            return 0;
        }
    }

    QApplication app(argc, argv);
    app.setApplicationName("BFView");
    app.setOrganizationName("Troplo");
    app.setWindowIcon(QIcon(":/icons/icon.png"));

    // Initialize Settings and apply saved Theme
    auto& settings = BFView::SettingsManager::GetInstance();
    BFView::ThemeManager::GetInstance().ApplyTheme(settings.GetAppTheme());

    BFView::MainWindow window;
    window.show();

    QString openPath;
    QString projectName;
    for (int i = 1; i < argc; ++i) {
        QString arg = QString::fromLocal8Bit(argv[i]);

        if (arg == "-p" || arg == "--project") {
            if (i + 1 < argc) {
                projectName = QString::fromLocal8Bit(argv[++i]);
            }
        } else if (arg.startsWith("--project=")) {
            projectName = arg.mid(10);
        } else if (arg.startsWith("-p") && arg.size() > 2) {
            projectName = arg.mid(2);
        } else if (!arg.startsWith("-")) {
            openPath = arg;
        }
    }

    if (!openPath.isEmpty()) {
        window.OpenArchiveFile(openPath, projectName);
    }

    return QApplication::exec();
}
