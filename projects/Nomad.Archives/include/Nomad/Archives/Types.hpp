//
// Created by troplo on 09/21/26.
//

#ifndef NOMADARCHIVES_TYPES_H
#define NOMADARCHIVES_TYPES_H

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Nomad::Archives {
    enum class Platform : uint8_t {
        Unknown = 0,
        Win32,
        Win64,
        Xenon,
        PS3,
        WiiU,
        Durango,
        Orbis,
        Scarlett,
        Prospero,
        Yeti
    };

    enum class CompressionScheme : uint8_t {
        None = 0,
        LZO1x,
        Zlib,
        XMemCompress,
        LZMA,
        LZ4LW,
        Oodle,
        Unknown
    };

    enum class CompressionSchemeV0 : uint8_t { None = 0 };

    enum class CompressionSchemeV4 : uint8_t { None = 0, LZO1x = 1, Zlib = 2 };

    enum class CompressionSchemeV5 : uint8_t {
        None = 0,
        LZO1x = 1,
        Zlib = 2,
        XMemCompress = 3
    };

    enum class CompressionSchemeV6 : uint8_t { None = 0, LZMA = 1, LZ4LW = 2 };

    enum class CompressionSchemeV8 : uint8_t {
        None = 0,
        Oodle = 1,
        LZMA = 2,
        LZ4LW = 3
    };

    enum class CompressionSchemeV9 : uint8_t {
        None = 0,
        Oodle = 1,
        LZMA = 2,
        LZ4LW = 3
    };

    enum class ArchiveVersion : uint8_t { Unknown = 0, FATv2, FATv3, FATv5 };

    struct ArchiveEntry {
        std::string path;
        uint64_t name_hash{0};
        uint64_t offset{0};
        uint64_t compressed_size{0};
        uint64_t uncompressed_size{0};
        CompressionScheme compression{CompressionScheme::None};
        uint8_t raw_compression_scheme_id{0};
        bool is_directory{false};
    };

    struct ArchiveCapacityLimits {
        uint64_t maxUncompressedSize{UINT64_MAX};
        uint64_t maxCompressedSize{UINT64_MAX};
        uint64_t maxOffset{UINT64_MAX};
    };

    bool ValidateArchiveEntries(std::string_view archiveName,
                                uint32_t rawVersion,
                                const std::vector<ArchiveEntry> &entries,
                                const ArchiveCapacityLimits &limits,
                                std::string &errorMsg);

    std::string_view PlatformToString(Platform platform);
    std::string_view CompressionToString(CompressionScheme compression);
    std::string_view VersionToString(ArchiveVersion version);
    bool HasWildcards(std::string_view pattern);
    bool MatchGlob(std::string_view pattern, std::string_view text,
                   bool caseInsensitive = true);
    std::string NormalizePath(std::string_view path);
    std::string NormalizePathForHash(std::string_view path);
    std::string NormalizePathForNfo(std::string_view path);
    std::string EscapeXml(std::string_view str);
    std::string UnescapeXml(std::string_view str);

    Platform ParsePlatformId(uint8_t rawPlatformId);
    uint8_t PlatformToId(Platform platform);

    CompressionScheme ParseCompressionScheme(std::string_view str);
    CompressionScheme GetDefaultCompressionForVersion(ArchiveVersion version);

    // Sub-version compression scheme conversions
    CompressionScheme ToCompressionScheme(CompressionSchemeV0 scheme);
    CompressionScheme ToCompressionScheme(CompressionSchemeV4 scheme);
    CompressionScheme ToCompressionScheme(CompressionSchemeV5 scheme);
    CompressionScheme ToCompressionScheme(CompressionSchemeV6 scheme);
    CompressionScheme ToCompressionScheme(CompressionSchemeV8 scheme);
    CompressionScheme ToCompressionScheme(CompressionSchemeV9 scheme);

    std::optional<CompressionSchemeV0>
    ToCompressionSchemeV0(CompressionScheme scheme);
    std::optional<CompressionSchemeV4>
    ToCompressionSchemeV4(CompressionScheme scheme);
    std::optional<CompressionSchemeV5>
    ToCompressionSchemeV5(CompressionScheme scheme);
    std::optional<CompressionSchemeV6>
    ToCompressionSchemeV6(CompressionScheme scheme);
    std::optional<CompressionSchemeV8>
    ToCompressionSchemeV8(CompressionScheme scheme);
    std::optional<CompressionSchemeV9>
    ToCompressionSchemeV9(CompressionScheme scheme);

    inline std::string_view CompressionToString(CompressionSchemeV0 scheme) {
        return CompressionToString(ToCompressionScheme(scheme));
    }
    inline std::string_view CompressionToString(CompressionSchemeV4 scheme) {
        return CompressionToString(ToCompressionScheme(scheme));
    }
    inline std::string_view CompressionToString(CompressionSchemeV5 scheme) {
        return CompressionToString(ToCompressionScheme(scheme));
    }
    inline std::string_view CompressionToString(CompressionSchemeV6 scheme) {
        return CompressionToString(ToCompressionScheme(scheme));
    }
    inline std::string_view CompressionToString(CompressionSchemeV8 scheme) {
        return CompressionToString(ToCompressionScheme(scheme));
    }
    inline std::string_view CompressionToString(CompressionSchemeV9 scheme) {
        return CompressionToString(ToCompressionScheme(scheme));
    }

    CompressionScheme ToCompressionSchemeV3(uint8_t id,
                                            uint8_t compressionVersion);
    uint8_t FromCompressionSchemeV3(CompressionScheme scheme,
                                    uint8_t compressionVersion);

    CompressionScheme ToCompressionSchemeV5(uint8_t id,
                                            uint8_t compressionVersion);
    uint8_t FromCompressionSchemeV5(CompressionScheme scheme,
                                    uint8_t compressionVersion);

    template <typename T, typename U = T>
    [[nodiscard]] constexpr T AlignUp(T value, U alignment) noexcept {
        return (value + (static_cast<T>(alignment) - 1)) &
               ~(static_cast<T>(alignment) - 1);
    }

} // namespace Nomad::Archives

#endif // NOMADARCHIVES_TYPES_H
