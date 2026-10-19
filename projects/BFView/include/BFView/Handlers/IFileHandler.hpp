//
// Created by troplo on 09/21/26.
//

#ifndef BFVIEW_IFILEHANDLER_H
#define BFVIEW_IFILEHANDLER_H

#include <QByteArray>
#include <QString>

namespace BFView {
    class IFileHandler {
      public:
        virtual ~IFileHandler() = default;

        virtual QString GetName() const = 0;
        virtual bool CanHandle(const QString& fileName,
                               const QByteArray& fileData) const = 0;
        virtual bool OpenFile(const QString& fileName,
                              const QByteArray& fileData,
                              const QString& tempDir) = 0;
    };

} // namespace BFView

#endif // BFVIEW_IFILEHANDLER_H
