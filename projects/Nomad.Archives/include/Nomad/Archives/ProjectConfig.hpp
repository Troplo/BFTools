//
// Created by troplo on 10/14/26.
//

#ifndef NOMADARCHIVES_PROJECTCONFIG_H
#define NOMADARCHIVES_PROJECTCONFIG_H

#include "Nomad/Archives/Types.hpp"
#include <cstdint>
#include <deque>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Nomad::Archives {
    class ProjectConfig {
      public:
        struct FilelistIndexEntry {
            uint64_t hash{0};
            uint32_t lineIndex{0};
        };

        ProjectConfig() = default;
        explicit ProjectConfig(std::string name);

        [[nodiscard]] const std::string &GetName() const { return m_name; }
        void SetName(std::string name) { m_name = std::move(name); }

        [[nodiscard]] const std::filesystem::path &GetProjectPath() const {
            return m_projectPath;
        }
        void SetProjectPath(std::filesystem::path path) {
            m_projectPath = std::move(path);
        }

        [[nodiscard]] ArchiveVersion GetDefaultFatVersion() const {
            return m_fatVersion;
        }
        void SetDefaultFatVersion(ArchiveVersion version) {
            m_fatVersion = version;
        }

        [[nodiscard]] uint32_t GetDefaultRawVersion() const {
            return m_rawVersion;
        }
        void SetDefaultRawVersion(uint32_t rawVersion) {
            m_rawVersion = rawVersion;
        }

        [[nodiscard]] const std::unordered_map<uint64_t, std::string> &
        GetOverrides() const {
            return m_hashToPath;
        }
        void AddOverride(uint64_t hash, std::string_view path);
        void RemoveOverride(uint64_t hash);

        [[nodiscard]] const std::vector<std::string_view> &
        GetFilelistLines() const {
            return m_filelistLines;
        }
        void SetFilelistLines(std::vector<std::string> lines);
        void SetFilelistLines(std::vector<std::string_view> lines);
        void ClearFilelists();

        void EnsureFilelistIndexed(ArchiveVersion version);
        [[nodiscard]] std::optional<std::string_view>
        FindFilelistPath(uint64_t hash, ArchiveVersion version) const;
        void InvalidateIndex();

        [[nodiscard]] std::optional<std::string>
        FindPathByHash(uint64_t hash, bool is32Bit = false) const;
        [[nodiscard]] std::optional<uint64_t>
        FindHashByPath(std::string_view path) const;

        bool LoadFilelist(const std::filesystem::path &filelistPath);
        bool LoadFromDirectory(const std::filesystem::path &projectDir);
        bool LoadConfigFile(const std::filesystem::path &jsonPath);
        bool LoadConfigJson(std::string_view jsonContent);
        bool SaveConfigFile(const std::filesystem::path &jsonPath) const;

        static std::optional<uint64_t> ParseHashString(std::string_view str);

      private:
        std::string m_name;
        std::filesystem::path m_projectPath;
        ArchiveVersion m_fatVersion{ArchiveVersion::Unknown};
        uint32_t m_rawVersion{0};

        std::unordered_map<uint64_t, std::string> m_hashToPath;
        std::unordered_map<uint32_t, std::string> m_hash32ToPath;
        std::unordered_map<std::string, uint64_t>
            m_pathToHash; // Normalized lowercase path to hash

        std::deque<std::string> m_filelistBuffers;
        std::vector<std::string_view> m_filelistLines;

        mutable std::mutex m_indexMutex;
        ArchiveVersion m_indexedVersion{ArchiveVersion::Unknown};
        std::vector<FilelistIndexEntry> m_indexedFilelist;
    };

} // namespace Nomad::Archives

#endif // NOMADARCHIVES_PROJECTCONFIG_H
