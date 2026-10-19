//
// Created by troplo on 10/14/26.
//

#ifndef NOMADARCHIVES_PROJECTMANAGER_H
#define NOMADARCHIVES_PROJECTMANAGER_H

#include "Nomad/Archives/ProjectConfig.hpp"
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Nomad::Archives {
    class ProjectManager {
      public:
        static ProjectManager& GetInstance();

        void SetProjectsRoot(const std::filesystem::path& path);
        [[nodiscard]] std::filesystem::path GetProjectsRoot() const;

        // Discover all candidate root directories where 'projects' might be
        // stored
        [[nodiscard]] std::vector<std::filesystem::path>
        GetCandidateProjectRoots(
            const std::filesystem::path& workspaceRoot = "") const;

        // Discover all available project names
        [[nodiscard]] std::vector<std::string> GetAvailableProjects(
            const std::filesystem::path& workspaceRoot = "") const;

        // Get or load a project by name
        std::shared_ptr<ProjectConfig>
        GetProject(const std::string& name,
                   const std::filesystem::path& workspaceRoot = "");

        // Path to a specific project folder
        [[nodiscard]] std::filesystem::path
        GetProjectPath(const std::string& name,
                       const std::filesystem::path& workspaceRoot = "") const;

        void ClearCache();

      private:
        ProjectManager() = default;
        ~ProjectManager() = default;
        ProjectManager(const ProjectManager&) = delete;
        ProjectManager& operator=(const ProjectManager&) = delete;

        std::filesystem::path m_customProjectsRoot;
        std::unordered_map<std::string, std::shared_ptr<ProjectConfig>>
            m_cachedProjects;
    };

} // namespace Nomad::Archives

#endif // NOMADARCHIVES_PROJECTMANAGER_H
