//
// Created by troplo on 09/21/26.
//

#include "BFView/Handlers/XbtFileHandler.hpp"
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUrl>

namespace BFView {
    bool XbtFileHandler::CanHandle(const QString& fileName,
                                   const QByteArray& fileData) const {
        if (fileName.endsWith(".xbt", Qt::CaseInsensitive)) {
            return true;
        }
        // Check XBT magic "TBX\0" (54 42 58 00)
        if (fileData.size() >= 4 &&
            static_cast<unsigned char>(fileData[0]) == 0x54 &&
            static_cast<unsigned char>(fileData[1]) == 0x42 &&
            static_cast<unsigned char>(fileData[2]) == 0x58 &&
            static_cast<unsigned char>(fileData[3]) == 0x00) {
            return true;
        }
        return false;
    }

    bool XbtFileHandler::OpenFile(const QString& fileName,
                                  const QByteArray& fileData,
                                  const QString& tempDir) {
        QDir().mkpath(tempDir);

        QFileInfo fileInfo(fileName);
        QString baseName = fileInfo.completeBaseName();
        if (baseName.isEmpty()) {
            baseName = "texture";
        }

        QString ddsPath = tempDir + "/" + baseName + ".dds";

        QByteArray ddsData;
        constexpr int xbtHeaderSize = 44;

        if (fileData.size() > xbtHeaderSize &&
            static_cast<unsigned char>(fileData[0]) == 0x54 &&
            static_cast<unsigned char>(fileData[1]) == 0x42 &&
            static_cast<unsigned char>(fileData[2]) == 0x58 &&
            static_cast<unsigned char>(fileData[3]) == 0x00) {
            // Strip 44-byte XBT header
            ddsData = fileData.mid(xbtHeaderSize);
        } else {
            // Fallback to full data if header check fails
            ddsData = fileData;
        }

        QFile ddsFile(ddsPath);
        if (!ddsFile.open(QIODevice::WriteOnly)) {
            qWarning() << "Failed to write temporary DDS file:" << ddsPath;
            return false;
        }

        ddsFile.write(ddsData);
        ddsFile.close();

        return QDesktopServices::openUrl(QUrl::fromLocalFile(ddsPath));
    }

} // namespace BFView
