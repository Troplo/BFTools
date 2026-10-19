//
// Created by troplo on 10/14/26.
//

#include "Nomad/Archives/ProjectManager.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <set>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__linux__)
#include <unistd.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace Nomad::Archives {
    static std::filesystem::path GetExecutablePath() {
#if defined(__linux__)
        std::error_code ec;
        auto exePath = std::filesystem::canonical("/proc/self/exe", ec);
        if (!ec && !exePath.empty()) {
            return exePath;
        }
        ec.clear();
        exePath = std::filesystem::read_symlink("/proc/self/exe", ec);
        if (!ec && !exePath.empty()) {
            return exePath;
        }
#elif defined(_WIN32)
        std::vector<wchar_t> buffer(MAX_PATH);
        while (true) {
            DWORD length = GetModuleFileNameW(
                nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
            if (length == 0) {
                break;
            }
            if (length < buffer.size()) {
                std::error_code ec;
                std::filesystem::path p(buffer.data(), buffer.data() + length);
                auto canonical = std::filesystem::canonical(p, ec);
                return ec ? p : canonical;
            }
            buffer.resize(buffer.size() * 2);
        }
#elif defined(__APPLE__)
        uint32_t size = 0;
        _NSGetExecutablePath(nullptr, &size);
        if (size > 0) {
            std::vector<char> buffer(size);
            if (_NSGetExecutablePath(buffer.data(), &size) == 0) {
                std::error_code ec;
                auto p = std::filesystem::canonical(buffer.data(), ec);
                return ec ? std::filesystem::path(buffer.data()) : p;
            }
        }
#endif
        return {};
    }

    ProjectManager& ProjectManager::GetInstance() {
        static ProjectManager instance;
        return instance;
    }

    void ProjectManager::SetProjectsRoot(const std::filesystem::path& path) {
        m_customProjectsRoot = path;
        m_cachedProjects.clear();
    }

    std::filesystem::path ProjectManager::GetProjectsRoot() const {
        // 1. Explicitly configured path by user
        if (!m_customProjectsRoot.empty()) {
            std::error_code ec;
            if (std::filesystem::exists(m_customProjectsRoot, ec) &&
                std::filesystem::is_directory(m_customProjectsRoot, ec)) {
                return m_customProjectsRoot;
            }
        }

        // 2. Return first candidate project root that actually exists and is a
        // directory
        auto candidates = GetCandidateProjectRoots();
        for (const auto& candidate : candidates) {
            std::error_code ec;
            if (std::filesystem::exists(candidate, ec) &&
                std::filesystem::is_directory(candidate, ec)) {
                return candidate;
            }
        }

        auto exe = GetExecutablePath();
        if (!exe.empty()) {
            return exe.parent_path() / "projects";
        }

        return std::filesystem::current_path() / "projects";
    }

    std::vector<std::filesystem::path> ProjectManager::GetCandidateProjectRoots(
        const std::filesystem::path& workspaceRoot) const {
        std::vector<std::filesystem::path> candidates;
        std::set<std::string> seen;

        auto addCandidate = [&](const std::filesystem::path& p) {
            if (p.empty())
                return;
            std::error_code ec;
            auto canonical = std::filesystem::weakly_canonical(p, ec);
            std::string s = canonical.string();
            if (!seen.contains(s)) {
                seen.insert(s);
                candidates.push_back(canonical);
            }
        };

        // 1. Custom explicit path configured by user/caller
        if (!m_customProjectsRoot.empty()) {
            addCandidate(m_customProjectsRoot);
        }

        auto exe = GetExecutablePath();
        if (!exe.empty()) {
            auto cur = exe.parent_path();
            for (int depth = 0; depth < 5; ++depth) {
                addCandidate(cur / "projects");
                addCandidate(cur / "common" / "projects");
                if (!cur.has_parent_path() || cur == cur.parent_path()) {
                    break;
                }
                cur = cur.parent_path();
            }
        }

        if (!workspaceRoot.empty()) {
            addCandidate(workspaceRoot / "projects");
            addCandidate(workspaceRoot / "common" / "projects");
            if (workspaceRoot.filename() == "projects") {
                addCandidate(workspaceRoot);
            }
        }

        auto cwd = std::filesystem::current_path();
        addCandidate(cwd / "projects");
        addCandidate(cwd / "common" / "projects");
        if (cwd.filename() == "projects") {
            addCandidate(cwd);
        }

#ifdef BF_DEFAULT_PROJECTS_DIR
        addCandidate(BF_DEFAULT_PROJECTS_DIR);
        addCandidate(std::filesystem::path(BF_DEFAULT_PROJECTS_DIR) /
                     "release" / "common" / "projects");
        addCandidate(std::filesystem::path(BF_DEFAULT_PROJECTS_DIR) / "common" /
                     "projects");
        addCandidate(
            std::filesystem::path(BF_DEFAULT_PROJECTS_DIR).parent_path() /
            "release" / "common" / "projects");
#endif

        return candidates;
    }

    std::vector<std::string> ProjectManager::GetAvailableProjects(
        const std::filesystem::path& workspaceRoot) const {
        std::set<std::string> projectNames;
        auto roots = GetCandidateProjectRoots(workspaceRoot);

        for (const auto& root : roots) {
            std::error_code ec;
            for (const auto& entry :
                 std::filesystem::directory_iterator(root, ec)) {
                if (!entry.is_directory(ec))
                    continue;

                std::string folderName = entry.path().filename().string();

                bool hasConfig = std::filesystem::exists(
                    entry.path() / "projectconfig.json", ec);
                bool hasFilelist =
                    std::filesystem::exists(entry.path() / "filelist.txt.gz",
                                            ec) ||
                    std::filesystem::exists(entry.path() / "filelist.txt", ec);
                bool hasAnyFilelist = false;

                if (!hasFilelist) {
                    for (const auto& subEntry :
                         std::filesystem::directory_iterator(entry.path(),
                                                             ec)) {
                        if (subEntry.is_regular_file(ec)) {
                            auto ext = subEntry.path().extension();
                            std::string fname =
                                subEntry.path().filename().string();
                            if (ext == ".filelist" ||
                                fname.ends_with(".filelist.gz") ||
                                fname.ends_with(".txt.gz")) {
                                hasAnyFilelist = true;
                                break;
                            }
                        }
                    }
                }

                if (hasConfig || hasFilelist || hasAnyFilelist) {
                    projectNames.insert(folderName);
                }
            }
        }

        return std::vector<std::string>(projectNames.begin(),
                                        projectNames.end());
    }

    std::filesystem::path ProjectManager::GetProjectPath(
        const std::string& name,
        const std::filesystem::path& workspaceRoot) const {
        if (name.empty())
            return {};

        std::error_code ec;
        // Direct path check if user passed a relative/absolute folder path
        std::filesystem::path directPath(name);
        if (std::filesystem::exists(directPath, ec) &&
            std::filesystem::is_directory(directPath, ec)) {
            return directPath;
        }

        auto roots = GetCandidateProjectRoots(workspaceRoot);

        // Pass 1: exact match
        for (const auto& root : roots) {
            auto candidate = root / name;
            if (std::filesystem::exists(candidate, ec) &&
                std::filesystem::is_directory(candidate, ec)) {
                return candidate;
            }
        }

        // Pass 2: case-insensitive match (helpful on Linux or case-mismatched
        // CLI inputs)
        for (const auto& root : roots) {
            if (!std::filesystem::exists(root, ec) ||
                !std::filesystem::is_directory(root, ec))
                continue;
            for (const auto& entry :
                 std::filesystem::directory_iterator(root, ec)) {
                if (entry.is_directory(ec)) {
                    std::string folder = entry.path().filename().string();
                    if (folder.size() == name.size() &&
                        std::equal(
                            folder.begin(), folder.end(), name.begin(),
                            [](char a, char b) {
                                return std::tolower(
                                           static_cast<unsigned char>(a)) ==
                                       std::tolower(
                                           static_cast<unsigned char>(b));
                            })) {
                        return entry.path();
                    }
                }
            }
        }

        return {};
    }

    std::shared_ptr<ProjectConfig>
    ProjectManager::GetProject(const std::string& name,
                               const std::filesystem::path& workspaceRoot) {
        if (name.empty())
            return nullptr;

        auto it = m_cachedProjects.find(name);
        if (it != m_cachedProjects.end()) {
            return it->second;
        }

        auto path = GetProjectPath(name, workspaceRoot);
        if (path.empty()) {
            return nullptr;
        }

        auto config = std::make_shared<ProjectConfig>();
        if (!config->LoadFromDirectory(path)) {
            return nullptr;
        }

        m_cachedProjects[name] = config;
        return config;
    }

    void ProjectManager::ClearCache() { m_cachedProjects.clear(); }

} // namespace Nomad::Archives
