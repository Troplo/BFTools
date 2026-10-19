//
// Created by troplo on 09/21/26.
//

#ifndef NOMADARCHIVES_IARCHIVE_H
#define NOMADARCHIVES_IARCHIVE_H

#include "Nomad/Archives/Types.hpp"
#include <iosfwd>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Nomad::Archives {
    class ProjectConfig;

    class IArchive {
      public:
        virtual ~IArchive() = default;

        [[nodiscard]] virtual ArchiveVersion GetVersion() const = 0;
        [[nodiscard]] virtual Platform GetPlatform() const = 0;
        [[nodiscard]] virtual int GetRawPlatform() const = 0;
        virtual void SetPlatform(Platform platform) {}
        [[nodiscard]] virtual uint8_t GetCompressionVersion() const = 0;
        virtual void SetCompressionVersion(uint8_t version) {}
        [[nodiscard]] virtual uint8_t GetNameHashVersion() const = 0;
        virtual void SetNameHashVersion(uint8_t version) {}
        [[nodiscard]] virtual uint32_t GetRawVersion() const = 0;
        virtual void SetRawVersion(uint32_t version) {}

        [[nodiscard]] virtual const std::vector<ArchiveEntry>&
        GetEntries() const = 0;
        [[nodiscard]] virtual std::vector<ArchiveEntry>& GetEntries() = 0;

        virtual bool Read(std::istream& stream) = 0;
        virtual bool Write(std::ostream& stream) const = 0;
        virtual bool SerializeNfo(std::ostream& stream) const = 0;

        [[nodiscard]] virtual ArchiveCapacityLimits
        GetCapacityLimits() const = 0;
        [[nodiscard]] virtual bool Validate(std::string& errorMsg) const = 0;

        [[nodiscard]] virtual uint64_t
        ComputeNameHash(std::string_view name) const = 0;
        [[nodiscard]] virtual uint64_t
        ComputeRawNameHash(std::string_view name) const {
            return ComputeNameHash(name);
        }
        [[nodiscard]] virtual std::string
        RenderNameHash(uint64_t hash) const = 0;
        [[nodiscard]] virtual CompressionScheme
        ToCompressionScheme(uint8_t id) const = 0;
        [[nodiscard]] virtual CompressionScheme
        ToCompressionScheme(uint8_t id, uint64_t compressedSize,
                            uint64_t uncompressedSize) const {
            return ToCompressionScheme(id);
        }
        [[nodiscard]] virtual uint8_t
        FromCompressionScheme(CompressionScheme scheme) const {
            return 0;
        }

        virtual void AddOrUpdateEntry(const std::string& path,
                                      uint64_t uncompressed_size,
                                      uint64_t compressed_size,
                                      CompressionScheme compression,
                                      uint64_t offset) = 0;

        virtual void
        SetProjectConfig(std::shared_ptr<ProjectConfig> config) = 0;
        [[nodiscard]] virtual std::shared_ptr<ProjectConfig>
        GetProjectConfig() const = 0;
    };

} // namespace Nomad::Archives

#endif // NOMADARCHIVES_IARCHIVE_H
