//
// Created by troplo on 09/21/26.
//

#ifndef BFVIEW_XBTFILEHANDLER_H
#define BFVIEW_XBTFILEHANDLER_H

#include "BFView/Handlers/IFileHandler.hpp"

namespace BFView {
    class XbtFileHandler : public IFileHandler {
      public:
        XbtFileHandler() = default;
        ~XbtFileHandler() override = default;

        QString GetName() const override { return "XBT Texture Handler"; }
        bool CanHandle(const QString& fileName,
                       const QByteArray& fileData) const override;
        bool OpenFile(const QString& fileName, const QByteArray& fileData,
                      const QString& tempDir) override;
    };

} // namespace BFView

#endif // BFVIEW_XBTFILEHANDLER_H
