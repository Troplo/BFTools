//
// Created by troplo on 09/21/26.
//

#ifndef NOMADARCHIVES_EXTRACTOR_H
#define NOMADARCHIVES_EXTRACTOR_H

#include "Nomad/Archives/IArchive.hpp"
#include <atomic>
#include <filesystem>
#include <functional>
#include <iosfwd>
#include <string>
#include <vector>

namespace Nomad::Archives {
    using ProgressCallback = std::function<void(
        size_t current, size_t total, const std::string& currentFile)>;

    class Extractor {
      public:
        static bool ExtractEntry(const ArchiveEntry& entry,
                                 std::istream& datStream,
                                 const std::filesystem::path& outputFilePath);
        static bool ExtractAll(const IArchive& archive,
                               const std::filesystem::path& fatPath,
                               const std::filesystem::path& outputDir,
                               ProgressCallback progress = nullptr,
                               size_t numThreads = 0);
        static bool ExtractAll(const IArchive& archive, std::istream& datStream,
                               const std::filesystem::path& outputDir,
                               ProgressCallback progress = nullptr,
                               size_t numThreads = 0);
        static bool
        ExtractEntries(const std::vector<const ArchiveEntry*>& entries,
                       const std::filesystem::path& datPath,
                       const std::filesystem::path& outputDir,
                       ProgressCallback progress = nullptr,
                       size_t numThreads = 0,
                       const std::atomic<bool>* cancelFlag = nullptr);
    };

} // namespace Nomad::Archives

#endif // NOMADARCHIVES_EXTRACTOR_H
