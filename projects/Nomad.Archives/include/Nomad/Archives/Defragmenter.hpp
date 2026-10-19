//
// Created by troplo on 10/14/26.
//

#ifndef NOMADARCHIVES_DEFRAGMENTER_H
#define NOMADARCHIVES_DEFRAGMENTER_H

#include "Nomad/Archives/IArchive.hpp"
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

namespace Nomad::Archives {
    struct DefragStats {
        uint64_t currentDatSize{0};
        uint64_t activePayloadSize{0};
        uint64_t entryCount{0};
        uint64_t estimatedNewDatSize{0};
        int64_t reclaimableBytes{0};
        double fragmentationPercent{0.0};
    };

    class Defragmenter {
      public:
        static DefragStats
        Analyze(const std::filesystem::path& fatPath, const IArchive& archive,
                CompressionScheme targetScheme = CompressionScheme::Unknown);

        static bool Defragment(
            const std::filesystem::path& fatPath, IArchive& archive,
            std::string& errorMsg,
            std::function<void(size_t current, size_t total)> progressCallback =
                nullptr,
            CompressionScheme targetScheme = CompressionScheme::Unknown,
            size_t numThreads = 0,
            const std::atomic<bool>* cancelFlag = nullptr);
    };

} // namespace Nomad::Archives

#endif // NOMADARCHIVES_DEFRAGMENTER_H
