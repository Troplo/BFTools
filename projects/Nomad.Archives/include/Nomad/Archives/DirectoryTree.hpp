//
// Created by troplo on 09/21/26.
//

#ifndef NOMADARCHIVES_DIRECTORYTREE_H
#define NOMADARCHIVES_DIRECTORYTREE_H

#include "Nomad/Archives/Types.hpp"
#include <memory>
#include <string>
#include <vector>

namespace Nomad::Archives {
    struct ArchiveNode {
        std::string name;
        std::string fullPath;
        bool isDirectory{false};
        const ArchiveEntry* entryRef{nullptr};
        ArchiveNode* parent{nullptr};
        size_t rowIndex{0};
        std::vector<std::shared_ptr<ArchiveNode>> children;

        [[nodiscard]] size_t RowInParent() const { return rowIndex; }
    };

    class DirectoryTreeBuilder {
      public:
        static std::shared_ptr<ArchiveNode>
        Build(const std::vector<ArchiveEntry>& entries);
    };

} // namespace Nomad::Archives

#endif // NOMADARCHIVES_DIRECTORYTREE_H
