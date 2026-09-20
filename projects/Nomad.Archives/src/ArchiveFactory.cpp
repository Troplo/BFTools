//
// Created by troplo on 09/21/26.
//

#include "Nomad/Archives/ArchiveFactory.hpp"
#include "Errors.h"
#include "Nomad/Archives/FATv2Archive.hpp"
#include "Nomad/Archives/FATv3Archive.hpp"
#include "Nomad/Archives/FATv5Archive.hpp"
#include "Nomad/Archives/HashingUtils.hpp"
#include "Nomad/Archives/Logger.hpp"
#include "Nomad/Archives/ProjectManager.hpp"
#include "Nomad/Archives/ThreadPool.hpp"
#include "features.h"
#include <fstream>
#include <future>
#include <iostream>
#include <pugixml.hpp>
#include <tracy/Tracy.hpp>
#include <unordered_map>
#include <vector>

namespace Nomad::Archives {
    ArchiveVersion
    ArchiveFactory::DetectVersion(const std::filesystem::path &fatPath) {
        std::ifstream file(fatPath, std::ios::binary);
        if (!file.is_open()) {
            Logger::Warn("ArchiveFactory",
                         "Failed to open '{}' to detect version",
                         fatPath.string());
            return ArchiveVersion::Unknown;
        }

        uint32_t magic = 0;
        file.read(reinterpret_cast<char *>(&magic), sizeof(magic));
        if (!file) {
            Logger::Warn("ArchiveFactory",
                         "Failed to read header magic from '{}'",
                         fatPath.string());
            return ArchiveVersion::Unknown;
        }

        ArchiveVersion ver = ArchiveVersion::Unknown;
        if (magic == FATv5Archive::Signature)
            ver = ArchiveVersion::FATv5;
        else if (magic == FATv3Archive::Signature)
            ver = ArchiveVersion::FATv3;
        else if (magic == FATv2Archive::Signature)
            ver = ArchiveVersion::FATv2;

        Logger::Debug("ArchiveFactory",
                      "Detected version {} (magic: 0x{:08X}) for '{}'",
                      VersionToString(ver), magic, fatPath.string());
        return ver;
    }

    uint32_t
    ArchiveFactory::DetectRawVersion(const std::filesystem::path &fatPath) {
        std::ifstream file(fatPath, std::ios::binary);
        if (!file.is_open()) {
            return 0;
        }

        uint32_t magic = 0;
        file.read(reinterpret_cast<char *>(&magic), sizeof(magic));
        if (!file) {
            return 0;
        }

        if (magic != FATv2Archive::Signature &&
            magic != FATv3Archive::Signature &&
            magic != FATv5Archive::Signature) {
            return 0;
        }

        uint32_t rawVersion = 0;
        file.read(reinterpret_cast<char *>(&rawVersion), sizeof(rawVersion));
        if (!file) {
            return 0;
        }

        return rawVersion;
    }

    std::unique_ptr<IArchive> ArchiveFactory::Create(ArchiveVersion version) {
        switch (version) {
        case ArchiveVersion::FATv2:
            if constexpr (ENABLE_FAT2)
                return std::make_unique<FATv2Archive>();
            break;
        case ArchiveVersion::FATv3:
            if constexpr (ENABLE_FAT3)
                return std::make_unique<FATv3Archive>();
            break;
        case ArchiveVersion::FATv5:
            if constexpr (ENABLE_FAT5_WD2 || ENABLE_FAT5_ORWELL)
                return std::make_unique<FATv5Archive>();
            break;
        default:
            break;
        }
        return nullptr;
    }

    uint64_t ArchiveFactory::ComputeRawNameHash(ArchiveVersion version,
                                                std::string_view path) {
        switch (version) {
        case ArchiveVersion::FATv2:
            return HashingUtils::ComputeCRC64Path(path);
        case ArchiveVersion::FATv3:
            return HashingUtils::ComputeFNV32Path(path);
        case ArchiveVersion::FATv5:
            return HashingUtils::ComputeFNV64Path(path);
        default:
            return 0;
        }
    }

    std::unique_ptr<IArchive>
    ArchiveFactory::Open(const std::filesystem::path &fatPath,
                         std::error_code &ec, const std::string &projectName,
                         const std::filesystem::path &workspaceRoot) {
        std::shared_ptr<ProjectConfig> proj;
        if (!projectName.empty()) {
            std::filesystem::path ws = workspaceRoot;
            if (ws.empty()) {
                ws = fatPath.parent_path();
            }
            proj = ProjectManager::GetInstance().GetProject(projectName, ws);
            if (!proj) {
                Logger::Warn("ArchiveFactory",
                             "Project '{}' was requested but could not be "
                             "found or loaded",
                             projectName);
            }
        }
        return OpenWithProject(fatPath, ec, proj, workspaceRoot);
    }

    std::unique_ptr<IArchive> ArchiveFactory::OpenWithProject(
        const std::filesystem::path &fatPath, std::error_code &ec,
        const std::shared_ptr<ProjectConfig> &project,
        const std::filesystem::path &workspaceRoot) {
        ZoneScopedN("ArchiveFactory::OpenWithProject");
        if (!std::filesystem::exists(fatPath)) {
            ec = std::make_error_code(std::errc::no_such_file_or_directory);
            Logger::Error("ArchiveFactory",
                          "FAT archive file does not exist: {}",
                          fatPath.string());
            return nullptr;
        }

        ArchiveVersion ver = DetectVersion(fatPath);
        if (ver == ArchiveVersion::Unknown) {
            ec = make_error_code(ErrorCodes::InvalidFAT);
            Logger::Error("ArchiveFactory",
                          "Unknown or corrupt FAT archive format: {}",
                          fatPath.string());
            return nullptr;
        }

        auto archive = Create(ver);
        if (!archive) {
            ec = make_error_code(ErrorCodes::InvalidFAT);
            Logger::Error(
                "ArchiveFactory",
                "Archive version {} is disabled or unsupported in this build",
                VersionToString(ver));
            return nullptr;
        }

        std::ifstream file(fatPath, std::ios::binary);
        if (!file.is_open()) {
            ec = make_error_code(ErrorCodes::ErrorReadingFile);
            Logger::Error("ArchiveFactory",
                          "Failed to open FAT archive file: {}",
                          fatPath.string());
            return nullptr;
        }

        if (!archive->Read(file)) {
            ec = make_error_code(ErrorCodes::ErrorReadingFile);
            Logger::Error("ArchiveFactory",
                          "Failed to read FAT archive header/data: {}",
                          fatPath.string());
            return nullptr;
        }

        std::filesystem::path nfoPath = fatPath;
        nfoPath.replace_extension(".nfo");
        if (std::filesystem::exists(nfoPath)) {
            LoadNfoNames(nfoPath, *archive);
        }

        if (project) {
            ApplyProject(*archive, project);
        }

        Logger::Info(
            "ArchiveFactory",
            "Loaded archive '{}' (version: {}, {} entries, project: '{}')",
            fatPath.string(), VersionToString(archive->GetVersion()),
            archive->GetEntries().size(),
            project ? project->GetName() : "None");

        ec.clear();
        return archive;
    }

    bool ArchiveFactory::ApplyProject(
        IArchive &archive, const std::shared_ptr<ProjectConfig> &project) {
        ZoneScopedN("ArchiveFactory::ApplyProject");
        if (!project)
            return false;

        archive.SetProjectConfig(project);

        ArchiveVersion ver = archive.GetVersion();
        bool is32 = (ver != ArchiveVersion::FATv5);

        project->EnsureFilelistIndexed(ver);

        auto &entries = archive.GetEntries();
        size_t count = entries.size();
        if (count == 0)
            return true;

        unsigned int numThreads =
            std::max(1u, std::thread::hardware_concurrency());
        size_t chunkSize = (count + numThreads - 1) / numThreads;
        ThreadPool pool(numThreads);
        std::vector<std::future<void>> futures;
        futures.reserve(numThreads);

        for (unsigned int t = 0; t < numThreads; ++t) {
            size_t start = t * chunkSize;
            size_t end = std::min(start + chunkSize, count);
            if (start >= end)
                break;

            futures.push_back(
                pool.Enqueue([&entries, &project, ver, is32, start, end]() {
                    for (size_t i = start; i < end; ++i) {
                        auto &entry = entries[i];
                        auto overridePath =
                            project->FindPathByHash(entry.name_hash, is32);
                        if (overridePath.has_value()) {
                            entry.path = *overridePath;
                            continue;
                        }
                        if (entry.path.starts_with("__UNKNOWN/")) {
                            auto pathOpt =
                                project->FindFilelistPath(entry.name_hash, ver);
                            if (pathOpt.has_value()) {
                                entry.path = NormalizePath(*pathOpt);
                            }
                        }
                    }
                }));
        }

        for (auto &f : futures) {
            f.get();
        }

        return true;
    }

    bool
    ArchiveFactory::ApplyProject(IArchive &archive,
                                 const std::string &projectName,
                                 const std::filesystem::path &workspaceRoot) {
        if (projectName.empty())
            return false;
        auto project = ProjectManager::GetInstance().GetProject(projectName,
                                                                workspaceRoot);
        if (!project)
            return false;
        return ApplyProject(archive, project);
    }

    bool ArchiveFactory::LoadNfoNames(const std::filesystem::path &nfoPath,
                                      IArchive &archive) {
        ZoneScopedN("ArchiveFactory::LoadNfoNames");
        pugi::xml_document doc;
        pugi::xml_parse_result result = doc.load_file(nfoPath.c_str());
        if (!result) {
            std::ifstream file(nfoPath, std::ios::binary);
            if (!file.is_open())
                return false;
            result = doc.load(file);
            if (!result)
                return false;
        }

        std::unordered_map<uint64_t, std::string> hashMap;

        auto processNode = [&](pugi::xml_node fileNode) {
            pugi::xml_attribute pathAttr = fileNode.attribute("Path");
            if (!pathAttr)
                pathAttr = fileNode.attribute("Hash");
            pugi::xml_attribute crcAttr = fileNode.attribute("Crc");
            if (!crcAttr)
                crcAttr = fileNode.attribute("NameHash");

            if (pathAttr && crcAttr) {
                std::string_view pathStr = pathAttr.value();
                std::string_view crcStr = crcAttr.value();
                if (!pathStr.empty() && !crcStr.empty()) {
                    try {
                        uint64_t crcVal = 0;
                        if (crcStr.starts_with("0x") ||
                            crcStr.starts_with("0X")) {
                            crcVal =
                                std::stoull(std::string(crcStr), nullptr, 16);
                        } else {
                            crcVal = std::stoull(std::string(crcStr));
                        }
                        hashMap[crcVal] = NormalizePath(pathStr);
                    } catch (...) {
                    }
                }
            }
        };

        pugi::xml_node root = doc.child("Root");
        if (!root) {
            root = doc.first_child();
        }

        if (root) {
            pugi::xml_node common = root.child("common");
            if (common) {
                for (pugi::xml_node fileNode : common.children("File")) {
                    processNode(fileNode);
                }
            }
            for (pugi::xml_node fileNode : root.children("File")) {
                processNode(fileNode);
            }
        }

        for (auto &entry : archive.GetEntries()) {
            auto it = hashMap.find(entry.name_hash);
            if (it != hashMap.end() && !it->second.empty()) {
                entry.path = it->second;
            }
        }

        return true;
    }

    bool ArchiveFactory::LoadProjectFilelists(
        const std::filesystem::path &workspaceRoot, IArchive &archive) {
        auto projects =
            ProjectManager::GetInstance().GetAvailableProjects(workspaceRoot);
        for (const auto &projName : projects) {
            auto project = ProjectManager::GetInstance().GetProject(
                projName, workspaceRoot);
            if (project) {
                ApplyProject(archive, project);
            }
        }
        return true;
    }

} // namespace Nomad::Archives
