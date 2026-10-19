//
// Created by troplo on 09/21/26.
//

#include "Nomad/Archives/FATv3Archive.hpp"
#include "Nomad/Archives/HashingUtils.hpp"
#include "Nomad/Archives/Logger.hpp"
#include "Nomad/Archives/ThreadPool.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <format>
#include <future>
#include <iostream>
#include <pugixml.hpp>
#include <tracy/Tracy.hpp>
#include <unordered_set>
#include <vector>

namespace Nomad::Archives {
#pragma pack(push, 1)
    struct RawFATv3Entry {
        uint32_t a;
        uint32_t b;
        uint32_t c;
        uint32_t d;
    };
#pragma pack(pop)
    static_assert(sizeof(RawFATv3Entry) == 16,
                  "RawFATv3Entry must be 16 bytes");

    static void FastFormatUnknownPath32(char* buf, uint32_t hash) {
        static const char hexChars[] = "0123456789ABCDEF";
        memcpy(buf, "__UNKNOWN/0x", 12);
        for (int i = 7; i >= 0; --i) {
            buf[12 + (7 - i)] = hexChars[(hash >> (i * 4)) & 0xF];
        }
        buf[20] = '\0';
    }

    static Platform ParsePlatformIdV3(uint8_t id) {
        switch (id) {
        case 0:
            return Platform::Unknown;
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
            return Platform::Unknown;
        }
    }

    static uint8_t PlatformToIdV3(Platform platform) {
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

    void FATv3Archive::SetPlatform(Platform platform) {
        m_platform = platform;
        switch (platform) {
        case Platform::PS3:
            m_compressionVersion = 4;
            m_nameHashVersion = 55;
            break;
        case Platform::Xenon:
            m_compressionVersion = 5;
            m_nameHashVersion = 55;
            break;
        case Platform::WiiU:
            m_compressionVersion = 5;
            m_nameHashVersion = 58;
            break;
        case Platform::Win32:
        case Platform::Win64:
        default:
            m_compressionVersion = 5;
            m_nameHashVersion = 50;
            break;
        }
    }

    bool FATv3Archive::Read(std::istream& stream) {
        ZoneScopedN("FATv3Archive::Read");
        if (!stream)
            return false;

        uint32_t magic = 0;
        stream.read(reinterpret_cast<char*>(&magic), sizeof(magic));
        if (magic != Signature)
            return false;

        stream.read(reinterpret_cast<char*>(&m_rawVersion),
                    sizeof(m_rawVersion));
        if (m_rawVersion != 7 && m_rawVersion != 8)
            return false;

        uint32_t flags = 0;
        stream.read(reinterpret_cast<char*>(&flags), sizeof(flags));

        uint8_t platformId = static_cast<uint8_t>(flags & 0xFF);
        m_platform = ParsePlatformIdV3(platformId);
        m_rawPlatform = platformId;
        m_compressionVersion = static_cast<uint8_t>((flags >> 8) & 0xFF);
        m_nameHashVersion = static_cast<uint8_t>((flags >> 16) & 0xFF);

        uint32_t count = 0;
        stream.read(reinterpret_cast<char*>(&count), sizeof(count));

        m_entries.resize(count);

        if (count > 0) {
            std::vector<RawFATv3Entry> rawEntries(count);
            stream.read(reinterpret_cast<char*>(rawEntries.data()),
                        count * sizeof(RawFATv3Entry));
            if (stream.gcount() <
                static_cast<std::streamsize>(count * sizeof(RawFATv3Entry))) {
                return false;
            }

            unsigned int numThreads =
                std::max(1u, std::thread::hardware_concurrency());
            size_t chunkSize = (count + numThreads - 1) / numThreads;
            ThreadPool pool(numThreads);
            std::vector<std::future<void>> futures;
            futures.reserve(numThreads);

            for (unsigned int t = 0; t < numThreads; ++t) {
                size_t start = t * chunkSize;
                size_t end =
                    std::min(start + chunkSize, static_cast<size_t>(count));
                if (start >= end)
                    break;

                futures.push_back(pool.Enqueue([this, &rawEntries, start,
                                                end]() {
                    char buf[32];
                    for (size_t i = start; i < end; ++i) {
                        const auto& raw = rawEntries[i];
                        uint32_t a = raw.a;
                        uint32_t b = raw.b;
                        uint32_t c = raw.c;
                        uint32_t d = raw.d;

                        ArchiveEntry& entry = m_entries[i];
                        entry.name_hash = a;

                        if (m_rawVersion == 7) {
                            entry.uncompressed_size = (b >> 2) & 0x3FFFFFFFu;
                            entry.raw_compression_scheme_id =
                                static_cast<uint8_t>(b & 0x3u);
                            entry.compressed_size = c & 0x3FFFFFFFu;
                            entry.offset = (static_cast<uint64_t>(d) << 2) |
                                           ((c >> 30) & 0x3u);
                        } else {
                            entry.uncompressed_size = (b >> 3) & 0x1FFFFFFFu;
                            entry.raw_compression_scheme_id =
                                static_cast<uint8_t>(b & 0x7u);
                            entry.compressed_size = c & 0x1FFFFFFFu;
                            entry.offset = (static_cast<uint64_t>(d) << 3) |
                                           ((c >> 29) & 0x7u);
                        }

                        entry.compression = ToCompressionScheme(
                            entry.raw_compression_scheme_id,
                            entry.compressed_size, entry.uncompressed_size);

                        if (entry.compression == CompressionScheme::None) {
                            if (entry.uncompressed_size == 0) {
                                entry.uncompressed_size = entry.compressed_size;
                            }
                            if (entry.compressed_size == 0) {
                                entry.compressed_size = entry.uncompressed_size;
                            }
                        } else {
                            if (entry.uncompressed_size == 0 &&
                                entry.compressed_size > 0) {
                                entry.uncompressed_size = entry.compressed_size;
                                entry.compression = CompressionScheme::None;
                            }
                        }

                        FastFormatUnknownPath32(buf, a);
                        entry.path.assign(buf, 20);
                    }
                }));
            }

            for (auto& f : futures) {
                f.get();
            }
        }

        return true;
    }

    ArchiveCapacityLimits FATv3Archive::GetCapacityLimits() const {
        if (m_rawVersion == 7) {
            return ArchiveCapacityLimits{
                .maxUncompressedSize = 0x3FFFFFFFu, // 1 GB - 1 byte (30 bits)
                .maxCompressedSize = 0x3FFFFFFFu,   // 1 GB - 1 byte (30 bits)
                .maxOffset = (1ULL << 34) - 1       // 16 GB - 1 byte (34 bits)
            };
        }
        // rawVersion 8 (default):
        return ArchiveCapacityLimits{
            .maxUncompressedSize = 0x1FFFFFFFu, // 512 MB - 1 byte (29 bits)
            .maxCompressedSize = 0x1FFFFFFFu,   // 512 MB - 1 byte (29 bits)
            .maxOffset = (1ULL << 35) - 1       // 32 GB - 1 byte (35 bits)
        };
    }

    bool FATv3Archive::Validate(std::string& errorMsg) const {
        return ValidateArchiveEntries("FATv3", m_rawVersion, m_entries,
                                      GetCapacityLimits(), errorMsg);
    }

    bool FATv3Archive::Write(std::ostream& stream) const {
        if (!stream)
            return false;
        if (m_rawVersion != 7 && m_rawVersion != 8)
            return false;

        std::string errorMsg;
        if (!Validate(errorMsg)) {
            Logger::Error("FATv3Archive", "Write failed validation: {}",
                          errorMsg);
            return false;
        }

        uint32_t magic = Signature;
        stream.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
        stream.write(reinterpret_cast<const char*>(&m_rawVersion),
                     sizeof(m_rawVersion));

        uint32_t flags = 0;
        flags |= (static_cast<uint32_t>(PlatformToIdV3(m_platform)) & 0xFF)
                 << 0;
        flags |= (static_cast<uint32_t>(m_compressionVersion) & 0xFF) << 8;
        flags |= (static_cast<uint32_t>(m_nameHashVersion) & 0xFF) << 16;
        stream.write(reinterpret_cast<const char*>(&flags), sizeof(flags));

        uint32_t count = static_cast<uint32_t>(m_entries.size());
        stream.write(reinterpret_cast<const char*>(&count), sizeof(count));

        for (const auto& entry : m_entries) {
            uint32_t a = static_cast<uint32_t>(entry.name_hash);
            uint8_t schemeId = FromCompressionSchemeV3(entry.compression,
                                                       m_compressionVersion);

            uint64_t compSize = entry.compressed_size > 0
                                    ? entry.compressed_size
                                    : entry.uncompressed_size;
            uint64_t uncompSize = entry.uncompressed_size > 0
                                      ? entry.uncompressed_size
                                      : compSize;

            uint32_t b = 0, c = 0, d = 0;
            if (m_rawVersion == 7) {
                uint32_t uncompField =
                    (schemeId == 0)
                        ? 0
                        : (static_cast<uint32_t>(uncompSize) & 0x3FFFFFFFu);
                b = (schemeId & 0x3u) | (uncompField << 2);
                c = (static_cast<uint32_t>(compSize) & 0x3FFFFFFFu) |
                    (static_cast<uint32_t>((entry.offset & 0x3u) << 30));
                d = static_cast<uint32_t>(entry.offset >> 2);
            } else {
                uint32_t uncompField =
                    (schemeId == 0)
                        ? 0
                        : (static_cast<uint32_t>(uncompSize) & 0x1FFFFFFFu);
                b = (schemeId & 0x7u) | (uncompField << 3);
                c = (static_cast<uint32_t>(compSize) & 0x1FFFFFFFu) |
                    (static_cast<uint32_t>((entry.offset & 0x7u) << 29));
                d = static_cast<uint32_t>(entry.offset >> 3);
            }

            stream.write(reinterpret_cast<const char*>(&a), sizeof(a));
            stream.write(reinterpret_cast<const char*>(&b), sizeof(b));
            stream.write(reinterpret_cast<const char*>(&c), sizeof(c));
            stream.write(reinterpret_cast<const char*>(&d), sizeof(d));
        }
        return true;
    }

    bool FATv3Archive::SerializeNfo(std::ostream& stream) const {
        if (!stream)
            return false;
        pugi::xml_document doc;
        pugi::xml_node root = doc.append_child("Root");
        pugi::xml_node common = root.append_child("common");

        for (const auto& entry : m_entries) {
            std::string nfoPath;
            if (!entry.path.starts_with("__UNKNOWN/")) {
                nfoPath = NormalizePathForNfo(entry.path);
            }
            uint64_t nfoSize = entry.uncompressed_size > 0
                                   ? entry.uncompressed_size
                                   : entry.compressed_size;

            pugi::xml_node fileNode = common.append_child("File");
            fileNode.append_attribute("Path").set_value(nfoPath.c_str());
            fileNode.append_attribute("Crc").set_value(
                std::to_string(static_cast<uint32_t>(entry.name_hash)).c_str());
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

    uint64_t FATv3Archive::ComputeRawNameHash(std::string_view name) const {
        return HashingUtils::ComputeFNV32Path(name);
    }

    uint64_t FATv3Archive::ComputeNameHash(std::string_view name) const {
        if (m_projectConfig) {
            auto overrideHash = m_projectConfig->FindHashByPath(name);
            if (overrideHash.has_value()) {
                return static_cast<uint32_t>(*overrideHash);
            }
        }
        return ComputeRawNameHash(name);
    }

    std::string FATv3Archive::RenderNameHash(uint64_t hash) const {
        return std::format("0x{:08X}", static_cast<uint32_t>(hash));
    }

    CompressionScheme FATv3Archive::ToCompressionScheme(uint8_t id) const {
        return ToCompressionSchemeV3(id, m_compressionVersion);
    }

    CompressionScheme
    FATv3Archive::ToCompressionScheme(uint8_t id, uint64_t /*compressedSize*/,
                                      uint64_t /*uncompressedSize*/) const {
        return ToCompressionScheme(id);
    }

    uint8_t
    FATv3Archive::FromCompressionScheme(CompressionScheme scheme) const {
        return FromCompressionSchemeV3(scheme, m_compressionVersion);
    }

    void FATv3Archive::AddOrUpdateEntry(const std::string& path,
                                        uint64_t uncompressedSize,
                                        uint64_t compressedSize,
                                        CompressionScheme compression,
                                        uint64_t offset) {
        uint64_t hash = ComputeNameHash(path);
        uint8_t schemeId =
            FromCompressionSchemeV3(compression, m_compressionVersion);
        for (auto& entry : m_entries) {
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
