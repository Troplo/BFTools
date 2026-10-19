//
// Created by troplo on 10/14/26.
//

#ifndef NOMADARCHIVES_ARCHIVEMODIFIER_H
#define NOMADARCHIVES_ARCHIVEMODIFIER_H

#include "Nomad/Archives/IArchive.hpp"
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace Nomad::Archives {
    struct FileToAdd {
        std::string archiveRelPath;
        std::filesystem::path sourceFilePath;
    };

    class ArchiveModifier {
      public:
        static bool AppendFile(const std::filesystem::path& fatPath,
                               IArchive& archive,
                               const std::string& archiveRelPath,
                               const std::filesystem::path& sourceFilePath,
                               CompressionScheme scheme, std::string& errorMsg);

        static bool
        AppendFiles(const std::filesystem::path& fatPath, IArchive& archive,
                    const std::vector<FileToAdd>& files,
                    CompressionScheme scheme, std::string& errorMsg,
                    const std::function<void(size_t current, size_t total)>&
                        progressCallback = nullptr);

        static bool
        MergeArchive(const std::filesystem::path& targetFatPath,
                     IArchive& targetArchive,
                     const std::filesystem::path& sourceFatPath,
                     const IArchive& sourceArchive, size_t& outMergedCount,
                     std::string& errorMsg,
                     const std::function<void(size_t current, size_t total)>&
                         progressCallback = nullptr);

        static bool SaveMetadata(const std::filesystem::path& fatPath,
                                 const IArchive& archive,
                                 std::string& errorMsg);
    };

} // namespace Nomad::Archives

#endif // NOMADARCHIVES_ARCHIVEMODIFIER_H
