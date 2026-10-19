//
// Created by troplo on 10/14/26.
//

#include "Nomad/Archives/Defragmenter.hpp"
#include "Nomad/Archives/Compression.hpp"
#include "Nomad/Archives/Logger.hpp"
#include "Nomad/Archives/ThreadPool.hpp"
#include "Nomad/Archives/Types.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <format>
#include <fstream>
#include <future>
#include <unordered_map>
#include <vector>

namespace Nomad::Archives {
    DefragStats Defragmenter::Analyze(const std::filesystem::path& fatPath,
                                      const IArchive& archive,
                                      CompressionScheme targetScheme) {
        if (targetScheme == CompressionScheme::Unknown) {
            targetScheme =
                GetDefaultCompressionForVersion(archive.GetVersion());
        }

        std::filesystem::path datPath = fatPath;
        datPath.replace_extension(".dat");

        DefragStats stats;
        stats.entryCount = archive.GetEntries().size();

        std::error_code ec;
        if (std::filesystem::exists(datPath, ec)) {
            stats.currentDatSize = std::filesystem::file_size(datPath, ec);
        }

        std::unordered_map<uint64_t, uint64_t> uniqueOffsets;
        for (const auto& entry : archive.GetEntries()) {
            if (!entry.is_directory) {
                uint64_t sz = entry.compressed_size > 0
                                  ? entry.compressed_size
                                  : entry.uncompressed_size;
                if (sz > 0) {
                    uniqueOffsets[entry.offset] =
                        std::max(uniqueOffsets[entry.offset], sz);
                }
            }
        }

        uint64_t activePayload = 0;
        uint64_t estimatedNewDatSize = 0;
        for (const auto& [off, sz] : uniqueOffsets) {
            activePayload += sz;
            estimatedNewDatSize = AlignUp(estimatedNewDatSize, 16ULL);
            estimatedNewDatSize += sz;
        }

        stats.activePayloadSize = activePayload;
        stats.estimatedNewDatSize = estimatedNewDatSize;

        if (stats.currentDatSize > estimatedNewDatSize) {
            stats.reclaimableBytes = static_cast<int64_t>(stats.currentDatSize -
                                                          estimatedNewDatSize);
        } else {
            stats.reclaimableBytes = 0;
        }

        if (stats.currentDatSize > 0 && stats.reclaimableBytes > 0) {
            stats.fragmentationPercent =
                (static_cast<double>(stats.reclaimableBytes) /
                 static_cast<double>(stats.currentDatSize)) *
                100.0;
        } else {
            stats.fragmentationPercent = 0.0;
        }

        Logger::Debug(
            "Defragmenter",
            "Analyze '{}': {} entries, DAT size: {} B, Active: {} B, Est "
            "Compact: {} B, Reclaimable: {} B ({:.2f}%)",
            fatPath.string(), stats.entryCount, stats.currentDatSize,
            stats.activePayloadSize, stats.estimatedNewDatSize,
            stats.reclaimableBytes, stats.fragmentationPercent);

        return stats;
    }

    bool Defragmenter::Defragment(
        const std::filesystem::path& fatPath, IArchive& archive,
        std::string& errorMsg,
        std::function<void(size_t current, size_t total)> progressCallback,
        CompressionScheme targetScheme, size_t numThreads,
        const std::atomic<bool>* cancelFlag) {
        auto startTime = std::chrono::steady_clock::now();
        errorMsg.clear();

        if (targetScheme == CompressionScheme::Unknown) {
            targetScheme =
                GetDefaultCompressionForVersion(archive.GetVersion());
        }

        size_t threads =
            (numThreads == 0) ? ThreadPool::DefaultThreadCount() : numThreads;
        Logger::Info(
            "Defragmenter",
            "Starting defragmentation for '{}' (Target compression: {}, "
            "Threads: {})",
            fatPath.string(), CompressionToString(targetScheme), threads);

        std::filesystem::path datPath = fatPath;
        datPath.replace_extension(".dat");

        std::error_code ec;
        if (!std::filesystem::exists(datPath, ec)) {
            Logger::Warn(
                "Defragmenter",
                "Source DAT file '{}' does not exist, writing FAT file only",
                datPath.string());
            std::ofstream fatStream(fatPath, std::ios::binary);
            if (!fatStream.is_open() || !archive.Write(fatStream)) {
                errorMsg = "Failed to write FAT file";
                Logger::Error("Defragmenter", "Failed to write FAT file '{}'",
                              fatPath.string());
                return false;
            }
            fatStream.close();

            std::filesystem::path realNfo = fatPath;
            realNfo.replace_extension(".nfo");
            std::ofstream nfoStream(realNfo, std::ios::trunc);
            if (nfoStream.is_open()) {
                archive.SerializeNfo(nfoStream);
                nfoStream.close();
                Logger::Info("Defragmenter",
                             "Generated/refreshed NFO file '{}'",
                             realNfo.string());
            }
            return true;
        }

        std::ifstream oldDat(datPath, std::ios::binary);
        if (!oldDat.is_open()) {
            errorMsg = "Failed to open source DAT file for reading: " +
                       datPath.string();
            Logger::Error("Defragmenter", "{}", errorMsg);
            return false;
        }

        std::filesystem::path tempDat = datPath;
        tempDat += ".defrag_tmp";
        std::filesystem::path tempFat = fatPath;
        tempFat += ".defrag_tmp";
        std::filesystem::path realNfo = fatPath;
        realNfo.replace_extension(".nfo");
        std::filesystem::path tempNfo = realNfo;
        tempNfo += ".defrag_tmp";

        std::ofstream newDat(tempDat, std::ios::binary | std::ios::trunc);
        if (!newDat.is_open()) {
            errorMsg =
                "Failed to create temporary DAT file for defragmentation: " +
                tempDat.string();
            Logger::Error("Defragmenter", "{}", errorMsg);
            return false;
        }

        struct OffsetBlock {
            uint64_t oldOffset{0};
            const ArchiveEntry* representativeEntry{nullptr};
        };

        std::unordered_map<uint64_t, const ArchiveEntry*> uniqueBlocks;
        for (const auto& entry : archive.GetEntries()) {
            if (!entry.is_directory) {
                if (uniqueBlocks.find(entry.offset) == uniqueBlocks.end()) {
                    uniqueBlocks[entry.offset] = &entry;
                }
            }
        }

        std::vector<OffsetBlock> blocks;
        blocks.reserve(uniqueBlocks.size());
        for (const auto& [off, entryPtr] : uniqueBlocks) {
            blocks.push_back({off, entryPtr});
        }

        std::sort(blocks.begin(), blocks.end(),
                  [](const auto& a, const auto& b) {
                      return a.oldOffset < b.oldOffset;
                  });

        Logger::Info(
            "Defragmenter",
            "Identified {} unique payload blocks across {} total entries",
            blocks.size(), archive.GetEntries().size());

        struct CompactedInfo {
            uint64_t newOffset{0};
            uint64_t newCompressedSize{0};
            uint64_t newUncompressedSize{0};
            CompressionScheme newCompression{CompressionScheme::None};
        };

        std::unordered_map<uint64_t, CompactedInfo> offsetMap;

        auto cleanupTemps = [&]() {
            newDat.close();
            oldDat.close();
            std::filesystem::remove(tempDat, ec);
            std::filesystem::remove(tempFat, ec);
            std::filesystem::remove(tempNfo, ec);
        };

        ThreadPool pool(threads);
        size_t batchSize = std::max<size_t>(32, threads * 4);
        size_t totalBatches =
            (blocks.size() + batchSize - 1) / (batchSize == 0 ? 1 : batchSize);

        struct ProcessedBlock {
            size_t blockIdx{0};
            uint64_t oldOffset{0};
            bool success{false};
            std::string errorMsg;
            std::vector<uint8_t> payloadToWrite;
            CompressionScheme newScheme{CompressionScheme::None};
            uint64_t uncompSize{0};
        };

        struct ArchiveDefragEntry {
            size_t blockIdx{0};
            uint64_t oldOffset{0};
            const ArchiveEntry* representativeEntry{nullptr};
            uint64_t compSize{0};
            uint64_t uncompSize{0};
            std::vector<uint8_t> rawBuffer;
            bool readError{false};
        };

        size_t processedCount = 0;
        for (size_t batchStart = 0; batchStart < blocks.size();
             batchStart += batchSize) {
            if (cancelFlag && cancelFlag->load()) {
                errorMsg = "Defragmentation cancelled by user";
                Logger::Warn(
                    "Defragmenter",
                    "Cancel flag detected before batch starting at block {}",
                    batchStart);
                cleanupTemps();
                return false;
            }

            size_t batchCount = std::min(batchSize, blocks.size() - batchStart);
            size_t batchIndex = (batchStart / batchSize) + 1;
            Logger::Debug("Defragmenter",
                          "Processing batch {}/{} (blocks {}..{} of {})",
                          batchIndex, totalBatches, batchStart + 1,
                          batchStart + batchCount, blocks.size());

            std::vector<ArchiveDefragEntry> batchItems;
            batchItems.reserve(batchCount);

            for (size_t b = 0; b < batchCount; ++b) {
                size_t idx = batchStart + b;
                const auto& block = blocks[idx];
                const ArchiveEntry& refEntry = *block.representativeEntry;

                uint64_t compSize = refEntry.compressed_size > 0
                                        ? refEntry.compressed_size
                                        : refEntry.uncompressed_size;
                uint64_t uncompSize = refEntry.uncompressed_size > 0
                                          ? refEntry.uncompressed_size
                                          : compSize;

                ArchiveDefragEntry item;
                item.blockIdx = idx;
                item.oldOffset = block.oldOffset;
                item.representativeEntry = &refEntry;
                item.compSize = compSize;
                item.uncompSize = uncompSize;

                oldDat.seekg(block.oldOffset, std::ios::beg);
                if (compSize > 0) {
                    item.rawBuffer.resize(compSize);
                    oldDat.read(reinterpret_cast<char*>(item.rawBuffer.data()),
                                compSize);
                    if (static_cast<uint64_t>(oldDat.gcount()) != compSize) {
                        item.readError = true;
                    }
                }
                batchItems.push_back(std::move(item));
            }

            for (const auto& item : batchItems) {
                if (item.readError) {
                    errorMsg = std::format(
                        "Failed reading stream data (expected {} bytes, "
                        "got {}) from source DAT at offset {}",
                        item.compSize, oldDat.gcount(), item.oldOffset);
                    Logger::Error("Defragmenter", "{}", errorMsg);
                    cleanupTemps();
                    return false;
                }
            }

            std::vector<std::future<ProcessedBlock>> futures;
            futures.reserve(batchCount);

            for (auto& item : batchItems) {
                futures.push_back(pool.Enqueue([item = std::move(item),
                                                targetScheme, cancelFlag]()
                                                   -> ProcessedBlock {
                    ProcessedBlock res;
                    res.blockIdx = item.blockIdx;
                    res.oldOffset = item.oldOffset;

                    if (cancelFlag && cancelFlag->load()) {
                        res.errorMsg = "Cancelled";
                        return res;
                    }

                    const ArchiveEntry& refEntry = *item.representativeEntry;
                    std::vector<uint8_t> uncompressedData;
                    uint64_t uncompSize = item.uncompSize;

                    if (refEntry.compression == CompressionScheme::None ||
                        refEntry.compression == CompressionScheme::Unknown) {
                        uncompSize = item.compSize;
                        uncompressedData = std::move(item.rawBuffer);
                    } else {
                        std::string decompErr;
                        if (!Compression::Decompress(
                                refEntry.compression, item.rawBuffer,
                                uncompressedData, uncompSize, decompErr)) {
                            res.errorMsg = std::format(
                                "Failed decompressing entry '{}' at offset {}: "
                                "{}",
                                refEntry.path, item.oldOffset, decompErr);
                            return res;
                        }
                    }

                    // Determine target compression:
                    // Rule: Files over 31 bytes can compress. Files <= 31 bytes
                    // always use no compression.
                    if (uncompSize <= 31 ||
                        targetScheme == CompressionScheme::None) {
                        res.newScheme = CompressionScheme::None;
                        res.payloadToWrite = std::move(uncompressedData);
                        res.uncompSize = res.payloadToWrite.size();
                    } else {
                        std::vector<uint8_t> compData;
                        std::string compErr;
                        if (Compression::Compress(
                                targetScheme, uncompressedData.data(),
                                uncompressedData.size(), compData, compErr) &&
                            compData.size() < uncompressedData.size()) {
                            res.newScheme = targetScheme;
                            res.payloadToWrite = std::move(compData);
                            res.uncompSize = uncompSize;
                        } else {
                            res.newScheme = CompressionScheme::None;
                            res.payloadToWrite = std::move(uncompressedData);
                            res.uncompSize = res.payloadToWrite.size();
                        }
                    }

                    res.success = true;
                    return res;
                }));
            }

            for (size_t b = 0; b < futures.size(); ++b) {
                ProcessedBlock res = futures[b].get();

                if (cancelFlag && cancelFlag->load()) {
                    errorMsg = "Defragmentation cancelled by user";
                    Logger::Warn("Defragmenter",
                                 "Cancel flag detected while collecting batch "
                                 "results at block {}",
                                 batchStart + b);
                    cleanupTemps();
                    return false;
                }

                if (!res.success) {
                    errorMsg =
                        res.errorMsg.empty()
                            ? "Error processing block during defragmentation"
                            : res.errorMsg;
                    Logger::Error("Defragmenter", "{}", errorMsg);
                    cleanupTemps();
                    return false;
                }

                // Write aligned to 16 bytes
                uint64_t currPos = static_cast<uint64_t>(newDat.tellp());
                uint64_t alignedPos = AlignUp(currPos, 16ULL);
                if (alignedPos > currPos) {
                    char pad[16] = {0};
                    newDat.write(pad, alignedPos - currPos);
                }

                if (!res.payloadToWrite.empty()) {
                    newDat.write(reinterpret_cast<const char*>(
                                     res.payloadToWrite.data()),
                                 res.payloadToWrite.size());
                }

                CompactedInfo info;
                info.newOffset = alignedPos;
                info.newCompressedSize = res.payloadToWrite.size();
                info.newUncompressedSize =
                    (res.newScheme == CompressionScheme::None)
                        ? res.payloadToWrite.size()
                        : res.uncompSize;
                info.newCompression = res.newScheme;
                offsetMap[res.oldOffset] = info;

                processedCount++;
                if (progressCallback) {
                    progressCallback(processedCount, blocks.size());
                }
            }
        }

        uint64_t writtenBytes = static_cast<uint64_t>(newDat.tellp());
        newDat.flush();
        if (!newDat.good()) {
            errorMsg = "I/O error while writing defragmented DAT file";
            Logger::Error("Defragmenter", "{}", errorMsg);
            cleanupTemps();
            return false;
        }
        newDat.close();
        oldDat.close();

        bool hasNonZeroBlocks = false;
        for (const auto& block : blocks) {
            const ArchiveEntry& ref = *block.representativeEntry;
            if (ref.compressed_size > 0 || ref.uncompressed_size > 0) {
                hasNonZeroBlocks = true;
                break;
            }
        }

        if (hasNonZeroBlocks && writtenBytes == 0) {
            errorMsg =
                "Safety abort: defragmented DAT file is 0 bytes while archive "
                "contains non-empty payloads.";
            Logger::Error("Defragmenter", "{}", errorMsg);
            cleanupTemps();
            return false;
        }

        Logger::Info("Defragmenter",
                     "New DAT payload written: {} bytes. Updating FAT entries "
                     "in memory...",
                     writtenBytes);

        // Update all entries in archive
        for (auto& entry : archive.GetEntries()) {
            if (!entry.is_directory) {
                auto it = offsetMap.find(entry.offset);
                if (it != offsetMap.end()) {
                    entry.offset = it->second.newOffset;
                    entry.compressed_size = it->second.newCompressedSize;
                    entry.uncompressed_size = it->second.newUncompressedSize;
                    entry.compression = it->second.newCompression;
                }
            }
        }

        {
            std::ofstream fatOut(tempFat, std::ios::binary | std::ios::trunc);
            if (!fatOut.is_open() || !archive.Write(fatOut)) {
                errorMsg = "Failed to write temporary defragmented FAT file: " +
                           tempFat.string();
                Logger::Error("Defragmenter", "{}", errorMsg);
                cleanupTemps();
                return false;
            }
        }

        {
            std::ofstream nfoOut(tempNfo, std::ios::trunc);
            if (nfoOut.is_open()) {
                archive.SerializeNfo(nfoOut);
                nfoOut.close();
            }
        }

        Logger::Info("Defragmenter",
                     "Swapping temporary files into production paths...");
        Logger::Debug("Defragmenter", "Rename '{}' -> '{}'", tempDat.string(),
                      datPath.string());
        Logger::Debug("Defragmenter", "Rename '{}' -> '{}'", tempFat.string(),
                      fatPath.string());

        try {
            std::filesystem::rename(tempDat, datPath);
            std::filesystem::rename(tempFat, fatPath);
            if (std::filesystem::exists(tempNfo, ec)) {
                Logger::Debug("Defragmenter", "Rename '{}' -> '{}'",
                              tempNfo.string(), realNfo.string());
                std::error_code rmEc;
                std::filesystem::remove(realNfo, rmEc);
                std::filesystem::rename(tempNfo, realNfo);
                Logger::Info("Defragmenter",
                             "Generated/refreshed NFO file '{}'",
                             realNfo.string());
            }
        } catch (const std::filesystem::filesystem_error& fe) {
            errorMsg =
                std::string(
                    "Filesystem error replacing original archive files: ") +
                fe.what();
            Logger::Error("Defragmenter", "{}", errorMsg);
            cleanupTemps();
            return false;
        }

        auto endTime = std::chrono::steady_clock::now();
        double elapsedSec =
            std::chrono::duration<double>(endTime - startTime).count();
        Logger::Info(
            "Defragmenter",
            "Successfully defragmented '{}' in {:.3f} seconds (DAT: {} bytes)",
            fatPath.string(), elapsedSec, writtenBytes);

        return true;
    }

} // namespace Nomad::Archives
