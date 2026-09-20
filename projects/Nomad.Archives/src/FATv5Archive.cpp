#include "Nomad/Archives/FATv5Archive.hpp"
#include "Nomad/Archives/Logger.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <format>
#include <iostream>
#include <optional>
#include <thread>
#include <tracy/Tracy.hpp>
#include <unordered_map>
#include <vector>

namespace Nomad::Archives {
#pragma pack(push, 1)
    struct RawFATv5Entry {
        uint64_t a;
        uint32_t b;
        uint32_t c;
        uint32_t d;
    };
    struct RawFATv5Extended {
        uint64_t hash;
        uint32_t compressedSize;
        uint32_t uncompressedSize;
        uint64_t offset;
        uint32_t compressionScheme;
    };
#pragma pack(pop)
    static_assert(sizeof(RawFATv5Entry) == 20,
                  "RawFATv5Entry must be 20 bytes");
    static_assert(sizeof(RawFATv5Extended) == 28,
                  "RawFATv5Extended must be 28 bytes");

    static void FastFormatUnknownPath(char *buf, uint64_t hash) {
        static const char hexChars[] = "0123456789ABCDEF";
        memcpy(buf, "__UNKNOWN/0x", 12);
        for (int i = 15; i >= 0; --i) {
            buf[12 + (15 - i)] = hexChars[(hash >> (i * 4)) & 0xF];
        }
        buf[28] = '\0';
    }

    static Platform ParsePlatformIdV5(uint8_t id) {
        switch (id) {
        case 0:
            return Platform::Unknown;
        case 1:
            return Platform::Win64;
        case 2:
            return Platform::Durango;
        case 3:
            return Platform::Orbis;
        case 4:
            return Platform::Scarlett;
        case 5:
            return Platform::Prospero;
        case 6:
            return Platform::Yeti;
        default:
            return Platform::Unknown;
        }
    }

    static uint8_t PlatformToIdV5(Platform platform) {
        switch (platform) {
        case Platform::Win64:
            return 1;
        case Platform::Durango:
            return 2;
        case Platform::Orbis:
            return 3;
        case Platform::Scarlett:
            return 4;
        case Platform::Prospero:
            return 5;
        case Platform::Yeti:
            return 6;
        default:
            return 0;
        }
    }

    static uint8_t CompressionSchemeToIdV5(CompressionScheme scheme,
                                           uint8_t compressionVersion) {
        return FromCompressionSchemeV5(scheme, compressionVersion);
    }

    void FATv5Archive::SetPlatform(Platform platform) { m_platform = platform; }

    void FATv5Archive::SetRawVersion(uint32_t version) {
        m_rawVersion = version;
    }

    bool FATv5Archive::Read(std::istream &stream) {
        ZoneScoped;
        if (!stream)
            return false;

        uint32_t magic = 0;
        stream.read(reinterpret_cast<char *>(&magic), sizeof(magic));
        if (magic != Signature)
            return false;

        stream.read(reinterpret_cast<char *>(&m_rawVersion),
                    sizeof(m_rawVersion));
        if (m_rawVersion != 11 && m_rawVersion != 12 && m_rawVersion != 13)
            return false;

        uint32_t flags = 0;
        stream.read(reinterpret_cast<char *>(&flags), sizeof(flags));
        m_platform = ParsePlatformIdV5(static_cast<uint8_t>(flags & 0xFF));
        m_compressionVersion = static_cast<uint8_t>((flags >> 8) & 0xFF);
        m_nameHashVersion = static_cast<uint8_t>((flags >> 16) & 0xFF);

        stream.read(reinterpret_cast<char *>(&m_archiveHash),
                    sizeof(m_archiveHash));

        uint32_t depCount = 0;
        stream.read(reinterpret_cast<char *>(&depCount), sizeof(depCount));
        m_dependencies.resize(depCount);
        for (uint32_t i = 0; i < depCount; ++i) {
            if (m_rawVersion >= 12) {
                stream.read(
                    reinterpret_cast<char *>(&m_dependencies[i].archiveHash),
                    sizeof(uint64_t));
                stream.read(
                    reinterpret_cast<char *>(&m_dependencies[i].nameHash),
                    sizeof(uint64_t));
            } else {
                uint32_t archHash32 = 0;
                stream.read(reinterpret_cast<char *>(&archHash32),
                            sizeof(archHash32));
                m_dependencies[i].archiveHash = archHash32;
                stream.read(
                    reinterpret_cast<char *>(&m_dependencies[i].nameHash),
                    sizeof(uint64_t));
            }
        }

        uint32_t entryCount = 0;
        stream.read(reinterpret_cast<char *>(&entryCount), sizeof(entryCount));

        m_entries.resize(entryCount);
        std::vector<std::optional<uint32_t>> duplicateIndices(entryCount,
                                                              std::nullopt);

        if (entryCount > 0) {
            std::vector<RawFATv5Entry> rawEntries(entryCount);
            stream.read(reinterpret_cast<char *>(rawEntries.data()),
                        entryCount * sizeof(RawFATv5Entry));
            if (stream.gcount() < static_cast<std::streamsize>(
                                      entryCount * sizeof(RawFATv5Entry))) {
                return false;
            }

            unsigned int numThreads =
                std::max(1u, std::thread::hardware_concurrency());
            size_t chunkSize = (entryCount + numThreads - 1) / numThreads;
            std::vector<std::thread> workers;

            for (unsigned int t = 0; t < numThreads; ++t) {
                size_t start = t * chunkSize;
                size_t end = std::min(start + chunkSize,
                                      static_cast<size_t>(entryCount));
                if (start >= end)
                    break;

                workers.emplace_back([this, &rawEntries, &duplicateIndices,
                                      start, end]() {
                    char buf[32];
                    for (size_t i = start; i < end; ++i) {
                        const auto &raw = rawEntries[i];
                        uint64_t a = raw.a;
                        uint32_t b = raw.b;
                        uint32_t c = raw.c;
                        uint32_t d = raw.d;

                        ArchiveEntry &entry = m_entries[i];
                        entry.name_hash = a;

                        if (b == 0xFFFFFFFFu && c == 0xFFFFFFFFu) {
                            duplicateIndices[i] = d;
                            entry.compressed_size = 0;
                            entry.offset = 0;
                            entry.raw_compression_scheme_id = 0;
                            entry.uncompressed_size = 0;
                            entry.compression = CompressionScheme::None;
                        } else {
                            entry.compressed_size = b & 0x3FFFFFFFu;
                            entry.offset = (static_cast<uint64_t>(c) << 2) |
                                           ((b >> 30) & 0x3u);
                            entry.raw_compression_scheme_id =
                                static_cast<uint8_t>(d & 0x3u);
                            entry.uncompressed_size = (d >> 2) & 0x3FFFFFFFu;

                            entry.compression = ToCompressionScheme(
                                entry.raw_compression_scheme_id,
                                entry.compressed_size, entry.uncompressed_size);

                            if (entry.compression == CompressionScheme::None) {
                                if (entry.uncompressed_size == 0) {
                                    entry.uncompressed_size =
                                        entry.compressed_size;
                                } else if (entry.compressed_size == 0) {
                                    entry.compressed_size =
                                        entry.uncompressed_size;
                                }
                            }
                        }

                        FastFormatUnknownPath(buf, entry.name_hash);
                        entry.path.assign(buf, 28);
                    }
                });
            }

            for (auto &w : workers) {
                if (w.joinable())
                    w.join();
            }
        }

        if (m_rawVersion >= 12) {
            uint32_t duplicateCount = 0;
            stream.read(reinterpret_cast<char *>(&duplicateCount),
                        sizeof(duplicateCount));
            m_duplicates.resize(duplicateCount);
            if (duplicateCount > 0) {
                std::vector<RawFATv5Extended> rawDuplicates(duplicateCount);
                stream.read(reinterpret_cast<char *>(rawDuplicates.data()),
                            duplicateCount * sizeof(RawFATv5Extended));
                if (stream.gcount() <
                    static_cast<std::streamsize>(duplicateCount *
                                                 sizeof(RawFATv5Extended))) {
                    return false;
                }
                for (uint32_t i = 0; i < duplicateCount; ++i) {
                    m_duplicates[i].hash = rawDuplicates[i].hash;
                    m_duplicates[i].compressedSize =
                        rawDuplicates[i].compressedSize;
                    m_duplicates[i].uncompressedSize =
                        rawDuplicates[i].uncompressedSize;
                    m_duplicates[i].offset = rawDuplicates[i].offset;
                    m_duplicates[i].compressionScheme =
                        rawDuplicates[i].compressionScheme;
                }
            }

            // Resolve entries pointing to duplicates/extended 64-bit table
            char buf[32];
            for (size_t i = 0; i < m_entries.size(); ++i) {
                if (duplicateIndices[i].has_value()) {
                    uint32_t dupIdx = *duplicateIndices[i];
                    if (dupIdx < m_duplicates.size()) {
                        const auto &dup = m_duplicates[dupIdx];
                        auto &entry = m_entries[i];
                        entry.name_hash = dup.hash;
                        entry.compressed_size = dup.compressedSize;
                        entry.uncompressed_size = dup.uncompressedSize;
                        entry.offset = dup.offset;
                        entry.raw_compression_scheme_id =
                            static_cast<uint8_t>(dup.compressionScheme & 0xFF);
                        entry.compression = ToCompressionScheme(
                            entry.raw_compression_scheme_id,
                            entry.compressed_size, entry.uncompressed_size);
                        if (entry.compression == CompressionScheme::None) {
                            if (entry.uncompressed_size == 0) {
                                entry.uncompressed_size = entry.compressed_size;
                            } else if (entry.compressed_size == 0) {
                                entry.compressed_size = entry.uncompressed_size;
                            }
                        }
                        FastFormatUnknownPath(buf, entry.name_hash);
                        entry.path.assign(buf, 28);
                    }
                }
            }

            uint32_t localizationCount = 0;
            stream.read(reinterpret_cast<char *>(&localizationCount),
                        sizeof(localizationCount));
            m_localizations.resize(localizationCount);
            for (uint32_t i = 0; i < localizationCount; ++i) {
                uint32_t strLen = 0;
                stream.read(reinterpret_cast<char *>(&strLen), sizeof(strLen));
                m_localizations[i].name.resize(strLen);
                if (strLen > 0) {
                    stream.read(&m_localizations[i].name[0], strLen);
                }
                stream.read(
                    reinterpret_cast<char *>(&m_localizations[i].unknown),
                    sizeof(uint64_t));
            }
        }

        return true;
    }

    // Orwell supports the extended FAT table for higher capacity files, WD2 doesn't seem to
    ArchiveCapacityLimits FATv5Archive::GetCapacityLimits() const {
        if (m_rawVersion < 12) {
            return ArchiveCapacityLimits{
                .maxUncompressedSize = 0x3FFFFFFFu, // 1 GB - 1 byte (30 bits)
                .maxCompressedSize = 0x3FFFFFFFu,   // 1 GB - 1 byte (30 bits)
                .maxOffset = (1ULL << 34) - 1       // 16 GB - 1 byte (34 bits)
            };
        }
        return ArchiveCapacityLimits{
            .maxUncompressedSize = 0xFFFFFFFFULL, // 4 GB - 1 byte (32 bits)
            .maxCompressedSize = 0xFFFFFFFFULL,   // 4 GB - 1 byte (32 bits)
            .maxOffset = UINT64_MAX};
    }

    bool FATv5Archive::Validate(std::string &errorMsg) const {
        return ValidateArchiveEntries("FATv5", m_rawVersion, m_entries,
                                      GetCapacityLimits(), errorMsg);
    }

    bool FATv5Archive::Write(std::ostream &stream) const {
        if (!stream)
            return false;
        if (m_rawVersion != 11 && m_rawVersion != 12 && m_rawVersion != 13)
            return false;

        std::string errorMsg;
        if (!Validate(errorMsg)) {
            Logger::Error("FATv5Archive", "Write failed validation: {}",
                          errorMsg);
            return false;
        }

        uint32_t magic = Signature;
        stream.write(reinterpret_cast<const char *>(&magic), sizeof(magic));
        stream.write(reinterpret_cast<const char *>(&m_rawVersion),
                     sizeof(m_rawVersion));

        uint8_t platformId = PlatformToIdV5(m_platform);
        uint32_t flags = static_cast<uint32_t>(platformId) |
                         (static_cast<uint32_t>(m_compressionVersion) << 8) |
                         (static_cast<uint32_t>(m_nameHashVersion) << 16);
        stream.write(reinterpret_cast<const char *>(&flags), sizeof(flags));

        stream.write(reinterpret_cast<const char *>(&m_archiveHash),
                     sizeof(m_archiveHash));

        uint32_t depCount = static_cast<uint32_t>(m_dependencies.size());
        stream.write(reinterpret_cast<const char *>(&depCount),
                     sizeof(depCount));
        for (const auto &dep : m_dependencies) {
            if (m_rawVersion >= 12) {
                stream.write(reinterpret_cast<const char *>(&dep.archiveHash),
                             sizeof(uint64_t));
                stream.write(reinterpret_cast<const char *>(&dep.nameHash),
                             sizeof(uint64_t));
            } else {
                uint32_t archHash32 = static_cast<uint32_t>(dep.archiveHash);
                stream.write(reinterpret_cast<const char *>(&archHash32),
                             sizeof(archHash32));
                stream.write(reinterpret_cast<const char *>(&dep.nameHash),
                             sizeof(uint64_t));
            }
        }

        uint32_t entryCount = static_cast<uint32_t>(m_entries.size());
        stream.write(reinterpret_cast<const char *>(&entryCount),
                     sizeof(entryCount));

        std::vector<ExtendedFat> duplicatesToWrite = m_duplicates;
        std::unordered_map<uint64_t, uint32_t> dupMap;
        for (size_t i = 0; i < duplicatesToWrite.size(); ++i) {
            dupMap[duplicatesToWrite[i].hash] = static_cast<uint32_t>(i);
        }

        std::unordered_map<size_t, uint32_t> entryToDupIndex;
        if (m_rawVersion >= 12) {
            for (size_t i = 0; i < m_entries.size(); ++i) {
                const auto &entry = m_entries[i];
                bool needs64Bit = (entry.compressed_size > 0x3FFFFFFFu) ||
                                  (entry.uncompressed_size > 0x3FFFFFFFu) ||
                                  (entry.offset > 0x3FFFFFFFFULL);
                if (needs64Bit) {
                    uint8_t schemeId =
                        (entry.raw_compression_scheme_id != 0)
                            ? entry.raw_compression_scheme_id
                            : CompressionSchemeToIdV5(entry.compression,
                                                      m_compressionVersion);

                    auto it = dupMap.find(entry.name_hash);
                    if (it != dupMap.end()) {
                        entryToDupIndex[i] = it->second;
                        auto &dup = duplicatesToWrite[it->second];
                        dup.compressedSize =
                            static_cast<uint32_t>(entry.compressed_size);
                        dup.uncompressedSize =
                            static_cast<uint32_t>(entry.uncompressed_size);
                        dup.offset = entry.offset;
                        dup.compressionScheme = schemeId;
                    } else {
                        uint32_t newIdx =
                            static_cast<uint32_t>(duplicatesToWrite.size());
                        entryToDupIndex[i] = newIdx;
                        dupMap[entry.name_hash] = newIdx;
                        ExtendedFat dup;
                        dup.hash = entry.name_hash;
                        dup.compressedSize =
                            static_cast<uint32_t>(entry.compressed_size);
                        dup.uncompressedSize =
                            static_cast<uint32_t>(entry.uncompressed_size);
                        dup.offset = entry.offset;
                        dup.compressionScheme = schemeId;
                        duplicatesToWrite.push_back(dup);
                    }
                }
            }
        }

        for (size_t i = 0; i < m_entries.size(); ++i) {
            const auto &entry = m_entries[i];
            uint64_t a = entry.name_hash;
            uint32_t b = 0, c = 0, d = 0;

            auto it = entryToDupIndex.find(i);
            if (m_rawVersion >= 12 && it != entryToDupIndex.end()) {
                b = 0xFFFFFFFFu;
                c = 0xFFFFFFFFu;
                d = it->second;
            } else {
                b = (entry.compressed_size & 0x3FFFFFFFu) |
                    (static_cast<uint32_t>((entry.offset & 0x3u) << 30));
                c = static_cast<uint32_t>((entry.offset >> 2) & 0xFFFFFFFFu);
                uint8_t schemeId =
                    (entry.raw_compression_scheme_id != 0)
                        ? entry.raw_compression_scheme_id
                        : CompressionSchemeToIdV5(entry.compression,
                                                  m_compressionVersion);
                d = (schemeId & 0x3u) |
                    ((static_cast<uint32_t>(entry.uncompressed_size) &
                      0x3FFFFFFFu)
                     << 2);
            }

            stream.write(reinterpret_cast<const char *>(&a), sizeof(a));
            stream.write(reinterpret_cast<const char *>(&b), sizeof(b));
            stream.write(reinterpret_cast<const char *>(&c), sizeof(c));
            stream.write(reinterpret_cast<const char *>(&d), sizeof(d));
        }

        if (m_rawVersion >= 12) {
            uint32_t duplicateCount =
                static_cast<uint32_t>(duplicatesToWrite.size());
            stream.write(reinterpret_cast<const char *>(&duplicateCount),
                         sizeof(duplicateCount));
            for (const auto &dup : duplicatesToWrite) {
                stream.write(reinterpret_cast<const char *>(&dup.hash),
                             sizeof(uint64_t));
                stream.write(
                    reinterpret_cast<const char *>(&dup.compressedSize),
                    sizeof(uint32_t));
                stream.write(
                    reinterpret_cast<const char *>(&dup.uncompressedSize),
                    sizeof(uint32_t));
                stream.write(reinterpret_cast<const char *>(&dup.offset),
                             sizeof(uint64_t));
                stream.write(
                    reinterpret_cast<const char *>(&dup.compressionScheme),
                    sizeof(uint32_t));
            }

            uint32_t localizationCount =
                static_cast<uint32_t>(m_localizations.size());
            stream.write(reinterpret_cast<const char *>(&localizationCount),
                         sizeof(localizationCount));
            for (const auto &loc : m_localizations) {
                uint32_t strLen = static_cast<uint32_t>(loc.name.size());
                stream.write(reinterpret_cast<const char *>(&strLen),
                             sizeof(strLen));
                if (strLen > 0) {
                    stream.write(loc.name.data(), strLen);
                }
                stream.write(reinterpret_cast<const char *>(&loc.unknown),
                             sizeof(uint64_t));
            }
        }

        return true;
    }

    bool FATv5Archive::SerializeNfo(std::ostream &stream) const {
        if (!stream)
            return false;
        stream << "<Root>\n\t<common>\n";
        for (const auto &entry : m_entries) {
            std::string nfoPath = entry.path;
            if (nfoPath.starts_with("__UNKNOWN/")) {
                nfoPath = "";
            } else {
                nfoPath = NormalizePathForNfo(nfoPath);
            }
            uint64_t nfoSize = entry.uncompressed_size > 0
                                   ? entry.uncompressed_size
                                   : entry.compressed_size;
            stream << std::format("\t\t<File Path=\"{}\" Crc=\"{}\" "
                                  "FilePosition=\"{}\" FileSize=\"{}\" "
                                  "FileTime=\"0\" />\n",
                                  nfoPath, entry.name_hash, entry.offset,
                                  nfoSize);
        }
        stream << "\t</common>\n</Root>\n";
        return true;
    }

    uint64_t FATv5Archive::ComputeRawNameHash(std::string_view name) const {
        size_t start = 0;
        while (start < name.size() &&
               (name[start] == '/' || name[start] == '\\')) {
            ++start;
        }
        if (start >= name.size())
            return 0xFFFFFFFFFFFFFFFFULL;
        uint64_t hash = 0xcbf29ce484222325ULL;
        for (size_t i = start; i < name.size(); ++i) {
            char c = name[i];
            if (c == '/')
                c = '\\';
            char lower =
                static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            hash *= 0x100000001b3ULL;
            hash ^= static_cast<uint8_t>(lower);
        }
        hash = (hash & 0x1FFFFFFFFFFFFFFFULL) | 0xA000000000000000ULL;
        return hash;
    }

    uint64_t FATv5Archive::ComputeNameHash(std::string_view name) const {
        if (m_projectConfig) {
            auto overrideHash = m_projectConfig->FindHashByPath(name);
            if (overrideHash.has_value()) {
                return *overrideHash;
            }
        }
        return ComputeRawNameHash(name);
    }

    std::string FATv5Archive::RenderNameHash(uint64_t hash) const {
        return std::format("0x{:016X}", hash);
    }

    CompressionScheme FATv5Archive::ToCompressionScheme(uint8_t id) const {
        return ToCompressionSchemeV5(id, m_compressionVersion);
    }

    CompressionScheme
    FATv5Archive::ToCompressionScheme(uint8_t id, uint64_t /*compressedSize*/,
                                      uint64_t /*uncompressedSize*/) const {
        return ToCompressionScheme(id);
    }

    uint8_t
    FATv5Archive::FromCompressionScheme(CompressionScheme scheme) const {
        return FromCompressionSchemeV5(scheme, m_compressionVersion);
    }

    void FATv5Archive::AddOrUpdateEntry(const std::string &path,
                                        uint64_t uncompressedSize,
                                        uint64_t compressedSize,
                                        CompressionScheme compression,
                                        uint64_t offset) {
        uint64_t hash = ComputeNameHash(path);
        uint8_t schemeId = FromCompressionScheme(compression);
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
