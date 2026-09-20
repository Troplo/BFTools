//
// Created by troplo on 09/21/26.
//

#include "Nomad/Archives/FATv2Archive.hpp"
#include "Nomad/Archives/HashingUtils.hpp"
#include "Nomad/Archives/Logger.hpp"
#include "Nomad/Archives/ThreadPool.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <format>
#include <future>
#include <istream>
#include <ostream>
#include <pugixml.hpp>
#include <tracy/Tracy.hpp>
#include <vector>

namespace Nomad::Archives {
#pragma pack(push, 1)
    struct RawFATv2Entry {
        uint64_t hash;
        uint32_t num;
        uint32_t dataHash;
        uint32_t num2;
        uint32_t num3;
    };
#pragma pack(pop)
    static_assert(sizeof(RawFATv2Entry) == 24,
                  "RawFATv2Entry must be 24 bytes");

    static void FastFormatUnknownPath64(char *buf, uint64_t hash) {
        static const char hexChars[] = "0123456789ABCDEF";
        memcpy(buf, "__UNKNOWN/0x", 12);
        for (int i = 15; i >= 0; --i) {
            buf[12 + (15 - i)] = hexChars[(hash >> (i * 4)) & 0xF];
        }
        buf[28] = '\0';
    }

    static Platform ParsePlatformIdV2(uint8_t targetId,
                                      uint8_t fallbackPlatformId) {
        switch (targetId) {
        case 1:
            return Platform::Win32;
        case 2:
            return Platform::Xenon;
        case 3:
            return Platform::PS3;
        case 4:
            return Platform::Win64;
        case 8:
            return Platform::WiiU;
        default:
            return ParsePlatformId(fallbackPlatformId);
        }
    }

    static uint8_t PlatformToIdV2(Platform platform) {
        switch (platform) {
        case Platform::Win32:
            return 1;
        case Platform::Xenon:
            return 2;
        case Platform::PS3:
            return 3;
        case Platform::Win64:
            return 4;
        case Platform::WiiU:
            return 8;
        default:
            return 0;
        }
    }

    static uint8_t SchemeToIdV2(CompressionScheme scheme, uint32_t rawVersion) {
        (void)rawVersion;
        switch (scheme) {
        case CompressionScheme::None:
            return 0;
        case CompressionScheme::LZO1x:
            return 1;
        case CompressionScheme::Zlib:
            return 2;
        case CompressionScheme::XMemCompress:
            return 3;
        case CompressionScheme::LZMA:
            return 4;
        default:
            return 0;
        }
    }

    bool FATv2Archive::Read(std::istream &stream) {
        ZoneScopedN("FATv2Archive::Read");
        if (!stream)
            return false;

        uint32_t magic = 0;
        stream.read(reinterpret_cast<char *>(&magic), sizeof(magic));
        if (magic != Signature)
            return false;

        stream.read(reinterpret_cast<char *>(&m_rawVersion),
                    sizeof(m_rawVersion));
        if (m_rawVersion != 7 && m_rawVersion != 8)
            return false;

        uint32_t flags = 0;
        stream.read(reinterpret_cast<char *>(&flags), sizeof(flags));

        uint8_t targetId = static_cast<uint8_t>(flags & 0xFF);
        uint8_t platformId = static_cast<uint8_t>((flags >> 8) & 0xFF);
        m_flags02 = static_cast<uint8_t>((flags >> 16) & 0xFF);

        m_platform = ParsePlatformIdV2(targetId, platformId);
        m_rawPlatform = targetId;

        uint32_t entryCount = 0;
        stream.read(reinterpret_cast<char *>(&entryCount), sizeof(entryCount));

        m_entries.resize(entryCount);

        if (entryCount > 0) {
            std::vector<RawFATv2Entry> rawEntries(entryCount);
            stream.read(reinterpret_cast<char *>(rawEntries.data()),
                        entryCount * sizeof(RawFATv2Entry));
            if (stream.gcount() < static_cast<std::streamsize>(
                                      entryCount * sizeof(RawFATv2Entry))) {
                return false;
            }

            unsigned int numThreads =
                std::max(1u, std::thread::hardware_concurrency());
            size_t chunkSize = (entryCount + numThreads - 1) / numThreads;
            ThreadPool pool(numThreads);
            std::vector<std::future<void>> futures;
            futures.reserve(numThreads);

            for (unsigned int t = 0; t < numThreads; ++t) {
                size_t start = t * chunkSize;
                size_t end = std::min(start + chunkSize,
                                      static_cast<size_t>(entryCount));
                if (start >= end)
                    break;

                futures.push_back(pool.Enqueue([this, &rawEntries, start,
                                                end]() {
                    char buf[32];
                    for (size_t i = start; i < end; ++i) {
                        const auto &raw = rawEntries[i];
                        uint64_t hash = raw.hash;
                        uint32_t num = raw.num;
                        uint32_t num2 = raw.num2;
                        uint32_t num3 = raw.num3;

                        ArchiveEntry &entry = m_entries[i];
                        entry.name_hash = hash;
                        entry.raw_compression_scheme_id =
                            static_cast<uint8_t>(num & 7);
                        entry.uncompressed_size = (num >> 3) & 0x1FFFFFFF;
                        entry.compressed_size = num2 & 0x1FFFFFFF;
                        entry.offset = (static_cast<uint64_t>(num3) << 3) |
                                       ((num2 >> 29) & 7);

                        if (entry.raw_compression_scheme_id == 0) {
                            entry.uncompressed_size = entry.compressed_size;
                            entry.compression = CompressionScheme::None;
                        } else {
                            entry.compression = ToCompressionScheme(
                                entry.raw_compression_scheme_id,
                                entry.compressed_size, entry.uncompressed_size);
                        }

                        FastFormatUnknownPath64(buf, hash);
                        entry.path.assign(buf, 28);
                    }
                }));
            }

            for (auto &f : futures) {
                f.get();
            }
        }

        return true;
    }

    ArchiveCapacityLimits FATv2Archive::GetCapacityLimits() const {
        if (m_rawVersion >= 7) {
            return ArchiveCapacityLimits{.maxUncompressedSize = 0x1FFFFFFFu,
                                         .maxCompressedSize = 0x1FFFFFFFu,
                                         .maxOffset = (1ULL << 35) - 1};
        }
        return ArchiveCapacityLimits{.maxUncompressedSize = 0x1FFFFFFFu,
                                     .maxCompressedSize = 0x1FFFFFFFu,
                                     .maxOffset = (1ULL << 34) - 1};
    }

    bool FATv2Archive::Validate(std::string &errorMsg) const {
        return ValidateArchiveEntries("FATv2", m_rawVersion, m_entries,
                                      GetCapacityLimits(), errorMsg);
    }

    bool FATv2Archive::Write(std::ostream &stream) const {
        if (!stream)
            return false;
        if (m_rawVersion != 7 && m_rawVersion != 8)
            return false;

        std::string errorMsg;
        if (!Validate(errorMsg)) {
            Logger::Error("FATv2Archive", "Write failed validation: {}",
                          errorMsg);
            return false;
        }

        uint32_t magic = Signature;
        stream.write(reinterpret_cast<const char *>(&magic), sizeof(magic));
        stream.write(reinterpret_cast<const char *>(&m_rawVersion),
                     sizeof(m_rawVersion));

        uint8_t targetId = PlatformToIdV2(m_platform);
        uint32_t platformId = static_cast<uint32_t>(PlatformToId(m_platform));
        uint32_t flags = (static_cast<uint32_t>(m_flags02) << 16) |
                         (platformId << 8) | targetId;
        stream.write(reinterpret_cast<const char *>(&flags), sizeof(flags));

        uint32_t count = static_cast<uint32_t>(m_entries.size());
        stream.write(reinterpret_cast<const char *>(&count), sizeof(count));

        for (const auto &entry : m_entries) {
            uint64_t hash = entry.name_hash;
            uint8_t schemeId = SchemeToIdV2(entry.compression, m_rawVersion);

            uint32_t num = (entry.uncompressed_size << 3) | (schemeId & 7);
            uint32_t dataHash = 0;
            uint32_t num2 = (entry.compressed_size & 0x1FFFFFFF) |
                            (static_cast<uint32_t>((entry.offset & 7) << 29));
            uint32_t num3 = static_cast<uint32_t>(entry.offset >> 3);

            stream.write(reinterpret_cast<const char *>(&hash), sizeof(hash));
            stream.write(reinterpret_cast<const char *>(&num), sizeof(num));
            stream.write(reinterpret_cast<const char *>(&dataHash),
                         sizeof(dataHash));
            stream.write(reinterpret_cast<const char *>(&num2), sizeof(num2));
            stream.write(reinterpret_cast<const char *>(&num3), sizeof(num3));
        }

        return true;
    }

    bool FATv2Archive::SerializeNfo(std::ostream &stream) const {
        if (!stream)
            return false;
        pugi::xml_document doc;
        pugi::xml_node root = doc.append_child("Root");
        pugi::xml_node common = root.append_child("common");

        for (const auto &entry : m_entries) {
            std::string nfoPath;
            if (!entry.path.starts_with("__UNKNOWN/")) {
                nfoPath = NormalizePathForNfo(entry.path);
            }
            uint64_t nfoSize = entry.compressed_size > 0
                                   ? entry.compressed_size
                                   : entry.uncompressed_size;

            pugi::xml_node fileNode = common.append_child("File");
            fileNode.append_attribute("Path").set_value(nfoPath.c_str());
            fileNode.append_attribute("Crc").set_value(
                std::to_string(entry.name_hash).c_str());
            fileNode.append_attribute("FilePosition")
                .set_value(std::to_string(entry.offset).c_str());
            fileNode.append_attribute("FileSize")
                .set_value(std::to_string(nfoSize).c_str());
            fileNode.append_attribute("FileTime").set_value("0");
        }

        doc.save(stream, "\t",
                 pugi::format_default | pugi::format_no_declaration);
        return true;
    }

    uint64_t FATv2Archive::ComputeRawNameHash(std::string_view name) const {
        return HashingUtils::ComputeCRC64Path(name);
    }

    uint64_t FATv2Archive::ComputeNameHash(std::string_view name) const {
        if (m_projectConfig) {
            auto overrideHash = m_projectConfig->FindHashByPath(name);
            if (overrideHash.has_value()) {
                return *overrideHash;
            }
        }
        return ComputeRawNameHash(name);
    }

    std::string FATv2Archive::RenderNameHash(uint64_t hash) const {
        return std::format("0x{:016X}", hash);
    }

    CompressionScheme FATv2Archive::ToCompressionScheme(uint8_t id) const {
        return ToCompressionScheme(id, 0, 0);
    }

    CompressionScheme
    FATv2Archive::ToCompressionScheme(uint8_t id, uint64_t /*compressedSize*/,
                                      uint64_t /*uncompressedSize*/) const {
        switch (id) {
        case 0:
            return CompressionScheme::None;
        case 1:
            return CompressionScheme::LZO1x;
        case 2:
            return CompressionScheme::Zlib;
        case 3:
            return CompressionScheme::XMemCompress;
        case 4:
            return CompressionScheme::LZMA;
        default:
            return CompressionScheme::None;
        }
    }

    uint8_t
    FATv2Archive::FromCompressionScheme(CompressionScheme scheme) const {
        return SchemeToIdV2(scheme, m_rawVersion);
    }

    void FATv2Archive::AddOrUpdateEntry(const std::string &path,
                                        uint64_t uncompressedSize,
                                        uint64_t compressedSize,
                                        CompressionScheme compression,
                                        uint64_t offset) {
        uint64_t hash = ComputeNameHash(path);
        uint8_t schemeId = SchemeToIdV2(compression, m_rawVersion);
        for (auto &entry : m_entries) {
            if (entry.name_hash == hash) {
                entry.path = path;
                entry.uncompressed_size = uncompressedSize;
                entry.compressed_size = compressedSize;
                entry.compression = compression;
                entry.raw_compression_scheme_id = schemeId;
                entry.offset = offset;
                return;
            }
        }

        ArchiveEntry newEntry;
        newEntry.name_hash = hash;
        newEntry.path = path;
        newEntry.uncompressed_size = uncompressedSize;
        newEntry.compressed_size = compressedSize;
        newEntry.compression = compression;
        newEntry.raw_compression_scheme_id = schemeId;
        newEntry.offset = offset;
        m_entries.push_back(newEntry);
    }

} // namespace Nomad::Archives
