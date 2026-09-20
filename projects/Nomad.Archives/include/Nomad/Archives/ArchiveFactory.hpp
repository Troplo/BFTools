//
// Created by troplo on 09/21/26.
//

#ifndef NOMADARCHIVES_ARCHIVEFACTORY_H
#define NOMADARCHIVES_ARCHIVEFACTORY_H

#include "Nomad/Archives/IArchive.hpp"
#include "Nomad/Archives/ProjectConfig.hpp"
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>

namespace Nomad::Archives {
    class ArchiveFactory {
      public:
        static ArchiveVersion
        DetectVersion(const std::filesystem::path &fatPath);
        static uint32_t DetectRawVersion(const std::filesystem::path &fatPath);
        static std::unique_ptr<IArchive> Create(ArchiveVersion version);

        static std::unique_ptr<IArchive>
        Open(const std::filesystem::path &fatPath, std::error_code &ec,
             const std::string &projectName = "",
             const std::filesystem::path &workspaceRoot = "");

        static std::unique_ptr<IArchive>
        OpenWithProject(const std::filesystem::path &fatPath,
                        std::error_code &ec,
                        const std::shared_ptr<ProjectConfig> &project,
                        const std::filesystem::path &workspaceRoot = "");

        static bool
        ApplyProject(IArchive &archive, const std::string &projectName,
                     const std::filesystem::path &workspaceRoot = "");

        static bool ApplyProject(IArchive &archive,
                                 const std::shared_ptr<ProjectConfig> &project);

        static uint64_t ComputeRawNameHash(ArchiveVersion version,
                                           std::string_view path);

        static bool LoadNfoNames(const std::filesystem::path &nfoPath,
                                 IArchive &archive);
        static bool
        LoadProjectFilelists(const std::filesystem::path &workspaceRoot,
                             IArchive &archive);
    };

} // namespace Nomad::Archives

#endif // NOMADARCHIVES_ARCHIVEFACTORY_H
