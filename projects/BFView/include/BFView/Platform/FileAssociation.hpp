//
// Created by troplo on 10/17/26.
//

#ifndef BFVIEW_FILEASSOCIATION_H
#define BFVIEW_FILEASSOCIATION_H

#include <QString>

namespace BFView::Platform {
    class FileAssociation {
      public:
        // Registers .fat file association with BFView for the current user.
        // Overrides any existing association if the executable path has
        // changed.
        static bool RegisterFatAssociation(bool overrideIfDifferent = true);

        // Checks whether .fat is currently registered to point to this
        // executable.
        static bool IsFatAssociated();
    };

} // namespace BFView::Platform

#endif // BFVIEW_FILEASSOCIATION_H
