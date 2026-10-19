//
// Created by troplo on 09/21/26.
//

#include "BFView/Handlers/FileHandlerManager.hpp"
#include "BFView/Handlers/XbtFileHandler.hpp"
#include "BFView/Settings/SettingsManager.hpp"
#include "Nomad/Archives/Extractor.hpp"
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QUrl>
#include <filesystem>
#include <fstream>

namespace BFView {
    FileHandlerManager& FileHandlerManager::GetInstance() {
        static FileHandlerManager instance;
        return instance;
    }

    FileHandlerManager::FileHandlerManager() {
        RegisterHandler(std::make_unique<XbtFileHandler>());
    }

    void
    FileHandlerManager::RegisterHandler(std::unique_ptr<IFileHandler> handler) {
        if (handler) {
            m_handlers.push_back(std::move(handler));
        }
    }

    IFileHandler*
    FileHandlerManager::FindHandler(const QString& fileName,
                                    const QByteArray& fileData) const {
        for (const auto& handler : m_handlers) {
            if (handler->CanHandle(fileName, fileData)) {
                return handler.get();
            }
        }
        return nullptr;
    }

    bool FileHandlerManager::OpenFile(
        const QString& fatPath, const Nomad::Archives::ArchiveEntry& entry,
        bool bypassCustomHandler, bool openWithChooser) {
        std::filesystem::path datPath = fatPath.toStdString();
        datPath.replace_extension(".dat");

        std::ifstream datStream(datPath, std::ios::binary);
        if (!datStream.is_open()) {
            qWarning() << "Failed to open DAT stream for:" << fatPath;
            return false;
        }

        QString tempDir = SettingsManager::GetInstance().GetTempExtractPath();
        QDir().mkpath(tempDir);

        QFileInfo entryInfo(QString::fromStdString(entry.path));
        QString fileName = entryInfo.fileName();
        if (fileName.isEmpty()) {
            fileName = "extracted_file";
        }

        std::filesystem::path rawOutFile =
            std::filesystem::path(tempDir.toStdString()) /
            fileName.toStdString();
        if (!Nomad::Archives::Extractor::ExtractEntry(entry, datStream,
                                                      rawOutFile)) {
            qWarning() << "Failed to extract entry to temp path:"
                       << QString::fromStdString(rawOutFile.string());
            return false;
        }

        QString rawPathStr = QString::fromStdString(rawOutFile.string());
        QFile rawFile(rawPathStr);
        if (!rawFile.open(QIODevice::ReadOnly)) {
            qWarning() << "Failed to read extracted file:" << rawPathStr;
            return false;
        }

        QByteArray fileData = rawFile.readAll();
        rawFile.close();

        bool useCustom =
            SettingsManager::GetInstance().GetUseCustomHandlers() &&
            !bypassCustomHandler;
        if (useCustom) {
            IFileHandler* handler = FindHandler(fileName, fileData);
            if (handler) {
                return handler->OpenFile(fileName, fileData, tempDir);
            }
        }

        if (openWithChooser) {
#if defined(_WIN32)
            bool launched = QProcess::startDetached(
                "rundll32.exe", {"shell32.dll,OpenAs_RunDLL", rawPathStr});
            if (!launched) {
                launched =
                    QDesktopServices::openUrl(QUrl::fromLocalFile(rawPathStr));
            }
            return launched;
#else
            // Try gio open --chooser or system open chooser
            bool launched = QProcess::startDetached(
                "gio", {"open", "--chooser", rawPathStr});
            if (!launched) {
                launched =
                    QDesktopServices::openUrl(QUrl::fromLocalFile(rawPathStr));
            }
            return launched;
#endif
        }

        return QDesktopServices::openUrl(QUrl::fromLocalFile(rawPathStr));
    }

} // namespace BFView
