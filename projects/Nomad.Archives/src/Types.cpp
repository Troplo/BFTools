//
// Created by troplo on 09/21/26.
//

#include "Nomad/Archives/Types.hpp"
#include <algorithm>
#include <cctype>
#include <format>

namespace Nomad::Archives {
    std::string_view PlatformToString(Platform platform) {
        switch (platform) {
        case Platform::Win32:
            return "Win32";
        case Platform::Win64:
            return "Win64";
        case Platform::Xenon:
            return "Xenon";
        case Platform::PS3:
            return "PS3";
        case Platform::WiiU:
            return "WiiU";
        case Platform::Durango:
            return "Durango";
        case Platform::Orbis:
            return "Orbis";
        case Platform::Scarlett:
            return "Scarlett";
        case Platform::Prospero:
            return "Prospero";
        case Platform::Yeti:
            return "Yeti";
        default:
            return "Unknown";
        }
    }

    std::string_view CompressionToString(CompressionScheme compression) {
        switch (compression) {
        case CompressionScheme::None:
            return "None";
        case CompressionScheme::LZO1x:
            return "LZO1x";
        case CompressionScheme::Zlib:
            return "Zlib";
        case CompressionScheme::XMemCompress:
            return "XMemCompress";
        case CompressionScheme::LZMA:
            return "LZMA";
        case CompressionScheme::LZ4LW:
            return "LZ4LW";
        case CompressionScheme::Oodle:
            return "Oodle";
        default:
            return "Unknown";
        }
    }

    std::string_view VersionToString(ArchiveVersion version) {
        switch (version) {
        case ArchiveVersion::FATv2:
            return "FATv2";
        case ArchiveVersion::FATv3:
            return "FATv3";
        case ArchiveVersion::FATv5:
            return "FATv5";
        default:
            return "Unknown";
        }
    }

    bool HasWildcards(std::string_view pattern) {
        return pattern.find_first_of("*?[") != std::string_view::npos;
    }

    static inline bool IsPathSeparator(char c) noexcept {
        return c == '/' || c == '\\';
    }

    static bool GlobMatchInternal(std::string_view pattern,
                                  std::string_view text, bool ci) {
        size_t p = 0;
        size_t s = 0;
        const size_t pLen = pattern.size();
        const size_t sLen = text.size();

        while (p < pLen) {
            if (pattern[p] == '*' && p + 1 < pLen && pattern[p + 1] == '*') {
                while (p < pLen && pattern[p] == '*')
                    ++p;
                if (p < pLen && IsPathSeparator(pattern[p])) {
                    if (GlobMatchInternal(pattern.substr(p), text.substr(s),
                                          ci)) {
                        return true;
                    }
                    if (GlobMatchInternal(pattern.substr(p + 1), text.substr(s),
                                          ci)) {
                        return true;
                    }
                }
                if (p == pLen)
                    return true;
                for (size_t i = s; i <= sLen; ++i) {
                    if (GlobMatchInternal(pattern.substr(p), text.substr(i),
                                          ci)) {
                        return true;
                    }
                }
                return false;
            }

            if (pattern[p] == '*') {
                while (p < pLen && pattern[p] == '*')
                    ++p;
                if (p == pLen) {
                    for (size_t i = s; i < sLen; ++i) {
                        if (IsPathSeparator(text[i]))
                            return false;
                    }
                    return true;
                }
                for (size_t i = s; i <= sLen; ++i) {
                    if (i > s && IsPathSeparator(text[i - 1])) {
                        break;
                    }
                    if (GlobMatchInternal(pattern.substr(p), text.substr(i),
                                          ci)) {
                        return true;
                    }
                }
                return false;
            }

            if (s == sLen)
                return false;

            if (pattern[p] == '?') {
                if (IsPathSeparator(text[s]))
                    return false;
                ++p;
                ++s;
                continue;
            }

            if (pattern[p] == '[') {
                if (IsPathSeparator(text[s]))
                    return false;
                ++p;
                bool invert = false;
                if (p < pLen && (pattern[p] == '!' || pattern[p] == '^')) {
                    invert = true;
                    ++p;
                }
                bool match = false;
                char sc = ci ? static_cast<char>(std::tolower(
                                   static_cast<unsigned char>(text[s])))
                             : text[s];
                while (p < pLen && pattern[p] != ']') {
                    char c1 =
                        ci ? static_cast<char>(std::tolower(
                                 static_cast<unsigned char>(pattern[p++])))
                           : pattern[p++];
                    if (p + 1 < pLen && pattern[p] == '-' &&
                        pattern[p + 1] != ']') {
                        ++p;
                        char c2 =
                            ci ? static_cast<char>(std::tolower(
                                     static_cast<unsigned char>(pattern[p++])))
                               : pattern[p++];
                        if (sc >= c1 && sc <= c2)
                            match = true;
                    } else {
                        if (sc == c1)
                            match = true;
                    }
                }
                if (p < pLen)
                    ++p;
                if (match == invert)
                    return false;
                ++s;
                continue;
            }

            if (IsPathSeparator(pattern[p])) {
                if (!IsPathSeparator(text[s]))
                    return false;
                ++p;
                ++s;
                continue;
            }

            char pc = ci ? static_cast<char>(std::tolower(
                               static_cast<unsigned char>(pattern[p])))
                         : pattern[p];
            char sc = ci ? static_cast<char>(std::tolower(
                               static_cast<unsigned char>(text[s])))
                         : text[s];
            if (pc != sc)
                return false;
            ++p;
            ++s;
        }

        return s == sLen;
    }

    bool MatchGlob(std::string_view pattern, std::string_view text,
                   bool caseInsensitive) {
        return GlobMatchInternal(pattern, text, caseInsensitive);
    }

    std::string NormalizePath(std::string_view path) {
        std::string result(path);
        for (char& c : result) {
            if (c == '\\')
                c = '/';
        }
        size_t start = 0;
        while (start < result.size() && result[start] == '/') {
            ++start;
        }
        if (start > 0) {
            result.erase(0, start);
        }
        while (!result.empty() && result.back() == '/') {
            result.pop_back();
        }
        return result;
    }

    std::string NormalizePathForHash(std::string_view path) {
        std::string result(path);
        for (char& c : result) {
            if (c == '/')
                c = '\\';
        }
        size_t start = 0;
        while (start < result.size() && result[start] == '\\') {
            ++start;
        }
        if (start > 0) {
            result.erase(0, start);
        }
        while (!result.empty() && result.back() == '\\') {
            result.pop_back();
        }
        return result;
    }

    std::string NormalizePathForNfo(std::string_view path) {
        return NormalizePathForHash(path);
    }

    std::string EscapeXml(std::string_view str) {
        std::string result;
        result.reserve(str.size());
        for (char c : str) {
            switch (c) {
            case '&':
                result += "&amp;";
                break;
            case '<':
                result += "&lt;";
                break;
            case '>':
                result += "&gt;";
                break;
            case '"':
                result += "&quot;";
                break;
            case '\'':
                result += "&apos;";
                break;
            default:
                result += c;
                break;
            }
        }
        return result;
    }

    std::string UnescapeXml(std::string_view str) {
        std::string result;
        result.reserve(str.size());
        for (size_t i = 0; i < str.size(); ++i) {
            if (str[i] == '&') {
                if (str.substr(i, 5) == "&amp;") {
                    result += '&';
                    i += 4;
                } else if (str.substr(i, 4) == "&lt;") {
                    result += '<';
                    i += 3;
                } else if (str.substr(i, 4) == "&gt;") {
                    result += '>';
                    i += 3;
                } else if (str.substr(i, 6) == "&quot;") {
                    result += '"';
                    i += 5;
                } else if (str.substr(i, 6) == "&apos;") {
                    result += '\'';
                    i += 5;
                } else {
                    result += str[i];
                }
            } else {
                result += str[i];
            }
        }
        return result;
    }

    Platform ParsePlatformId(uint8_t rawPlatformId) {
        switch (rawPlatformId) {
        case 0:
            return Platform::Unknown;
        case 1:
            return Platform::Win32;
        case 2:
            return Platform::Win64;
        case 3:
            return Platform::Durango;
        case 4:
            return Platform::Orbis;
        case 5:
            return Platform::Scarlett;
        case 6:
            return Platform::Prospero;
        case 7:
            return Platform::Yeti;
        case 8:
            return Platform::Xenon;
        case 9:
            return Platform::PS3;
        case 10:
            return Platform::WiiU;
        default:
            return Platform::Unknown;
        }
    }

    uint8_t PlatformToId(Platform platform) {
        switch (platform) {
        case Platform::Win32:
            return 1;
        case Platform::Win64:
            return 2;
        case Platform::Durango:
            return 3;
        case Platform::Orbis:
            return 4;
        case Platform::Scarlett:
            return 5;
        case Platform::Prospero:
            return 6;
        case Platform::Yeti:
            return 7;
        case Platform::Xenon:
            return 8;
        case Platform::PS3:
            return 9;
        case Platform::WiiU:
            return 10;
        default:
            return 0;
        }
    }

    CompressionScheme ParseCompressionScheme(std::string_view str) {
        std::string lower(str);
        std::transform(
            lower.begin(), lower.end(), lower.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (lower == "none" || lower == "uncompressed" || lower == "0")
            return CompressionScheme::None;
        if (lower == "lzo" || lower == "lzo1x" || lower == "1")
            return CompressionScheme::LZO1x;
        if (lower == "zlib" || lower == "deflate" || lower == "2")
            return CompressionScheme::Zlib;
        if (lower == "xmem" || lower == "xmemcompress" || lower == "lzx" ||
            lower == "3")
            return CompressionScheme::XMemCompress;
        if (lower == "lzma" || lower == "4")
            return CompressionScheme::LZMA;
        if (lower == "lz4" || lower == "lz4lw" || lower == "5")
            return CompressionScheme::LZ4LW;
        if (lower == "oodle" || lower == "6")
            return CompressionScheme::Oodle;
        return CompressionScheme::Unknown;
    }

    CompressionScheme GetDefaultCompressionForVersion(ArchiveVersion version) {
        switch (version) {
        case ArchiveVersion::FATv2:
            return CompressionScheme::LZO1x;
        case ArchiveVersion::FATv3:
            return CompressionScheme::XMemCompress;
        case ArchiveVersion::FATv5:
            return CompressionScheme::LZ4LW;
        default:
            return CompressionScheme::None;
        }
    }

    CompressionScheme ToCompressionScheme(CompressionSchemeV0 scheme) {
        switch (scheme) {
        case CompressionSchemeV0::None:
            return CompressionScheme::None;
        default:
            return CompressionScheme::Unknown;
        }
    }

    CompressionScheme ToCompressionScheme(CompressionSchemeV4 scheme) {
        switch (scheme) {
        case CompressionSchemeV4::None:
            return CompressionScheme::None;
        case CompressionSchemeV4::LZO1x:
            return CompressionScheme::LZO1x;
        case CompressionSchemeV4::Zlib:
            return CompressionScheme::Zlib;
        default:
            return CompressionScheme::Unknown;
        }
    }

    CompressionScheme ToCompressionScheme(CompressionSchemeV5 scheme) {
        switch (scheme) {
        case CompressionSchemeV5::None:
            return CompressionScheme::None;
        case CompressionSchemeV5::LZO1x:
            return CompressionScheme::LZO1x;
        case CompressionSchemeV5::Zlib:
            return CompressionScheme::Zlib;
        case CompressionSchemeV5::XMemCompress:
            return CompressionScheme::XMemCompress;
        default:
            return CompressionScheme::Unknown;
        }
    }

    CompressionScheme ToCompressionScheme(CompressionSchemeV6 scheme) {
        switch (scheme) {
        case CompressionSchemeV6::None:
            return CompressionScheme::None;
        case CompressionSchemeV6::LZMA:
            return CompressionScheme::LZMA;
        case CompressionSchemeV6::LZ4LW:
            return CompressionScheme::LZ4LW;
        default:
            return CompressionScheme::Unknown;
        }
    }

    CompressionScheme ToCompressionScheme(CompressionSchemeV8 scheme) {
        switch (scheme) {
        case CompressionSchemeV8::None:
            return CompressionScheme::None;
        case CompressionSchemeV8::Oodle:
            return CompressionScheme::Oodle;
        case CompressionSchemeV8::LZMA:
            return CompressionScheme::LZMA;
        case CompressionSchemeV8::LZ4LW:
            return CompressionScheme::LZ4LW;
        default:
            return CompressionScheme::Unknown;
        }
    }

    CompressionScheme ToCompressionScheme(CompressionSchemeV9 scheme) {
        switch (scheme) {
        case CompressionSchemeV9::None:
            return CompressionScheme::None;
        case CompressionSchemeV9::Oodle:
            return CompressionScheme::Oodle;
        case CompressionSchemeV9::LZMA:
            return CompressionScheme::LZMA;
        case CompressionSchemeV9::LZ4LW:
            return CompressionScheme::LZ4LW;
        default:
            return CompressionScheme::Unknown;
        }
    }

    std::optional<CompressionSchemeV0>
    ToCompressionSchemeV0(CompressionScheme scheme) {
        switch (scheme) {
        case CompressionScheme::None:
            return CompressionSchemeV0::None;
        default:
            return std::nullopt;
        }
    }

    std::optional<CompressionSchemeV4>
    ToCompressionSchemeV4(CompressionScheme scheme) {
        switch (scheme) {
        case CompressionScheme::None:
            return CompressionSchemeV4::None;
        case CompressionScheme::LZO1x:
            return CompressionSchemeV4::LZO1x;
        case CompressionScheme::Zlib:
            return CompressionSchemeV4::Zlib;
        default:
            return std::nullopt;
        }
    }

    std::optional<CompressionSchemeV5>
    ToCompressionSchemeV5(CompressionScheme scheme) {
        switch (scheme) {
        case CompressionScheme::None:
            return CompressionSchemeV5::None;
        case CompressionScheme::LZO1x:
            return CompressionSchemeV5::LZO1x;
        case CompressionScheme::Zlib:
            return CompressionSchemeV5::Zlib;
        case CompressionScheme::XMemCompress:
            return CompressionSchemeV5::XMemCompress;
        default:
            return std::nullopt;
        }
    }

    std::optional<CompressionSchemeV6>
    ToCompressionSchemeV6(CompressionScheme scheme) {
        switch (scheme) {
        case CompressionScheme::None:
            return CompressionSchemeV6::None;
        case CompressionScheme::LZMA:
            return CompressionSchemeV6::LZMA;
        case CompressionScheme::LZ4LW:
            return CompressionSchemeV6::LZ4LW;
        default:
            return std::nullopt;
        }
    }

    std::optional<CompressionSchemeV8>
    ToCompressionSchemeV8(CompressionScheme scheme) {
        switch (scheme) {
        case CompressionScheme::None:
            return CompressionSchemeV8::None;
        case CompressionScheme::Oodle:
            return CompressionSchemeV8::Oodle;
        case CompressionScheme::LZMA:
            return CompressionSchemeV8::LZMA;
        case CompressionScheme::LZ4LW:
            return CompressionSchemeV8::LZ4LW;
        default:
            return std::nullopt;
        }
    }

    std::optional<CompressionSchemeV9>
    ToCompressionSchemeV9(CompressionScheme scheme) {
        switch (scheme) {
        case CompressionScheme::None:
            return CompressionSchemeV9::None;
        case CompressionScheme::Oodle:
            return CompressionSchemeV9::Oodle;
        case CompressionScheme::LZMA:
            return CompressionSchemeV9::LZMA;
        case CompressionScheme::LZ4LW:
            return CompressionSchemeV9::LZ4LW;
        default:
            return std::nullopt;
        }
    }

    CompressionScheme ToCompressionSchemeV3(uint8_t id,
                                            uint8_t compressionVersion) {
        switch (compressionVersion) {
        case 0:
            if (id == 0)
                return CompressionScheme::None;
            return CompressionScheme::Unknown;
        case 4:
            switch (id) {
            case 0:
                return CompressionScheme::None;
            case 1:
                return CompressionScheme::LZO1x;
            case 2:
                return CompressionScheme::Zlib;
            default:
                return CompressionScheme::Unknown;
            }
        case 5:
        default:
            switch (id) {
            case 0:
                return CompressionScheme::None;
            case 1:
                return CompressionScheme::LZO1x;
            case 2:
                return CompressionScheme::Zlib;
            case 3:
                return CompressionScheme::XMemCompress;
            default:
                return CompressionScheme::Unknown;
            }
        }
    }

    uint8_t FromCompressionSchemeV3(CompressionScheme scheme,
                                    uint8_t compressionVersion) {
        switch (compressionVersion) {
        case 0:
            return 0;
        case 4:
            switch (scheme) {
            case CompressionScheme::None:
                return 0;
            case CompressionScheme::LZO1x:
                return 1;
            case CompressionScheme::Zlib:
                return 2;
            default:
                return 0;
            }
        case 5:
        default:
            switch (scheme) {
            case CompressionScheme::None:
                return 0;
            case CompressionScheme::LZO1x:
                return 1;
            case CompressionScheme::Zlib:
                return 2;
            case CompressionScheme::XMemCompress:
                return 3;
            default:
                return 0;
            }
        }
    }

    CompressionScheme ToCompressionSchemeV5(uint8_t id,
                                            uint8_t compressionVersion) {
        switch (compressionVersion) {
        case 0:
            if (id == 0)
                return CompressionScheme::None;
            return CompressionScheme::Unknown;
        case 6:
            switch (id) {
            case 0:
                return CompressionScheme::None;
            case 1:
                return CompressionScheme::LZMA;
            case 2:
                return CompressionScheme::LZ4LW;
            default:
                return CompressionScheme::Unknown;
            }
        case 8:
        case 9:
            switch (id) {
            case 0:
                return CompressionScheme::None;
            case 1:
                return CompressionScheme::Oodle;
            case 2:
                return CompressionScheme::LZMA;
            case 3:
                return CompressionScheme::LZ4LW;
            default:
                return CompressionScheme::Unknown;
            }
        default:
            if (id == 0)
                return CompressionScheme::None;
            switch (id) {
            case 1:
                return CompressionScheme::Oodle;
            case 2:
                return CompressionScheme::LZMA;
            case 3:
                return CompressionScheme::LZ4LW;
            default:
                return CompressionScheme::Unknown;
            }
        }
    }

    uint8_t FromCompressionSchemeV5(CompressionScheme scheme,
                                    uint8_t compressionVersion) {
        if (scheme == CompressionScheme::None)
            return 0;
        switch (compressionVersion) {
        case 0:
            return 0;
        case 6:
            switch (scheme) {
            case CompressionScheme::LZMA:
                return 1;
            case CompressionScheme::LZ4LW:
                return 2;
            default:
                return 0;
            }
        case 8:
        case 9:
        default:
            switch (scheme) {
            case CompressionScheme::Oodle:
                return 1;
            case CompressionScheme::LZMA:
                return 2;
            case CompressionScheme::LZ4LW:
                return 3;
            default:
                return 0;
            }
        }
    }

    bool ValidateArchiveEntries(std::string_view archiveName,
                                uint32_t rawVersion,
                                const std::vector<ArchiveEntry>& entries,
                                const ArchiveCapacityLimits& limits,
                                std::string& errorMsg) {
        errorMsg.clear();
        for (const auto& entry : entries) {
            std::string_view pathDesc = entry.path.empty()
                                            ? std::string_view("<unnamed>")
                                            : std::string_view(entry.path);

            if (entry.uncompressed_size > limits.maxUncompressedSize) {
                errorMsg = std::format(
                    "File '{}' uncompressed size ({:.2f} MB / {} bytes) "
                    "exceeds {} "
                    "(raw "
                    "v{}) maximum supported size of {:.2f} MB ({} bytes)",
                    pathDesc,
                    static_cast<double>(entry.uncompressed_size) /
                        (1024.0 * 1024.0),
                    entry.uncompressed_size, archiveName, rawVersion,
                    static_cast<double>(limits.maxUncompressedSize) /
                        (1024.0 * 1024.0),
                    limits.maxUncompressedSize);
                return false;
            }

            if (entry.compressed_size > limits.maxCompressedSize) {
                errorMsg = std::format(
                    "File '{}' compressed size ({:.2f} MB / {} bytes) exceeds "
                    "{} (raw "
                    "v{}) maximum supported size of {:.2f} MB ({} bytes)",
                    pathDesc,
                    static_cast<double>(entry.compressed_size) /
                        (1024.0 * 1024.0),
                    entry.compressed_size, archiveName, rawVersion,
                    static_cast<double>(limits.maxCompressedSize) /
                        (1024.0 * 1024.0),
                    limits.maxCompressedSize);
                return false;
            }

            if (entry.offset > limits.maxOffset) {
                errorMsg = std::format(
                    "File '{}' offset ({:.2f} GB / {} bytes) exceeds {} (raw "
                    "v{}) "
                    "maximum addressable offset of {:.2f} GB ({} bytes)",
                    pathDesc,
                    static_cast<double>(entry.offset) /
                        (1024.0 * 1024.0 * 1024.0),
                    entry.offset, archiveName, rawVersion,
                    static_cast<double>(limits.maxOffset) /
                        (1024.0 * 1024.0 * 1024.0),
                    limits.maxOffset);
                return false;
            }
        }
        return true;
    }
} // namespace Nomad::Archives
