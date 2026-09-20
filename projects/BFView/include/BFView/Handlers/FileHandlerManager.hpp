//
// Created by troplo on 09/21/26.
//

#ifndef BFVIEW_FILEHANDLERMANAGER_H
#define BFVIEW_FILEHANDLERMANAGER_H

#include "BFView/Handlers/IFileHandler.hpp"
#include "Nomad/Archives/IArchive.hpp"
#include <QByteArray>
#include <QString>
#include <memory>
#include <vector>

namespace BFView {
    class FileHandlerManager {
      public:
        static FileHandlerManager &GetInstance();

        void RegisterHandler(std::unique_ptr<IFileHandler> handler);
        IFileHandler *FindHandler(const QString &fileName,
                                  const QByteArray &fileData) const;

        bool OpenFile(const QString &fatPath,
                      const Nomad::Archives::ArchiveEntry &entry,
                      bool bypassCustomHandler = false,
                      bool openWithChooser = false);

      private:
        FileHandlerManager();
        ~FileHandlerManager() = default;

        std::vector<std::unique_ptr<IFileHandler>> m_handlers;
    };

} // namespace BFView

#endif // BFVIEW_FILEHANDLERMANAGER_H
