#pragma once

#include "Nomad/Archives/IArchive.hpp"
#include "Nomad/Archives/ProjectConfig.hpp"
#include <string>
#include <vector>

namespace Nomad::Archives {
    struct FatItem {
        uint64_t archiveHash{0};
        uint64_t nameHash{0};
    };

#pragma pack(push, 1)
    struct ExtendedFat {
        uint64_t hash{0};
        uint32_t compressedSize{0};
        uint32_t uncompressedSize{0};
        uint64_t offset{0};
        uint32_t compressionScheme{0};
    };
#pragma pack(pop)
    static_assert(sizeof(ExtendedFat) == 28, "BigDuplicate must be 28 bytes");

    struct FatLocalization {
        std::string name;
        uint64_t unknown{0};
    };

    class FATv5Archive : public IArchive {
      public:
        static constexpr uint32_t Signature = 0x46415435; // 'FAT5'

        FATv5Archive() = default;
        ~FATv5Archive() override = default;

        [[nodiscard]] ArchiveVersion GetVersion() const override {
            return ArchiveVersion::FATv5;
        }
        [[nodiscard]] Platform GetPlatform() const override {
            return m_platform;
        }
        [[nodiscard]] int GetRawPlatform() const override {
            return m_rawPlatform;
        }
        void SetPlatform(Platform platform) override;
        [[nodiscard]] uint8_t GetCompressionVersion() const override {
            return m_compressionVersion;
        }
        void SetCompressionVersion(uint8_t version) override {
            m_compressionVersion = version;
        }
        [[nodiscard]] uint8_t GetNameHashVersion() const override {
            return m_nameHashVersion;
        }
        void SetNameHashVersion(uint8_t version) override {
            m_nameHashVersion = version;
        }
        [[nodiscard]] uint32_t GetRawVersion() const override {
            return m_rawVersion;
        }
        void SetRawVersion(uint32_t version) override;

        [[nodiscard]] uint64_t GetArchiveHash() const { return m_archiveHash; }
        void SetArchiveHash(uint64_t hash) { m_archiveHash = hash; }
        [[nodiscard]] const std::vector<FatItem>& GetDependencies() const {
            return m_dependencies;
        }
        [[nodiscard]] std::vector<FatItem>& GetDependencies() {
            return m_dependencies;
        }
        [[nodiscard]] const std::vector<ExtendedFat>& GetDuplicates() const {
            return m_duplicates;
        }
        [[nodiscard]] std::vector<ExtendedFat>& GetDuplicates() {
            return m_duplicates;
        }
        [[nodiscard]] const std::vector<FatLocalization>&
        GetLocalizations() const {
            return m_localizations;
        }
        [[nodiscard]] std::vector<FatLocalization>& GetLocalizations() {
            return m_localizations;
        }

        [[nodiscard]] const std::vector<ArchiveEntry>&
        GetEntries() const override {
            return m_entries;
        }
        [[nodiscard]] std::vector<ArchiveEntry>& GetEntries() override {
            return m_entries;
        }

        bool Read(std::istream& stream) override;
        bool Write(std::ostream& stream) const override;
        bool SerializeNfo(std::ostream& stream) const override;

        [[nodiscard]] ArchiveCapacityLimits GetCapacityLimits() const override;
        [[nodiscard]] bool Validate(std::string& errorMsg) const override;

        [[nodiscard]] uint64_t
        ComputeNameHash(std::string_view name) const override;
        [[nodiscard]] uint64_t
        ComputeRawNameHash(std::string_view name) const override;
        [[nodiscard]] std::string RenderNameHash(uint64_t hash) const override;
        [[nodiscard]] CompressionScheme
        ToCompressionScheme(uint8_t id) const override;
        [[nodiscard]] CompressionScheme
        ToCompressionScheme(uint8_t id, uint64_t compressedSize,
                            uint64_t uncompressedSize) const override;
        [[nodiscard]] uint8_t
        FromCompressionScheme(CompressionScheme scheme) const override;

        void AddOrUpdateEntry(const std::string& path,
                              uint64_t uncompressedSize,
                              uint64_t compressedSize,
                              CompressionScheme compression,
                              uint64_t offset) override;

        void SetProjectConfig(std::shared_ptr<ProjectConfig> config) override {
            m_projectConfig = std::move(config);
        }
        [[nodiscard]] std::shared_ptr<ProjectConfig>
        GetProjectConfig() const override {
            return m_projectConfig;
        }

      private:
        uint32_t m_rawVersion{11};
        Platform m_platform{Platform::Win64};
        int m_rawPlatform{0};
        uint8_t m_compressionVersion{6};
        uint8_t m_nameHashVersion{70};
        uint64_t m_archiveHash{0xFFFFFFFFFFFFFFFFULL};
        std::vector<FatItem> m_dependencies;
        std::vector<ArchiveEntry> m_entries;
        std::vector<ExtendedFat> m_duplicates;
        std::vector<FatLocalization> m_localizations;
        std::shared_ptr<ProjectConfig> m_projectConfig;
    };

} // namespace Nomad::Archives
