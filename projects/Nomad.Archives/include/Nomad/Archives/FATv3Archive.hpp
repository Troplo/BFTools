//
// Created by troplo on 09/21/26.
//

#ifndef NOMADARCHIVES_FATV3ARCHIVE_H
#define NOMADARCHIVES_FATV3ARCHIVE_H

#include "Nomad/Archives/IArchive.hpp"
#include "Nomad/Archives/ProjectConfig.hpp"

namespace Nomad::Archives {
    class FATv3Archive : public IArchive {
      public:
        static constexpr uint32_t Signature = 0x46415433; // 'FAT3'

        FATv3Archive() = default;
        ~FATv3Archive() override = default;

        [[nodiscard]] ArchiveVersion GetVersion() const override {
            return ArchiveVersion::FATv3;
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
        void SetRawVersion(uint32_t version) override {
            if (version == 7 || version == 8)
                m_rawVersion = version;
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
        uint32_t m_rawVersion{8};
        Platform m_platform{Platform::Win64};
        uint8_t m_compressionVersion{5};
        uint8_t m_nameHashVersion{50};
        std::vector<ArchiveEntry> m_entries;
        std::shared_ptr<ProjectConfig> m_projectConfig;

        // raw
        int m_rawPlatform{0};
    };

} // namespace Nomad::Archives

#endif // NOMADARCHIVES_FATV3ARCHIVE_H
