#include "Nomad/Archives/ArchiveFactory.hpp"
#include "Nomad/Archives/Compression.hpp"
#include "Nomad/Archives/Defragmenter.hpp"
#include "Nomad/Archives/DirectoryTree.hpp"
#include "Nomad/Archives/Extractor.hpp"
#include "Nomad/Archives/MemoryManager.hpp"
#include "Nomad/Archives/ProjectManager.hpp"
#include "Nomad/Archives/ThreadPool.hpp"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <format>
#include <fstream>
#include <future>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "NomadArchives.hpp"
#include <tracy/Tracy.hpp>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <io.h>
#include <windows.h>
#define isatty _isatty
#define fileno _fileno

static bool IsLaunchedFromExplorer() {
    DWORD processList[2];
    DWORD count = GetConsoleProcessList(processList, 2);
    return (count <= 1);
}

static void PauseIfExplorer() {
    if (isatty(fileno(stdin)) && IsLaunchedFromExplorer()) {
        std::cout << "\nPress Enter to exit..." << std::flush;
        std::string dummy;
        std::getline(std::cin, dummy);
    }
}
#else
#include <unistd.h>
static void PauseIfExplorer() {}
#endif

static bool verbose = false;

using namespace Nomad::Archives;

static void PrintUsage(std::string_view execName) {
    std::cout
        << std::format("Usage: {} [command] <input_dir|archive_file> [output] "
                       "[options]\n\n",
                       execName)
        << "Commands:\n"
        << "  unpack <fat_file|dat|nfo> [output_dir]  Unpack files from "
           "bigfile "
           "archive (default: all threads - 1)\n"
        << "  pack <input_dir> [fat_output] [-jN]     Pack directory into "
           "bigfile archive (default: all threads - 1)\n"
        << "  info <fat_file|dat|nfo>                 Display metadata and "
           "entry "
           "count for an archive\n"
        << "  list <fat_file|dat|nfo> [pattern]       List directory contents "
           "of "
           "an archive (supports wildcards)\n"
        << "  unpack-filter <fat_file> <pat> [output] Extract files matching "
           "wildcard pattern (e.g. 'worlds/**', '*.xbt')\n"
        << "  defrag <fat_file|dat> [--analyze]       Optimize/defragment, or "
           "check if it's already optimized\n\n"
        << "Options:\n"
        << "  -p <name>, --project <name>             Specify project "
           "configuration (e.g. -p Watch_DogsLegion)\n"
        << "  -c <scheme>, --compression <scheme>     Specify compression "
           "scheme "
           "(e.g. lz4lw, lzo1x, none, xmemcompress)\n"
        << "  -j<N>, -j <N>, --threads <N>            Specify thread count "
           "(e.g. "
           "-j24 for 24 threads)\n"
        << "  --fat2, --fat3, --fat5                  Force FAT format version "
           "when packing\n"
        << "  --wd2, --legion                         Target Watch Dogs 2 "
           "(v11) "
           "or Legion (v13)\n"
        << "  --raw-version <N>                       Specify raw version (7, "
           "8 "
           "for FATv2/3; 11, 13 for FATv5)\n"
        << "  -V, --version                           Get the current version "
           "of "
           "BFTools.\n"
        << "  -v, --verbose                           Enable verbose mode.\n"
        << "  -h, --help                              Display this help "
           "message.\n";
}

static void PrintNode(const ArchiveNode& node, int indent = 0) {
    std::string padding(indent * 2, ' ');
    if (node.isDirectory) {
        std::cout << std::format("{}[DIR] {}\n", padding, node.name);
        for (const auto& child : node.children) {
            PrintNode(*child, indent + 1);
        }
    } else {
        std::cout << std::format(
            "{}[FILE] {} (Offset: {}, CompSize: {}, UncompSize: {}, Scheme: "
            "{})\n",
            padding, node.name, node.entryRef ? node.entryRef->offset : 0,
            node.entryRef ? node.entryRef->compressed_size : 0,
            node.entryRef ? node.entryRef->uncompressed_size : 0,
            node.entryRef ? CompressionToString(node.entryRef->compression)
                          : "None");
    }
}

static std::filesystem::path
ResolveFatPath(const std::filesystem::path& inputPath) {
    std::string ext = inputPath.extension().string();
    std::string extLower = ext;
    std::transform(
        extLower.begin(), extLower.end(), extLower.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (extLower == ".dat" || extLower == ".nfo") {
        std::filesystem::path candidate = inputPath;
        candidate.replace_extension(".fat");
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
        std::filesystem::path candidateUpper = inputPath;
        candidateUpper.replace_extension(".FAT");
        if (std::filesystem::exists(candidateUpper)) {
            return candidateUpper;
        }
        return candidate;
    }

    if (ext.empty() && !std::filesystem::is_directory(inputPath)) {
        std::filesystem::path candidate = inputPath;
        candidate.replace_extension(".fat");
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }

    return inputPath;
}

static std::string
SelectProjectInteractively(const std::string& currentProject) {
    if (!currentProject.empty())
        return currentProject;

    if (!isatty(fileno(stdin))) {
        return "";
    }

    std::vector<std::string> projectNames =
        ProjectManager::GetInstance().GetAvailableProjects();
    if (projectNames.empty())
        return "";

    std::sort(projectNames.begin(), projectNames.end());

    std::cout << "\nAvailable Projects:\n";
    std::cout << "  0. None (Default)\n";
    for (size_t i = 0; i < projectNames.size(); ++i) {
        std::cout << std::format("  {}. {}\n", i + 1, projectNames[i]);
    }

    std::cout << "Select a project to use [0-" << projectNames.size()
              << "] (default 0): ";
    std::string line;
    if (std::getline(std::cin, line)) {
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        if (line.empty())
            return "";

        try {
            int choice = std::stoi(line);
            if (choice > 0 && choice <= static_cast<int>(projectNames.size())) {
                return projectNames[choice - 1];
            }
        } catch (...) {
            for (const auto& name : projectNames) {
                if (name == line)
                    return name;
            }
        }
    }

    return "";
}

static std::shared_ptr<ProjectConfig>
ResolveAndReportProject(std::string& projectName) {
    projectName = SelectProjectInteractively(projectName);
    if (!projectName.empty()) {
        auto projConfig = ProjectManager::GetInstance().GetProject(projectName);
        if (projConfig) {
            std::cout << std::format("Using project: {}\n",
                                     projConfig->GetName());
            return projConfig;
        } else {
            std::cerr << std::format("Warning: Project '{}' not found.\n",
                                     projectName);
        }
    }
    return nullptr;
}

static int DoPack(const std::filesystem::path& inputDir,
                  std::filesystem::path fatOutPath, std::string projectName,
                  ArchiveVersion forcedFatVersion, uint32_t forcedRawVersion,
                  CompressionScheme forcedCompression,
                  unsigned int numThreads) {
    if (!std::filesystem::is_directory(inputDir)) {
        std::cerr << std::format("Error: input directory '{}' does not exist "
                                 "or is not a directory.\n",
                                 inputDir.string());
        return 1;
    }

    if (fatOutPath.empty()) {
        std::filesystem::path cleanDir = inputDir;
        if (cleanDir.filename().empty()) {
            cleanDir = cleanDir.parent_path();
        }
        std::string folderName = cleanDir.filename().string();
        if (folderName.ends_with("_unpack")) {
            folderName = folderName.substr(0, folderName.length() - 7);
        }
        if (folderName.empty()) {
            folderName = "archive";
        }
        fatOutPath = cleanDir.parent_path() / (folderName + ".fat");
    }

    std::string ext = fatOutPath.extension().string();
    std::string extLower = ext;
    std::transform(
        extLower.begin(), extLower.end(), extLower.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (extLower != ".fat") {
        fatOutPath.replace_extension(".fat");
    }

    auto projConfig = ResolveAndReportProject(projectName);
    ArchiveVersion packVersion = ArchiveVersion::FATv5;
    if (projConfig &&
        projConfig->GetDefaultFatVersion() != ArchiveVersion::Unknown) {
        packVersion = projConfig->GetDefaultFatVersion();
    }

    if (forcedFatVersion != ArchiveVersion::Unknown) {
        packVersion = forcedFatVersion;
    }

    uint32_t rawVersionToUse = forcedRawVersion;
    if (rawVersionToUse == 0 && projConfig &&
        projConfig->GetDefaultRawVersion() != 0) {
        rawVersionToUse = projConfig->GetDefaultRawVersion();
    }

    if (rawVersionToUse != 0) {
        if ((packVersion == ArchiveVersion::FATv2 ||
             packVersion == ArchiveVersion::FATv3) &&
            (rawVersionToUse != 7 && rawVersionToUse != 8)) {
            std::cerr << std::format(
                "Error: Raw version {} is not supported for {}. "
                "Only versions 7 and 8 are supported.\n",
                rawVersionToUse, VersionToString(packVersion));
            return 1;
        } else if (packVersion == ArchiveVersion::FATv5 &&
                   (rawVersionToUse != 11 && rawVersionToUse != 12 &&
                    rawVersionToUse != 13)) {
            std::cerr << std::format(
                "Error: Raw version {} is not supported for FATv5. Only "
                "version 11 "
                "(Watch Dogs 2) and version 12/13 (Watch Dogs: Legion) are "
                "supported.\n",
                rawVersionToUse);
            return 1;
        }
    }

    auto archive = ArchiveFactory::Create(packVersion);
    if (!archive) {
        std::cerr << "Error creating archive object.\n";
        return 1;
    }

    if (rawVersionToUse != 0) {
        archive->SetRawVersion(rawVersionToUse);
    }

    if (projConfig) {
        archive->SetProjectConfig(projConfig);
    }

    std::cout << std::format(
        "Packing directory '{}' -> '{}' ({}, raw v{}) using {} threads...\n",
        inputDir.string(), fatOutPath.string(), VersionToString(packVersion),
        archive->GetRawVersion(), numThreads);

    std::filesystem::path datOutPath = fatOutPath;
    datOutPath.replace_extension(".dat");

    std::filesystem::path nfoOutPath = fatOutPath;
    nfoOutPath.replace_extension(".nfo");

    std::ofstream datStream(datOutPath, std::ios::binary);
    if (!datStream.is_open()) {
        std::cerr << std::format("Error creating DAT file '{}'\n",
                                 datOutPath.string());
        return 1;
    }

    CompressionScheme packCompression = forcedCompression;
    if (packCompression == CompressionScheme::Unknown) {
        packCompression = GetDefaultCompressionForVersion(packVersion);
    }

    std::cout << std::format("Packing with default compression: {}\n",
                             CompressionToString(packCompression));

    struct ProcessedFile {
        std::string relPathStr;
        uintmax_t fileSize{0};
        CompressionScheme actualScheme{CompressionScheme::None};
        std::vector<uint8_t> payloadData;
    };

    ThreadPool pool(numThreads);
    std::vector<std::filesystem::path> allFiles;
    for (const auto& dirEntry :
         std::filesystem::recursive_directory_iterator(inputDir)) {
        if (dirEntry.is_regular_file()) {
            std::filesystem::path p = dirEntry.path();
            if (p == datOutPath || p == fatOutPath || p == nfoOutPath) {
                continue;
            }
            allFiles.push_back(p);
        }
    }

    size_t fileCount = 0;
    const size_t batchSize = std::max<size_t>(64, numThreads * 8);

    for (size_t batchStart = 0; batchStart < allFiles.size();
         batchStart += batchSize) {
        size_t batchEnd = std::min(batchStart + batchSize, allFiles.size());
        std::vector<std::future<ProcessedFile>> futures;
        futures.reserve(batchEnd - batchStart);

        for (size_t i = batchStart; i < batchEnd; ++i) {
            const auto& filePath = allFiles[i];
            futures.push_back(pool.Enqueue(
                [filePath, inputDir, packCompression]() -> ProcessedFile {
                    std::filesystem::path relPath =
                        std::filesystem::relative(filePath, inputDir);
                    std::string relPathStr = NormalizePath(relPath.string());

                    std::error_code ec;
                    uintmax_t fileSize =
                        std::filesystem::file_size(filePath, ec);
                    if (ec) {
                        return {relPathStr, 0, CompressionScheme::None, {}};
                    }

                    std::ifstream srcFile(filePath, std::ios::binary);
                    if (!srcFile.is_open()) {
                        return {
                            relPathStr, fileSize, CompressionScheme::None, {}};
                    }

                    std::vector<uint8_t> uncompressedData(fileSize);
                    if (fileSize > 0) {
                        srcFile.read(
                            reinterpret_cast<char*>(uncompressedData.data()),
                            fileSize);
                    }

                    CompressionScheme actualScheme = CompressionScheme::None;
                    std::vector<uint8_t> payloadData;

                    if (fileSize <= 31 ||
                        packCompression == CompressionScheme::None) {
                        actualScheme = CompressionScheme::None;
                        payloadData = std::move(uncompressedData);
                    } else {
                        std::string compErr;
                        if (Compression::Compress(packCompression,
                                                  uncompressedData.data(),
                                                  uncompressedData.size(),
                                                  payloadData, compErr) &&
                            payloadData.size() < uncompressedData.size()) {
                            actualScheme = packCompression;
                        } else {
                            actualScheme = CompressionScheme::None;
                            payloadData = std::move(uncompressedData);
                        }
                    }

                    return {std::move(relPathStr), fileSize, actualScheme,
                            std::move(payloadData)};
                }));
        }

        for (auto& fut : futures) {
            ProcessedFile item = fut.get();
            if (item.relPathStr.empty())
                continue;

            uint64_t currPos = static_cast<uint64_t>(datStream.tellp());
            uint64_t alignedOffset = (currPos + 15) & ~15ULL;
            if (alignedOffset > currPos) {
                char pad[16] = {0};
                datStream.write(pad, alignedOffset - currPos);
            }

            if (!item.payloadData.empty()) {
                datStream.write(
                    reinterpret_cast<const char*>(item.payloadData.data()),
                    item.payloadData.size());
            }

            archive->AddOrUpdateEntry(item.relPathStr, item.fileSize,
                                      item.payloadData.size(),
                                      item.actualScheme, alignedOffset);
            fileCount++;

            if (verbose)
                std::cout << std::format(
                    "[{}] Packed {} ({} -> {} bytes, {})\n", fileCount,
                    item.relPathStr, item.fileSize, item.payloadData.size(),
                    CompressionToString(item.actualScheme));
        }
    }

    datStream.close();

    std::ofstream fatStream(fatOutPath, std::ios::binary);
    if (!fatStream.is_open() || !archive->Write(fatStream)) {
        std::cerr << std::format("Error writing FAT file '{}'\n",
                                 fatOutPath.string());
        return 1;
    }
    fatStream.close();

    std::ofstream nfoStream(nfoOutPath);
    if (nfoStream.is_open()) {
        archive->SerializeNfo(nfoStream);
        nfoStream.close();
    }

    std::cout << std::format(
        "Successfully packed {} files into archive '{}' (.fat, .dat, .nfo).\n",
        fileCount, fatOutPath.string());
    return 0;
}

static int DoUnpack(const std::filesystem::path& inputPath,
                    std::filesystem::path outputDir, std::string projectName,
                    unsigned int numThreads) {
    std::filesystem::path fatPath = ResolveFatPath(inputPath);
    if (!std::filesystem::exists(fatPath)) {
        std::string origExt = inputPath.extension().string();
        std::string extLower = origExt;
        std::transform(
            extLower.begin(), extLower.end(), extLower.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (extLower == ".dat" || extLower == ".nfo") {
            std::cerr << std::format(
                "Error: Cannot find matching FAT archive '{}' for '{}'. Both "
                "FAT and "
                "DAT files are required to unpack an archive.\n",
                fatPath.string(), inputPath.string());
        } else {
            std::cerr << std::format(
                "Error: FAT archive file does not exist: '{}'\n",
                fatPath.string());
        }
        return 1;
    }

    if (outputDir.empty()) {
        outputDir =
            fatPath.parent_path() / (fatPath.stem().string() + "_unpack");
    }

    ResolveAndReportProject(projectName);

    std::error_code ec;
    auto archive = ArchiveFactory::Open(fatPath, ec, projectName);
    if (ec) {
        std::cerr << std::format("Error opening FAT archive '{}': {}\n",
                                 fatPath.string(), ec.message());
        return 1;
    }

    std::cout << std::format("Unpacking archive '{}' -> '{}' using {} threads "
                             "(default: all threads - 1)...\n",
                             fatPath.string(), outputDir.string(), numThreads);

    Extractor::ExtractAll(
        *archive, fatPath, outputDir,
        [](size_t current, size_t total, const std::string& name) {
            if (verbose)
                std::cout << std::format("[{}/{}] Extracted {}\n", current,
                                         total, name);
        },
        numThreads);

    std::filesystem::path nfoPath = fatPath;
    nfoPath.replace_extension(".nfo");
    if (!std::filesystem::exists(nfoPath)) {
        std::ofstream nfoStream(nfoPath);
        if (nfoStream.is_open()) {
            archive->SerializeNfo(nfoStream);
            nfoStream.close();
        }
    }

    std::cout << "Unpack completed successfully.\n";
    return 0;
}

int main(int argc, char* argv[]) {
    ZoneScopedN("BFTools::Main");
    Nexus::MemoryManager::SetReady(true);
    if (argc < 2) {
        PrintUsage(argv[0]);
        PauseIfExplorer();
        return 1;
    }

    std::vector<std::string> positionalArgs;
    std::vector<std::string> options;
    std::string projectName;
    unsigned int numThreads = ThreadPool::DefaultThreadCount();
    ArchiveVersion forcedFatVersion = ArchiveVersion::Unknown;
    uint32_t forcedRawVersion = 0;
    CompressionScheme forcedCompression = CompressionScheme::Unknown;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else if (arg == "-V" || arg == "--version") {
            std::cout << "BFTools\nVersion: "
                      << Nomad::Archives::ProjectInterface::GetVersion()
                      << "\nBuild Date: " << ProjectInterface::GetBuildDate()
                      << "\nCommit: " << ProjectInterface::GetCommit()
                      << std::endl;
            PauseIfExplorer();
            return 0;
        } else if (arg == "-h" || arg == "--help" || arg == "help") {
            PrintUsage(argv[0]);
            PauseIfExplorer();
            return 0;
        } else if (arg == "-c" || arg == "--compression") {
            if (i + 1 < argc) {
                std::string compStr = argv[++i];
                forcedCompression = ParseCompressionScheme(compStr);
                if (forcedCompression == CompressionScheme::Unknown) {
                    std::cerr << std::format(
                        "Error: Unknown compression scheme '{}'\n", compStr);
                    PauseIfExplorer();
                    return 1;
                }
            } else {
                std::cerr
                    << "Error: -c/--compression requires a compression scheme "
                       "argument.\n";
                PauseIfExplorer();
                return 1;
            }
        } else if (arg == "-p" || arg == "--project") {
            if (i + 1 < argc) {
                projectName = argv[++i];
            } else {
                std::cerr << "Error: -p/--project requires a project name "
                             "argument.\n";
                PauseIfExplorer();
                return 1;
            }
        } else if (arg.starts_with("-j")) {
            std::string val = arg.substr(2);
            if (val.empty() && i + 1 < argc) {
                val = argv[++i];
            }
            if (!val.empty()) {
                try {
                    int parsed = std::stoi(val);
                    if (parsed > 0)
                        numThreads = static_cast<unsigned int>(parsed);
                } catch (...) {
                    std::cerr << std::format("Warning: Invalid thread count "
                                             "'{}', using default ({})\n",
                                             val, numThreads);
                }
            }
        } else if (arg == "--threads") {
            if (i + 1 < argc) {
                std::string val = argv[++i];
                try {
                    int parsed = std::stoi(val);
                    if (parsed > 0)
                        numThreads = static_cast<unsigned int>(parsed);
                } catch (...) {
                    std::cerr << std::format("Warning: Invalid thread count "
                                             "'{}', using default ({})\n",
                                             val, numThreads);
                }
            }
        } else if (arg == "--fat2") {
            forcedFatVersion = ArchiveVersion::FATv2;
        } else if (arg == "--fat3") {
            forcedFatVersion = ArchiveVersion::FATv3;
        } else if (arg == "--fat5") {
            forcedFatVersion = ArchiveVersion::FATv5;
        } else if (arg == "--wd2" || arg == "--fat5-wd2") {
            forcedFatVersion = ArchiveVersion::FATv5;
            forcedRawVersion = 11;
        } else if (arg == "--legion" || arg == "--fat5-legion") {
            forcedFatVersion = ArchiveVersion::FATv5;
            forcedRawVersion = 13;
        } else if (arg == "--raw-version") {
            if (i + 1 < argc) {
                std::string val = argv[++i];
                try {
                    forcedRawVersion = static_cast<uint32_t>(std::stoul(val));
                } catch (...) {
                    std::cerr << std::format(
                        "Error: Invalid raw version '{}'.\n", val);
                    PauseIfExplorer();
                    return 1;
                }
            } else {
                std::cerr
                    << "Error: --raw-version requires a numeric argument.\n";
                PauseIfExplorer();
                return 1;
            }
        } else if (arg.starts_with("-")) {
            options.push_back(arg);
        } else {
            positionalArgs.push_back(arg);
        }
    }

    if (positionalArgs.empty()) {
        PrintUsage(argv[0]);
        PauseIfExplorer();
        return 1;
    }

    std::string command = positionalArgs[0];

    // Explicit command: pack
    if (command == "pack") {
        if (positionalArgs.size() < 2) {
            std::cerr << "Error: pack command requires an input directory "
                         "argument.\n";
            PrintUsage(argv[0]);
            PauseIfExplorer();
            return 1;
        }

        std::filesystem::path inputDir = positionalArgs[1];
        std::filesystem::path fatOutPath =
            (positionalArgs.size() >= 3) ? positionalArgs[2] : "";

        int res = DoPack(inputDir, fatOutPath, projectName, forcedFatVersion,
                         forcedRawVersion, forcedCompression, numThreads);
        PauseIfExplorer();
        return res;
    }

    // Explicit command: unpack
    if (command == "unpack") {
        if (positionalArgs.size() < 2) {
            std::cerr
                << "Error: unpack command requires an archive file argument.\n";
            PrintUsage(argv[0]);
            PauseIfExplorer();
            return 1;
        }

        std::filesystem::path inputPath = positionalArgs[1];
        std::filesystem::path outputDir =
            (positionalArgs.size() >= 3) ? positionalArgs[2] : "";

        int res = DoUnpack(inputPath, outputDir, projectName, numThreads);
        PauseIfExplorer();
        return res;
    }

    // Explicit command: unpack-filter
    if (command == "unpack-filter") {
        if (positionalArgs.size() < 3) {
            std::cerr
                << "Error: unpack-filter command requires fat_file and pattern "
                   "arguments.\n";
            PrintUsage(argv[0]);
            PauseIfExplorer();
            return 1;
        }

        std::filesystem::path fatPath = ResolveFatPath(positionalArgs[1]);
        std::string targetPattern = positionalArgs[2];

        ResolveAndReportProject(projectName);

        std::error_code ec;
        auto archive = ArchiveFactory::Open(fatPath, ec, projectName);
        if (ec) {
            std::cerr << std::format("Error opening FAT archive '{}': {}\n",
                                     fatPath.string(), ec.message());
            PauseIfExplorer();
            return 1;
        }

        std::filesystem::path datPath = fatPath;
        datPath.replace_extension(".dat");

        bool endsWithSlash =
            targetPattern.ends_with('/') || targetPattern.ends_with('\\');
        if (endsWithSlash) {
            std::string norm = NormalizePath(targetPattern);
            while (!norm.empty() && norm.back() == '/') {
                norm.pop_back();
            }
            targetPattern = norm.empty() ? "**" : (norm + "/**");
        }

        bool hasWildcards = HasWildcards(targetPattern);
        std::string normPattern = NormalizePath(targetPattern);
        bool patternHasSlash = (normPattern.find('/') != std::string::npos);

        std::vector<const ArchiveEntry*> matchingEntries;

        if (!hasWildcards) {
            // 1. Exact path match
            for (const auto& entry : archive->GetEntries()) {
                if (entry.path == targetPattern || entry.path == normPattern ||
                    NormalizePath(entry.path) == normPattern) {
                    matchingEntries.push_back(&entry);
                    break;
                }
            }

            // 2. Hash match
            if (matchingEntries.empty()) {
                uint64_t targetHash = archive->ComputeNameHash(targetPattern);
                uint64_t numericHash = 0;
                bool isNumeric = false;
                try {
                    if (targetPattern.starts_with("0x") ||
                        targetPattern.starts_with("0X")) {
                        numericHash = std::stoull(targetPattern, nullptr, 16);
                        isNumeric = true;
                    } else if (!targetPattern.empty() &&
                               std::all_of(targetPattern.begin(),
                                           targetPattern.end(), ::isdigit)) {
                        numericHash = std::stoull(targetPattern, nullptr, 10);
                        isNumeric = true;
                    }
                } catch (...) {
                }

                for (const auto& entry : archive->GetEntries()) {
                    if (entry.name_hash == targetHash ||
                        (isNumeric && entry.name_hash == numericHash)) {
                        matchingEntries.push_back(&entry);
                        break;
                    }
                }
            }

            // 3. Exact filename match (if pattern has no slash)
            if (matchingEntries.empty() && !patternHasSlash) {
                for (const auto& entry : archive->GetEntries()) {
                    if (std::filesystem::path(entry.path).filename() ==
                        targetPattern) {
                        matchingEntries.push_back(&entry);
                    }
                }
            }

            // 4. Directory prefix match (e.g. user typed "worlds" without
            // trailing slash)
            if (matchingEntries.empty()) {
                std::string dirPrefix = normPattern + "/";
                for (const auto& entry : archive->GetEntries()) {
                    std::string normEntry = NormalizePath(entry.path);
                    if (normEntry.starts_with(dirPrefix)) {
                        matchingEntries.push_back(&entry);
                    }
                }
            }
        } else {
            // Wildcard glob match
            for (const auto& entry : archive->GetEntries()) {
                std::string normEntry = NormalizePath(entry.path);
                bool match = MatchGlob(normPattern, normEntry);
                if (!match && !patternHasSlash) {
                    std::string filename =
                        std::filesystem::path(normEntry).filename().string();
                    match = MatchGlob(normPattern, filename);
                }
                if (match) {
                    matchingEntries.push_back(&entry);
                }
            }
        }

        if (matchingEntries.empty()) {
            std::cerr << std::format(
                "Error: No entries matching '{}' found in archive.\n",
                targetPattern);
            PauseIfExplorer();
            return 1;
        }

        // Single entry exact extraction
        if (matchingEntries.size() == 1 && !hasWildcards) {
            const auto* found = matchingEntries[0];
            std::filesystem::path targetOutPath;
            if (positionalArgs.size() >= 4) {
                std::filesystem::path specifiedOut = positionalArgs[3];
                if (std::filesystem::is_directory(specifiedOut) ||
                    specifiedOut.string().ends_with('/') ||
                    specifiedOut.string().ends_with('\\')) {
                    targetOutPath =
                        specifiedOut /
                        std::filesystem::path(found->path).filename();
                } else {
                    targetOutPath = specifiedOut;
                }
            } else {
                targetOutPath = std::filesystem::path(targetPattern).filename();
            }

            std::ifstream datStream(datPath, std::ios::binary);
            if (!datStream.is_open()) {
                std::cerr << std::format("Error opening DAT file '{}'\n",
                                         datPath.string());
                PauseIfExplorer();
                return 1;
            }

            if (verbose)
                std::cout << std::format(
                    "Extracting entry '{}' (offset: {}, comp: {}, uncomp: {}, "
                    "scheme: "
                    "{}) -> '{}'...\n",
                    found->path, found->offset, found->compressed_size,
                    found->uncompressed_size,
                    CompressionToString(found->compression),
                    targetOutPath.string());

            if (!Extractor::ExtractEntry(*found, datStream, targetOutPath)) {
                std::cerr << "Failed to extract entry.\n";
                PauseIfExplorer();
                return 1;
            }

            std::cout << std::format("Successfully extracted '{}' to '{}'.\n",
                                     found->path, targetOutPath.string());
            PauseIfExplorer();
            return 0;
        }

        // Multiple entries or wildcard pattern extraction
        std::filesystem::path outputDir =
            (positionalArgs.size() >= 4) ? positionalArgs[3] : ".";

        std::cout << std::format(
            "Extracting {} matching entries for pattern '{}' "
            "> '{}' using {} threads...\n",
            matchingEntries.size(), targetPattern, outputDir.string(),
            numThreads);

        bool success = Extractor::ExtractEntries(
            matchingEntries, datPath, outputDir,
            [](size_t current, size_t total, const std::string& name) {
                if (verbose)
                    std::cout << std::format("[{}/{}] Extracted {}\n", current,
                                             total, name);
            },
            numThreads);

        if (!success) {
            std::cerr << "Some entries failed to extract.\n";
            PauseIfExplorer();
            return 1;
        }

        std::cout << std::format("Successfully extracted {} entries to '{}'.\n",
                                 matchingEntries.size(), outputDir.string());
        PauseIfExplorer();
        return 0;
    }

    // Explicit command: info or list
    if (command == "info" || command == "list") {
        if (positionalArgs.size() < 2) {
            std::cerr << std::format(
                "Error: command '{}' requires an archive path.\n", command);
            PrintUsage(argv[0]);
            PauseIfExplorer();
            return 1;
        }

        std::filesystem::path inputPath = ResolveFatPath(positionalArgs[1]);

        ResolveAndReportProject(projectName);

        std::error_code ec;
        auto archive = ArchiveFactory::Open(inputPath, ec, projectName);
        if (ec) {
            std::cerr << std::format("Error opening FAT archive '{}': {}\n",
                                     inputPath.string(), ec.message());
            PauseIfExplorer();
            return 1;
        }

        if (command == "info") {
            std::cout << std::format("Archive Information for '{}':\n",
                                     inputPath.string())
                      << std::format("  Format Version : {}\n",
                                     VersionToString(archive->GetVersion()))
                      << std::format("  Raw Version    : {}\n",
                                     archive->GetRawVersion())
                      << std::format("  Target Platform: {}\n",
                                     PlatformToString(archive->GetPlatform()))
                      << std::format("  Comp Version   : {}\n",
                                     archive->GetCompressionVersion())
                      << std::format("  NameHash Ver   : {}\n",
                                     archive->GetNameHashVersion())
                      << std::format("  Entries        : {}\n",
                                     archive->GetEntries().size());
        } else {
            std::string filterPattern =
                (positionalArgs.size() >= 3) ? positionalArgs[2] : "";
            if (!filterPattern.empty()) {
                std::cout << std::format(
                    "Directory Listing for '{}' matching '{}':\n",
                    inputPath.string(), filterPattern);
                std::string normPattern = NormalizePath(filterPattern);
                bool patternHasSlash =
                    (normPattern.find('/') != std::string::npos);
                std::vector<ArchiveEntry> filtered;
                for (const auto& entry : archive->GetEntries()) {
                    std::string normEntry = NormalizePath(entry.path);
                    bool match = MatchGlob(normPattern, normEntry);
                    if (!match && !patternHasSlash) {
                        std::string filename = std::filesystem::path(normEntry)
                                                   .filename()
                                                   .string();
                        match = MatchGlob(normPattern, filename);
                    }
                    if (match) {
                        filtered.push_back(entry);
                    }
                }
                auto root = DirectoryTreeBuilder::Build(filtered);
                for (const auto& child : root->children) {
                    PrintNode(*child, 0);
                }
                std::cout << std::format("\nTotal matched entries: {}\n",
                                         filtered.size());
            } else {
                std::cout << std::format("Directory Listing for '{}':\n",
                                         inputPath.string());
                auto root = DirectoryTreeBuilder::Build(archive->GetEntries());
                for (const auto& child : root->children) {
                    PrintNode(*child, 0);
                }
            }
        }
        PauseIfExplorer();
        return 0;
    }

    // Explicit command: defrag
    if (command == "defrag") {
        if (positionalArgs.size() < 2) {
            std::cerr << "Error: defrag command requires an archive path.\n";
            PrintUsage(argv[0]);
            PauseIfExplorer();
            return 1;
        }

        std::filesystem::path fatPath = ResolveFatPath(positionalArgs[1]);

        ResolveAndReportProject(projectName);

        std::error_code ec;
        auto archive = ArchiveFactory::Open(fatPath, ec, projectName);
        if (ec) {
            std::cerr << std::format("Error opening FAT archive '{}': {}\n",
                                     fatPath.string(), ec.message());
            PauseIfExplorer();
            return 1;
        }

        bool analyzeOnly = false;
        for (const auto& opt : options) {
            if (opt == "--analyze") {
                analyzeOnly = true;
            }
        }

        CompressionScheme targetScheme = forcedCompression;
        if (targetScheme == CompressionScheme::Unknown) {
            targetScheme =
                GetDefaultCompressionForVersion(archive->GetVersion());
        }

        auto stats = Defragmenter::Analyze(fatPath, *archive, targetScheme);

        auto formatBytes = [](uint64_t bytes) -> std::string {
            if (bytes >= 1024ULL * 1024ULL * 1024ULL) {
                return std::format("{:.2f} GB ({} bytes)",
                                   static_cast<double>(bytes) /
                                       (1024.0 * 1024.0 * 1024.0),
                                   bytes);
            } else if (bytes >= 1024ULL * 1024ULL) {
                return std::format(
                    "{:.2f} MB ({} bytes)",
                    static_cast<double>(bytes) / (1024.0 * 1024.0), bytes);
            } else if (bytes >= 1024ULL) {
                return std::format("{:.2f} KB ({} bytes)",
                                   static_cast<double>(bytes) / 1024.0, bytes);
            }
            return std::format("{} bytes", bytes);
        };

        std::cout << std::format("Fragmentation Analysis for '{}':\n",
                                 fatPath.string())
                  << std::format("  Current DAT Size       : {}\n",
                                 formatBytes(stats.currentDatSize))
                  << std::format("  Active Payload Size    : {}\n",
                                 formatBytes(stats.activePayloadSize))
                  << std::format("  Estimated Compact Size : {}\n",
                                 formatBytes(stats.estimatedNewDatSize))
                  << std::format("  Reclaimable Space      : {}\n",
                                 formatBytes(stats.reclaimableBytes))
                  << std::format("  Fragmentation          : {:.2f}%\n",
                                 stats.fragmentationPercent)
                  << std::format("  Total Entries          : {}\n",
                                 stats.entryCount);

        if (analyzeOnly) {
            PauseIfExplorer();
            return 0;
        }

        if (stats.reclaimableBytes <= 0 &&
            forcedCompression == CompressionScheme::Unknown) {
            std::filesystem::path nfoPath = fatPath;
            nfoPath.replace_extension(".nfo");
            std::ofstream nfoStream(nfoPath, std::ios::trunc);
            if (nfoStream.is_open()) {
                archive->SerializeNfo(nfoStream);
                nfoStream.close();
            }
            std::cout
                << "Archive is already fully compact and no compression scheme "
                   "change requested. NFO file refreshed.\n";
            PauseIfExplorer();
            return 0;
        }

        std::cout << std::format(
            "Defragmenting archive '{}' (Target Compression: {})...\n",
            fatPath.string(), CompressionToString(targetScheme));
        std::string errorMsg;
        bool success = Defragmenter::Defragment(
            fatPath, *archive, errorMsg,
            [](size_t current, size_t total) {
                if (total > 0 && (current % 100 == 0 || current == total)) {
                    std::cout
                        << std::format(
                               "\rProgress: [{}/{}] ({:.1f}%)", current, total,
                               (static_cast<double>(current) / total) * 100.0)
                        << std::flush;
                }
            },
            targetScheme, numThreads);
        std::cout << "\n";

        if (!success) {
            std::cerr << std::format("Defragmentation failed: {}\n", errorMsg);
            PauseIfExplorer();
            return 1;
        }

        std::cout << std::format(
            "Defragmentation completed successfully! Reclaimed {}.\n",
            formatBytes(stats.reclaimableBytes));
        PauseIfExplorer();
        return 0;
    }

    // Default actions (drag-and-drop or path passed directly)
    std::filesystem::path target(command);

    if (std::filesystem::is_directory(target)) {
        // Dragging a folder generates an archive (.dat, .fat, .nfo)
        std::filesystem::path inputDir = target;
        std::filesystem::path fatOutPath =
            (positionalArgs.size() >= 2) ? positionalArgs[1] : "";
        int res = DoPack(inputDir, fatOutPath, projectName, forcedFatVersion,
                         forcedRawVersion, forcedCompression, numThreads);
        PauseIfExplorer();
        return res;
    }

    // Check if target is a file or dat/fat/nfo archive
    std::string ext = target.extension().string();
    std::string extLower = ext;
    std::transform(
        extLower.begin(), extLower.end(), extLower.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    std::filesystem::path fatCandidate = ResolveFatPath(target);

    if (extLower == ".dat" || extLower == ".fat" || extLower == ".nfo" ||
        std::filesystem::is_regular_file(target) ||
        std::filesystem::exists(fatCandidate)) {
        // Dragging a dat/fat/nfo unpacks it by default
        std::filesystem::path outputDir;
        if (positionalArgs.size() >= 2) {
            std::filesystem::path p2(positionalArgs[1]);
            std::string p2Ext = p2.extension().string();
            std::transform(p2Ext.begin(), p2Ext.end(), p2Ext.begin(),
                           [](unsigned char c) {
                               return static_cast<char>(std::tolower(c));
                           });
            // If user selected both .dat and .fat when dragging,
            // positionalArgs[1] is the sibling file, not outputDir
            if (p2Ext != ".dat" && p2Ext != ".fat" && p2Ext != ".nfo") {
                outputDir = p2;
            }
        }
        int res = DoUnpack(target, outputDir, projectName, numThreads);
        PauseIfExplorer();
        return res;
    }

    std::cerr << std::format(
        "Error: Unknown command or target '{}' does not exist.\n", command);
    PrintUsage(argv[0]);
    PauseIfExplorer();
    return 1;
}
