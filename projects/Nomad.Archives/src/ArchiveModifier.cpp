//
// Created by troplo on 10/14/26.
//

#include "Nomad/Archives/ArchiveModifier.hpp"
#include "Nomad/Archives/Compression.hpp"
#include "Nomad/Archives/Logger.hpp"
#include "Nomad/Archives/ThreadPool.hpp"
#include "Nomad/Archives/Types.hpp"
#include <algorithm>
#include <format>
#include <fstream>
#include <future>
#include <vector>

namespace Nomad::Archives {
    bool ArchiveModifier::AppendFile(
        const std::filesystem::path& fatPath, IArchive& archive,
        const std::string& archiveRelPath,
        const std::filesystem::path& sourceFilePath, CompressionScheme scheme,
        std::string& errorMsg) {
        std::vector<FileToAdd> files = {{archiveRelPath, sourceFilePath}};
        return AppendFiles(fatPath, archive, files, scheme, errorMsg, nullptr);
    }

    bool ArchiveModifier::AppendFiles(
        const std::filesystem::path& fatPath, IArchive& archive,
        const std::vector<FileToAdd>& files, CompressionScheme scheme,
        std::string& errorMsg,
        const std::function<void(size_t current, size_t total)>&
            progressCallback) {
        errorMsg.clear();

        if (files.empty()) {
            return true;
        }

        const auto limits = archive.GetCapacityLimits();

        for (const auto& file : files) {
            std::error_code ec;
            if (!std::filesystem::is_regular_file(file.sourceFilePath, ec)) {
                errorMsg =
                    "Source file does not exist or is not a regular file: " +
                    file.sourceFilePath.string();
                return false;
            }
            uint64_t uncompressedSize =
                std::filesystem::file_size(file.sourceFilePath, ec);
            if (!ec && uncompressedSize > limits.maxUncompressedSize) {
                errorMsg = std::format(
                    "'{}': size ({:.2f} MB / {} bytes) exceeds {} maximum "
                    "limit of "
                    "{:.2f} MB ({} bytes)",
                    file.archiveRelPath,
                    static_cast<double>(uncompressedSize) / (1024.0 * 1024.0),
                    uncompressedSize, VersionToString(archive.GetVersion()),
                    static_cast<double>(limits.maxUncompressedSize) /
                        (1024.0 * 1024.0),
                    limits.maxUncompressedSize);
                return false;
            }
        }

        std::filesystem::path datPath = fatPath;
        datPath.replace_extension(".dat");

        if (!std::filesystem::exists(datPath)) {
            std::ofstream createDat(datPath, std::ios::binary);
        }

        std::fstream datStream(datPath,
                               std::ios::binary | std::ios::in | std::ios::out);
        if (!datStream.is_open()) {
            errorMsg =
                "Failed to open or create DAT payload file for appending: " +
                datPath.string();
            return false;
        }

        datStream.seekp(0, std::ios::end);

        struct ProcessedFile {
            size_t index{0};
            std::string archiveRelPath;
            uint64_t uncompressedSize{0};
            std::vector<uint8_t> payloadData;
            CompressionScheme actualScheme{CompressionScheme::None};
            bool success{false};
            std::string errorMsg;
        };

        ThreadPool pool(ThreadPool::DefaultThreadCount());
        std::vector<std::future<ProcessedFile>> futures;
        futures.reserve(files.size());

        for (size_t i = 0; i < files.size(); ++i) {
            futures.push_back(pool.Enqueue([&files, i, scheme, &limits,
                                            &archive]() -> ProcessedFile {
                const auto& file = files[i];
                ProcessedFile pf;
                pf.index = i;
                pf.archiveRelPath = file.archiveRelPath;

                std::error_code ec;
                if (!std::filesystem::is_regular_file(file.sourceFilePath,
                                                      ec)) {
                    pf.errorMsg = "Source file does not exist or is not a "
                                  "regular file: " +
                                  file.sourceFilePath.string();
                    return pf;
                }

                uint64_t uncompressedSize =
                    std::filesystem::file_size(file.sourceFilePath, ec);
                pf.uncompressedSize = uncompressedSize;

                if (uncompressedSize > limits.maxUncompressedSize) {
                    pf.errorMsg = std::format(
                        "File '{}' uncompressed size ({:.2f} MB / {} bytes) "
                        "exceeds {} "
                        "maximum limit of {:.2f} MB ({} bytes)",
                        file.archiveRelPath,
                        static_cast<double>(uncompressedSize) /
                            (1024.0 * 1024.0),
                        uncompressedSize, VersionToString(archive.GetVersion()),
                        static_cast<double>(limits.maxUncompressedSize) /
                            (1024.0 * 1024.0),
                        limits.maxUncompressedSize);
                    return pf;
                }

                std::ifstream srcIn(file.sourceFilePath, std::ios::binary);
                if (!srcIn.is_open()) {
                    pf.errorMsg = "Failed to open source file for reading: " +
                                  file.sourceFilePath.string();
                    return pf;
                }

                std::vector<uint8_t> uncompressedData(uncompressedSize);
                if (uncompressedSize > 0) {
                    srcIn.read(reinterpret_cast<char*>(uncompressedData.data()),
                               uncompressedSize);
                    if (static_cast<uint64_t>(srcIn.gcount()) !=
                        uncompressedSize) {
                        pf.errorMsg =
                            "Failed to read complete content of source file: " +
                            file.sourceFilePath.string();
                        return pf;
                    }
                }
                srcIn.close();

                if (scheme == CompressionScheme::None ||
                    uncompressedSize <= 31) {
                    pf.payloadData = std::move(uncompressedData);
                    pf.actualScheme = CompressionScheme::None;
                } else {
                    std::string compError;
                    if (Compression::Compress(scheme, uncompressedData.data(),
                                              uncompressedData.size(),
                                              pf.payloadData, compError) &&
                        pf.payloadData.size() < uncompressedData.size()) {
                        pf.actualScheme = scheme;
                    } else {
                        pf.payloadData = std::move(uncompressedData);
                        pf.actualScheme = CompressionScheme::None;
                    }
                }

                if (pf.payloadData.size() > limits.maxCompressedSize) {
                    pf.errorMsg = std::format(
                        "File '{}' compressed size ({:.2f} MB / {} bytes) "
                        "exceeds {} "
                        "maximum limit of {:.2f} MB ({} bytes)",
                        file.archiveRelPath,
                        static_cast<double>(pf.payloadData.size()) /
                            (1024.0 * 1024.0),
                        pf.payloadData.size(),
                        VersionToString(archive.GetVersion()),
                        static_cast<double>(limits.maxCompressedSize) /
                            (1024.0 * 1024.0),
                        limits.maxCompressedSize);
                    return pf;
                }

                pf.success = true;
                return pf;
            }));
        }

        for (size_t i = 0; i < futures.size(); ++i) {
            ProcessedFile pf = futures[i].get();
            if (!pf.success) {
                errorMsg = pf.errorMsg;
                datStream.close();
                return false;
            }

            uint64_t currPos = static_cast<uint64_t>(datStream.tellp());
            uint64_t alignedOffset = AlignUp(currPos, 16ULL);

            if (alignedOffset > limits.maxOffset) {
                errorMsg = std::format(
                    "Cannot write file '{}': offset ({:.2f} GB / {} bytes) "
                    "exceeds {} "
                    "maximum addressable offset of {:.2f} GB ({} bytes)",
                    pf.archiveRelPath,
                    static_cast<double>(alignedOffset) /
                        (1024.0 * 1024.0 * 1024.0),
                    alignedOffset, VersionToString(archive.GetVersion()),
                    static_cast<double>(limits.maxOffset) /
                        (1024.0 * 1024.0 * 1024.0),
                    limits.maxOffset);
                datStream.close();
                return false;
            }

            if (alignedOffset > currPos) {
                char pad[16] = {0};
                datStream.write(pad, alignedOffset - currPos);
            }

            uint64_t entryOffset = alignedOffset;
            if (!pf.payloadData.empty()) {
                datStream.write(
                    reinterpret_cast<const char*>(pf.payloadData.data()),
                    pf.payloadData.size());
            }

            archive.AddOrUpdateEntry(NormalizePath(pf.archiveRelPath),
                                     pf.uncompressedSize, pf.payloadData.size(),
                                     pf.actualScheme, entryOffset);

            if (progressCallback) {
                progressCallback(i + 1, files.size());
            }
        }

        datStream.flush();
        datStream.close();

        return SaveMetadata(fatPath, archive, errorMsg);
    }

    bool ArchiveModifier::MergeArchive(
        const std::filesystem::path& targetFatPath, IArchive& targetArchive,
        const std::filesystem::path& sourceFatPath,
        const IArchive& sourceArchive, size_t& outMergedCount,
        std::string& errorMsg,
        const std::function<void(size_t current, size_t total)>&
            progressCallback) {
        errorMsg.clear();
        outMergedCount = 0;

        std::filesystem::path sourceDatPath = sourceFatPath;
        sourceDatPath.replace_extension(".dat");

        if (!std::filesystem::exists(sourceDatPath)) {
            errorMsg = "Source DAT file not found: " + sourceDatPath.string();
            return false;
        }

        std::ifstream srcDat(sourceDatPath, std::ios::binary);
        if (!srcDat.is_open()) {
            errorMsg =
                "Failed to open source DAT file: " + sourceDatPath.string();
            return false;
        }

        std::filesystem::path targetDatPath = targetFatPath;
        targetDatPath.replace_extension(".dat");

        if (!std::filesystem::exists(targetDatPath)) {
            std::ofstream createDat(targetDatPath, std::ios::binary);
        }

        std::fstream tgtDat(targetDatPath,
                            std::ios::binary | std::ios::in | std::ios::out);
        if (!tgtDat.is_open()) {
            errorMsg =
                "Failed to open target DAT file: " + targetDatPath.string();
            return false;
        }

        tgtDat.seekp(0, std::ios::end);

        const auto& srcEntries = sourceArchive.GetEntries();
        std::vector<uint8_t> buffer;

        for (size_t i = 0; i < srcEntries.size(); ++i) {
            const auto& entry = srcEntries[i];
            if (entry.is_directory)
                continue;

            uint64_t dataSize = entry.compressed_size > 0
                                    ? entry.compressed_size
                                    : entry.uncompressed_size;

            srcDat.seekg(entry.offset);
            buffer.resize(dataSize);
            if (dataSize > 0) {
                srcDat.read(reinterpret_cast<char*>(buffer.data()), dataSize);
                if (static_cast<uint64_t>(srcDat.gcount()) != dataSize) {
                    errorMsg = "Failed to read entry payload from source DAT "
                               "at offset " +
                               std::to_string(entry.offset);
                    return false;
                }
            }

            uint64_t currPos = static_cast<uint64_t>(tgtDat.tellp());
            uint64_t alignedOffset = AlignUp(currPos, 16ULL);
            if (alignedOffset > currPos) {
                char pad[16] = {0};
                tgtDat.write(pad, alignedOffset - currPos);
            }

            uint64_t newOffset = alignedOffset;
            if (dataSize > 0) {
                tgtDat.write(reinterpret_cast<const char*>(buffer.data()),
                             dataSize);
            }

            targetArchive.AddOrUpdateEntry(entry.path, entry.uncompressed_size,
                                           entry.compressed_size,
                                           entry.compression, newOffset);

            ++outMergedCount;

            if (progressCallback) {
                progressCallback(i + 1, srcEntries.size());
            }
        }

        tgtDat.flush();
        tgtDat.close();
        srcDat.close();

        return SaveMetadata(targetFatPath, targetArchive, errorMsg);
    }

    bool ArchiveModifier::SaveMetadata(const std::filesystem::path& fatPath,
                                       const IArchive& archive,
                                       std::string& errorMsg) {
        errorMsg.clear();

        std::string validateError;
        if (!archive.Validate(validateError)) {
            errorMsg = "Archive validation failed: " + validateError;
            Logger::Error("ArchiveModifier", "{}", errorMsg);
            return false;
        }

        Logger::Debug("ArchiveModifier",
                      "Writing updated metadata for '{}' ({} entries)...",
                      fatPath.string(), archive.GetEntries().size());

        std::ofstream fatStream(fatPath, std::ios::binary | std::ios::trunc);
        if (!fatStream.is_open() || !archive.Write(fatStream)) {
            errorMsg = "Failed to write FAT file: " + fatPath.string();
            Logger::Error("ArchiveModifier", "{}", errorMsg);
            return false;
        }
        fatStream.close();

        std::filesystem::path nfoPath = fatPath;
        nfoPath.replace_extension(".nfo");
        std::ofstream nfoStream(nfoPath, std::ios::trunc);
        if (nfoStream.is_open()) {
            archive.SerializeNfo(nfoStream);
            nfoStream.close();
            Logger::Info("ArchiveModifier", "Generated/refreshed NFO file '{}'",
                         nfoPath.string());
        } else {
            Logger::Warn("ArchiveModifier",
                         "Failed to open NFO file '{}' for writing",
                         nfoPath.string());
        }

        return true;
    }
} // namespace Nomad::Archives
