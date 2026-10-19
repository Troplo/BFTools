//
// Created by troplo on 09/21/26.
//

#include "Nomad/Archives/DirectoryTree.hpp"
#include <algorithm>
#include <string_view>
#include <tracy/Tracy.hpp>
#include <unordered_map>

namespace Nomad::Archives {
    struct StringHash {
        using is_transparent = void;
        size_t operator()(std::string_view sv) const noexcept {
            return std::hash<std::string_view>{}(sv);
        }
        size_t operator()(const std::string& s) const noexcept {
            return std::hash<std::string_view>{}(s);
        }
    };

    std::shared_ptr<ArchiveNode>
    DirectoryTreeBuilder::Build(const std::vector<ArchiveEntry>& entries) {
        ZoneScopedN("DirectoryTreeBuilder::Build");
        auto root = std::make_shared<ArchiveNode>();
        root->name = "/";
        root->fullPath = "/";
        root->isDirectory = true;
        root->rowIndex = 0;

        std::unordered_map<std::string, ArchiveNode*, StringHash,
                           std::equal_to<>>
            dirMap;
        dirMap.reserve(std::max<size_t>(64, entries.size() / 4));
        dirMap[""] = root.get();
        dirMap["/"] = root.get();

        auto getOrCreateDir = [&](std::string_view dirPath,
                                  auto& self) -> ArchiveNode* {
            if (dirPath.empty() || dirPath == "/") {
                return root.get();
            }
            auto it = dirMap.find(dirPath);
            if (it != dirMap.end()) {
                return it->second;
            }

            size_t lastSlash = dirPath.find_last_of("/\\");
            std::string_view parentPath;
            std::string_view dirName;
            if (lastSlash == std::string_view::npos) {
                dirName = dirPath;
            } else {
                parentPath = dirPath.substr(0, lastSlash);
                dirName = dirPath.substr(lastSlash + 1);
            }

            ArchiveNode* parentNode = self(parentPath, self);

            auto dirNode = std::make_shared<ArchiveNode>();
            dirNode->name = std::string(dirName);
            dirNode->fullPath =
                (parentNode == root.get())
                    ? std::string(dirName)
                    : (parentNode->fullPath + "/" + std::string(dirName));
            dirNode->isDirectory = true;
            dirNode->parent = parentNode;
            dirNode->rowIndex = parentNode->children.size();

            ArchiveNode* rawDir = dirNode.get();
            parentNode->children.push_back(std::move(dirNode));
            std::string dirPathStr(dirPath);
            dirMap[dirPathStr] = rawDir;
            if (rawDir->fullPath != dirPathStr) {
                dirMap[rawDir->fullPath] = rawDir;
            }
            return rawDir;
        };

        for (const auto& entry : entries) {
            if (entry.path.empty())
                continue;

            size_t lastSlash = entry.path.find_last_of("/\\");
            std::string_view dirPath;
            std::string_view fileName;

            if (lastSlash == std::string_view::npos) {
                fileName = entry.path;
            } else {
                dirPath = std::string_view(entry.path).substr(0, lastSlash);
                fileName = std::string_view(entry.path).substr(lastSlash + 1);
            }

            ArchiveNode* parentNode = nullptr;
            if (dirPath.empty() || dirPath == "/") {
                parentNode = root.get();
            } else {
                auto it = dirMap.find(dirPath);
                if (it != dirMap.end()) {
                    parentNode = it->second;
                } else {
                    parentNode = getOrCreateDir(dirPath, getOrCreateDir);
                }
            }

            auto fileNode = std::make_shared<ArchiveNode>();
            fileNode->name = std::string(fileName);
            fileNode->fullPath = entry.path;
            fileNode->isDirectory = false;
            fileNode->entryRef = &entry;
            fileNode->parent = parentNode;
            fileNode->rowIndex = parentNode->children.size();

            parentNode->children.push_back(std::move(fileNode));
        }

        return root;
    }

} // namespace Nomad::Archives
