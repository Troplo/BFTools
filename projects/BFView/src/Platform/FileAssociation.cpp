//
// Created by troplo on 10/17/26.
//

#include "BFView/Platform/FileAssociation.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QProcess>
#include <QStandardPaths>
#include <QTextStream>

#if defined(_WIN32) || defined(__WIN32__)
#include <shlobj.h>
#include <windows.h>

namespace {

    bool SetRegSz(HKEY rootKey, const std::wstring& subKey,
                  const std::wstring& valueName, const std::wstring& data) {
        HKEY hKey = nullptr;
        LONG res = RegCreateKeyExW(rootKey, subKey.c_str(), 0, nullptr,
                                   REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr,
                                   &hKey, nullptr);
        if (res != ERROR_SUCCESS)
            return false;

        res = RegSetValueExW(
            hKey, valueName.empty() ? nullptr : valueName.c_str(), 0, REG_SZ,
            reinterpret_cast<const BYTE*>(data.c_str()),
            static_cast<DWORD>((data.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);
        return res == ERROR_SUCCESS;
    }

    std::wstring GetRegSz(HKEY rootKey, const std::wstring& subKey,
                          const std::wstring& valueName) {
        HKEY hKey = nullptr;
        LONG res = RegOpenKeyExW(rootKey, subKey.c_str(), 0, KEY_READ, &hKey);
        if (res != ERROR_SUCCESS)
            return {};

        wchar_t buf[2048] = {0};
        DWORD bufSize = sizeof(buf);
        DWORD type = REG_SZ;
        res = RegQueryValueExW(
            hKey, valueName.empty() ? nullptr : valueName.c_str(), nullptr,
            &type, reinterpret_cast<BYTE*>(buf), &bufSize);
        RegCloseKey(hKey);
        if (res == ERROR_SUCCESS && type == REG_SZ) {
            return std::wstring(buf);
        }
        return {};
    }

} // namespace
#endif

namespace BFView::Platform {
    bool FileAssociation::RegisterFatAssociation(bool overrideIfDifferent) {
#if defined(_WIN32) || defined(__WIN32__)
        QString exePath =
            QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        std::wstring wExePath = exePath.toStdWString();
        std::wstring cmd = L"\"" + wExePath + L"\" \"%1\"";
        std::wstring icon = L"\"" + wExePath + L"\",0";

        if (!overrideIfDifferent && IsFatAssociated()) {
            return true;
        }

        // 1. Register .fat extension to BFView.FAT ProgID
        SetRegSz(HKEY_CURRENT_USER, L"Software\\Classes\\.fat", L"",
                 L"BFView.FAT");
        SetRegSz(HKEY_CURRENT_USER, L"Software\\Classes\\.fat", L"Content Type",
                 L"application/x-fat-archive");
        SetRegSz(HKEY_CURRENT_USER, L"Software\\Classes\\.fat\\OpenWithProgids",
                 L"BFView.FAT", L"");

        // 2. Register BFView.FAT ProgID description, icon and launch command
        SetRegSz(HKEY_CURRENT_USER, L"Software\\Classes\\BFView.FAT", L"",
                 L"Nomad/Dunia Archive");
        SetRegSz(HKEY_CURRENT_USER,
                 L"Software\\Classes\\BFView.FAT\\DefaultIcon", L"", icon);
        SetRegSz(HKEY_CURRENT_USER,
                 L"Software\\Classes\\BFView.FAT\\shell\\open\\command", L"",
                 cmd);

        // 3. Register Applications\BFView.exe for Windows Open With list
        SetRegSz(HKEY_CURRENT_USER,
                 L"Software\\Classes\\Applications\\BFView.exe",
                 L"FriendlyAppName", L"BFView");
        SetRegSz(HKEY_CURRENT_USER,
                 L"Software\\Classes\\Applications\\BFView.exe\\DefaultIcon",
                 L"", icon);
        SetRegSz(HKEY_CURRENT_USER,
                 L"Software\\Classes\\Applications\\BFView.exe\\SupportedTypes",
                 L".fat", L"");
        SetRegSz(HKEY_CURRENT_USER,
                 L"Software\\Classes\\Applications\\BFView."
                 L"exe\\shell\\open\\command",
                 L"", cmd);

        // Refresh Windows Explorer shell caches immediately
        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
        return true;
#else
        if (!overrideIfDifferent && IsFatAssociated()) {
            return true;
        }
        QString exePath = QCoreApplication::applicationFilePath();
        QString appsDir = QStandardPaths::writableLocation(
            QStandardPaths::ApplicationsLocation);
        QDir().mkpath(appsDir);
        QString desktopPath = appsDir + "/bfview.desktop";

        QString genericDataDir = QStandardPaths::writableLocation(
            QStandardPaths::GenericDataLocation);
        QString iconsBaseDir = genericDataDir + "/icons";
        QDir().mkpath(iconsBaseDir);

        // Export icon to user icon theme for application and file MIME types
        QImage iconImg(":/icons/icon.png");
        if (!iconImg.isNull()) {
            iconImg.save(iconsBaseDir + "/bfview.png", "PNG");
            iconImg.save(iconsBaseDir + "/application-x-fat-archive.png",
                         "PNG");
            iconImg.save(iconsBaseDir + "/application-x-bf-fat.png", "PNG");

            const int sizes[] = {16, 24, 32, 48, 64, 128, 256, 512};
            for (int s : sizes) {
                QString appDir =
                    QString("%1/hicolor/%2x%2/apps").arg(iconsBaseDir).arg(s);
                QString mimeDir = QString("%1/hicolor/%2x%2/mimetypes")
                                      .arg(iconsBaseDir)
                                      .arg(s);
                QDir().mkpath(appDir);
                QDir().mkpath(mimeDir);

                QImage scaled = iconImg.scaled(s, s, Qt::IgnoreAspectRatio,
                                               Qt::SmoothTransformation);
                scaled.save(appDir + "/bfview.png", "PNG");
                scaled.save(mimeDir + "/application-x-fat-archive.png", "PNG");
                scaled.save(mimeDir + "/application-x-bf-fat.png", "PNG");
            }
            QProcess::execute("gtk-update-icon-cache",
                              {"-f", "-t", iconsBaseDir + "/hicolor"});
        }

        // Write desktop entry
        QFile desktopFile(desktopPath);
        if (desktopFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&desktopFile);
            out << "[Desktop Entry]\n";
            out << "Type=Application\n";
            out << "Name=BFView\n";
            out << "GenericName=BigFile Archive Viewer\n";
            out << "Comment=Nomad BigFile Archive Viewer\n";
            out << "Exec=\"" << exePath << "\" %f\n";
            out << "Icon=bfview\n";
            out << "Terminal=false\n";
            out << "Categories=Utility;Development;\n";
            out << "MimeType=application/x-fat-archive;application/x-bf-fat;\n";
            desktopFile.close();
        }

        // Write custom MIME xml definition for .fat
        QString mimePackagesDir = genericDataDir + "/mime/packages";
        QDir().mkpath(mimePackagesDir);
        QString mimeXmlPath = mimePackagesDir + "/bfview.xml";
        QFile mimeFile(mimeXmlPath);
        if (mimeFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&mimeFile);
            out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
            out << "<mime-info "
                   "xmlns=\"http://www.freedesktop.org/standards/"
                   "shared-mime-info\">\n";
            out << "  <mime-type type=\"application/x-fat-archive\">\n";
            out << "    <comment>Nomad BigFile FAT Archive</comment>\n";
            out << "    <glob pattern=\"*.fat\"/>\n";
            out << "    <icon name=\"application-x-fat-archive\"/>\n";
            out << "    <generic-icon name=\"package-x-generic\"/>\n";
            out << "  </mime-type>\n";
            out << "  <mime-type type=\"application/x-bf-fat\">\n";
            out << "    <comment>Nomad BigFile FAT Archive</comment>\n";
            out << "    <glob pattern=\"*.fat\"/>\n";
            out << "    <icon name=\"application-x-fat-archive\"/>\n";
            out << "    <generic-icon name=\"package-x-generic\"/>\n";
            out << "  </mime-type>\n";
            out << "</mime-info>\n";
            mimeFile.close();

            QProcess::execute("update-mime-database",
                              {genericDataDir + "/mime"});
        }

        // Set default handler via xdg-mime
        QProcess::execute("xdg-mime", {"default", "bfview.desktop",
                                       "application/x-fat-archive"});
        QProcess::execute(
            "xdg-mime", {"default", "bfview.desktop", "application/x-bf-fat"});

        return true;
#endif
    }

    bool FileAssociation::IsFatAssociated() {
#if defined(_WIN32) || defined(__WIN32__)
        QString exePath =
            QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        std::wstring expectedCmd =
            L"\"" + exePath.toStdWString() + L"\" \"%1\"";
        std::wstring actualCmd = GetRegSz(
            HKEY_CURRENT_USER,
            L"Software\\Classes\\BFView.FAT\\shell\\open\\command", L"");
        if (actualCmd != expectedCmd) {
            return false;
        }

        std::wstring extProgId =
            GetRegSz(HKEY_CURRENT_USER, L"Software\\Classes\\.fat", L"");
        if (extProgId != L"BFView.FAT") {
            return false;
        }

        std::wstring userChoice = GetRegSz(
            HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileEx"
            L"ts\\.fat\\UserChoice",
            L"ProgId");
        if (!userChoice.empty() && userChoice != L"BFView.FAT") {
            return false;
        }

        return true;
#else
        QString appsDir = QStandardPaths::writableLocation(
            QStandardPaths::ApplicationsLocation);
        QString desktopPath = appsDir + "/bfview.desktop";
        if (!QFile::exists(desktopPath)) {
            return false;
        }
        QFile f(desktopPath);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString content = QString::fromUtf8(f.readAll());
            return content.contains(QCoreApplication::applicationFilePath());
        }
        return false;
#endif
    }

} // namespace BFView::Platform
