//
// Created by troplo on 09/21/26.
//

#include "Nomad/Archives/Extractor.hpp"
#include "Nomad/Archives/Compression.hpp"
#include "Nomad/Archives/Logger.hpp"
#include "Nomad/Archives/ThreadPool.hpp"
#include <atomic>
#include <fstream>
#include <iostream>
#include <mutex>

namespace Nomad::Archives {
    bool Extractor::ExtractEntry(const ArchiveEntry &entry,
                                 std::istream &datStream,
                                 const std::filesystem::path &outputFilePath) {
        if (!datStream)
            return false;

        std::filesystem::path normalizedPath =
            outputFilePath.lexically_normal();
        std::filesystem::create_directories(normalizedPath.parent_path());
        std::ofstream outFile(normalizedPath, std::ios::binary);
        if (!outFile.is_open())
            return false;

        datStream.clear();
        datStream.seekg(entry.offset, std::ios::beg);

        if (entry.compression == CompressionScheme::None) {
            size_t bytesToRead = entry.compressed_size > 0
                                     ? entry.compressed_size
                                     : entry.uncompressed_size;
            constexpr size_t bufferSize = 64 * 1024;
            std::vector<char> buffer(bufferSize);
            size_t totalRead = 0;

            while (totalRead < bytesToRead) {
                size_t chunk = std::min(bufferSize, bytesToRead - totalRead);
                datStream.read(buffer.data(), chunk);
                std::streamsize readBytes = datStream.gcount();
                if (readBytes <= 0)
                    break;
                outFile.write(buffer.data(), readBytes);
                totalRead += readBytes;
            }

            if (totalRead < bytesToRead) {
                Logger::Error(
                    "Extractor",
                    "Incomplete read for entry '{}' at offset {} (expected {} "
                    "bytes, read {})",
                    entry.path, entry.offset, bytesToRead, totalRead);
                return false;
            }
            return true;
        } else {
            std::vector<uint8_t> compressedBuffer(entry.compressed_size);
            datStream.read(reinterpret_cast<char *>(compressedBuffer.data()),
                           entry.compressed_size);
            if (datStream.gcount() <
                static_cast<std::streamsize>(entry.compressed_size)) {
                Logger::Error(
                    "Extractor",
                    "Incomplete read for entry '{}' at offset {} (expected {} "
                    "bytes, read {})",
                    entry.path, entry.offset, entry.compressed_size,
                    datStream.gcount());
                return false;
            }

            std::vector<uint8_t> uncompressedBuffer;
            std::string errorMsg;
            if (!Compression::Decompress(entry.compression, compressedBuffer,
                                         uncompressedBuffer,
                                         entry.uncompressed_size, errorMsg)) {
                Logger::Error(
                    "Extractor",
                    "Decompression failed for entry '{}' (scheme: {}): {}",
                    entry.path, CompressionToString(entry.compression),
                    errorMsg);
                // Write raw compressed bytes as fallback
                outFile.write(
                    reinterpret_cast<const char *>(compressedBuffer.data()),
                    compressedBuffer.size());
                return false;
            }

            outFile.write(
                reinterpret_cast<const char *>(uncompressedBuffer.data()),
                uncompressedBuffer.size());
            return true;
        }
    }

    bool
    Extractor::ExtractEntries(const std::vector<const ArchiveEntry *> &entries,
                              const std::filesystem::path &datPath,
                              const std::filesystem::path &outputDir,
                              ProgressCallback progress, size_t numThreads,
                              const std::atomic<bool> *cancelFlag) {
        size_t total = entries.size();
        if (total == 0)
            return true;

        if (numThreads == 0) {
            numThreads = GetDefaultThreadCount();
        }

        Logger::Info("Extractor",
                     "Extracting {} entries to '{}' using {} threads", total,
                     outputDir.string(), numThreads);

        if (numThreads <= 1) {
            std::ifstream datStream(datPath, std::ios::binary);
            if (!datStream.is_open()) {
                Logger::Error("Extractor", "Failed to open DAT file: {}",
                              datPath.string());
                return false;
            }

            size_t current = 0;
            bool allSuccess = true;
            for (const auto *entry : entries) {
                if (cancelFlag && cancelFlag->load()) {
                    Logger::Warn("Extractor",
                                 "Extraction cancelled by user at entry {}/{}",
                                 current, total);
                    return false;
                }

                current++;
                std::filesystem::path normEntryPath =
                    NormalizePath(entry->path);
                std::filesystem::path entryOutPath = outputDir / normEntryPath;
                if (progress) {
                    progress(current, total, normEntryPath.string());
                }

                if (!ExtractEntry(*entry, datStream, entryOutPath)) {
                    allSuccess = false;
                }
            }
            return allSuccess;
        }

        ThreadPool pool(numThreads);
        std::atomic<size_t> currentCount{0};
        std::mutex progressMutex;
        std::vector<std::future<bool>> futures;
        futures.reserve(total);

        for (const auto *entry : entries) {
            futures.push_back(pool.Enqueue([entry, &datPath, &outputDir,
                                            progress, &currentCount, total,
                                            &progressMutex,
                                            cancelFlag]() -> bool {
                if (cancelFlag && cancelFlag->load()) {
                    return false;
                }

                std::ifstream localStream(datPath, std::ios::binary);
                if (!localStream.is_open()) {
                    return false;
                }

                std::filesystem::path normEntryPath =
                    NormalizePath(entry->path);
                std::filesystem::path entryOutPath = outputDir / normEntryPath;

                bool success = ExtractEntry(*entry, localStream, entryOutPath);

                size_t c = ++currentCount;
                if (progress) {
                    std::lock_guard<std::mutex> lock(progressMutex);
                    progress(c, total, normEntryPath.string());
                }
                return success;
            }));
        }

        bool allSuccess = true;
        for (auto &f : futures) {
            if (!f.get()) {
                allSuccess = false;
            }
        }

        if (cancelFlag && cancelFlag->load()) {
            Logger::Warn(
                "Extractor",
                "Extraction cancelled by user. Processed {}/{} entries.",
                currentCount.load(), total);
        } else {
            Logger::Info(
                "Extractor",
                "Extraction finished: {}/{} entries extracted (success: {}).",
                currentCount.load(), total, allSuccess);
        }
        return allSuccess;
    }

    bool Extractor::ExtractAll(const IArchive &archive,
                               const std::filesystem::path &fatPath,
                               const std::filesystem::path &outputDir,
                               ProgressCallback progress, size_t numThreads) {
        std::filesystem::path datPath = fatPath;
        datPath.replace_extension(".dat");

        const auto &entries = archive.GetEntries();
        std::vector<const ArchiveEntry *> entryPtrs;
        entryPtrs.reserve(entries.size());
        for (const auto &entry : entries) {
            entryPtrs.push_back(&entry);
        }

        return ExtractEntries(entryPtrs, datPath, outputDir, progress,
                              numThreads);
    }

    bool Extractor::ExtractAll(const IArchive &archive, std::istream &datStream,
                               const std::filesystem::path &outputDir,
                               ProgressCallback progress, size_t numThreads) {
        const auto &entries = archive.GetEntries();
        size_t total = entries.size();
        if (total == 0)
            return true;

        if (numThreads == 0) {
            numThreads = GetDefaultThreadCount();
        }

        if (numThreads <= 1) {
            size_t current = 0;
            for (const auto &entry : entries) {
                current++;
                std::filesystem::path normEntryPath = NormalizePath(entry.path);
                std::filesystem::path entryOutPath = outputDir / normEntryPath;
                if (progress) {
                    progress(current, total, normEntryPath.string());
                }
                ExtractEntry(entry, datStream, entryOutPath);
            }
            return true;
        }

        return false;
    }

} // namespace Nomad::Archives
