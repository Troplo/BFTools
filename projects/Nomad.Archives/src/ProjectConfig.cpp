//
// Created by troplo on 10/14/26.
//

#include "Nomad/Archives/ProjectConfig.hpp"
#include "Nomad/Archives/ArchiveFactory.hpp"
#include "Nomad/Archives/ThreadPool.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <future>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <sstream>
#include <thread>
#include <tracy/Tracy.hpp>
#include <zlib.h>

namespace Nomad::Archives {
    static std::string ToLowerString(std::string_view str) {
        std::string result(str);
        std::transform(
            result.begin(), result.end(), result.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return result;
    }

    static ArchiveVersion ParseVersionString(std::string_view str) {
        std::string s = ToLowerString(str);
        if (s.find("fatv2") != std::string::npos || s == "2")
            return ArchiveVersion::FATv2;
        if (s.find("fatv3") != std::string::npos || s == "3")
            return ArchiveVersion::FATv3;
        if (s.find("fatv5") != std::string::npos || s == "5")
            return ArchiveVersion::FATv5;
        return ArchiveVersion::Unknown;
    }

    ProjectConfig::ProjectConfig(std::string name) : m_name(std::move(name)) {}

    std::optional<uint64_t>
    ProjectConfig::ParseHashString(std::string_view str) {
        if (str.empty())
            return std::nullopt;

        std::string s(str);
        try {
            if (s.starts_with("0x") || s.starts_with("0X")) {
                return std::stoull(s, nullptr, 16);
            }
            return std::stoull(s);
        } catch (...) {
            return std::nullopt;
        }
    }

    void ProjectConfig::AddOverride(uint64_t hash, std::string_view path) {
        std::string normPath = NormalizePath(path);
        m_hashToPath[hash] = normPath;
        m_hash32ToPath[static_cast<uint32_t>(hash)] = normPath;

        std::string lowerPath = ToLowerString(normPath);
        m_pathToHash[lowerPath] = hash;
    }

    void ProjectConfig::RemoveOverride(uint64_t hash) {
        auto it = m_hashToPath.find(hash);
        if (it != m_hashToPath.end()) {
            std::string lowerPath = ToLowerString(it->second);
            m_pathToHash.erase(lowerPath);
            m_hashToPath.erase(it);
        }
        m_hash32ToPath.erase(static_cast<uint32_t>(hash));
    }

    std::optional<std::string>
    ProjectConfig::FindPathByHash(uint64_t hash, bool is32Bit) const {
        auto it = m_hashToPath.find(hash);
        if (it != m_hashToPath.end()) {
            return it->second;
        }

        if (is32Bit) {
            auto it32 = m_hash32ToPath.find(static_cast<uint32_t>(hash));
            if (it32 != m_hash32ToPath.end()) {
                return it32->second;
            }
        }

        return std::nullopt;
    }

    std::optional<uint64_t>
    ProjectConfig::FindHashByPath(std::string_view path) const {
        std::string normPath = NormalizePath(path);
        std::string lowerPath = ToLowerString(normPath);

        auto it = m_pathToHash.find(lowerPath);
        if (it != m_pathToHash.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    void ProjectConfig::ClearFilelists() {
        InvalidateIndex();
        m_filelistBuffers.clear();
        m_filelistLines.clear();
    }

    void ProjectConfig::SetFilelistLines(std::vector<std::string> lines) {
        ClearFilelists();
        size_t totalLen = 0;
        for (const auto &l : lines)
            totalLen += l.size() + 1;
        std::string &buf = m_filelistBuffers.emplace_back();
        buf.reserve(totalLen);
        m_filelistLines.reserve(lines.size());
        for (const auto &l : lines) {
            size_t offset = buf.size();
            buf.append(l);
            buf.push_back('\0');
            m_filelistLines.emplace_back(buf.data() + offset, l.size());
        }
        InvalidateIndex();
    }

    void ProjectConfig::SetFilelistLines(std::vector<std::string_view> lines) {
        ClearFilelists();
        size_t totalLen = 0;
        for (const auto &l : lines)
            totalLen += l.size() + 1;
        std::string &buf = m_filelistBuffers.emplace_back();
        buf.reserve(totalLen);
        m_filelistLines.reserve(lines.size());
        for (const auto &l : lines) {
            size_t offset = buf.size();
            buf.append(l);
            buf.push_back('\0');
            m_filelistLines.emplace_back(buf.data() + offset, l.size());
        }
        InvalidateIndex();
    }

    void ProjectConfig::InvalidateIndex() {
        std::unique_lock<std::mutex> lock(m_indexMutex);
        m_indexedVersion = ArchiveVersion::Unknown;
        m_indexedFilelist.clear();
    }

    void ProjectConfig::EnsureFilelistIndexed(ArchiveVersion version) {
        ZoneScopedN("ProjectConfig::EnsureFilelistIndexed");
        std::unique_lock<std::mutex> lock(m_indexMutex);
        if (m_indexedVersion == version && !m_indexedFilelist.empty()) {
            return;
        }
        if (m_filelistLines.empty()) {
            return;
        }

        size_t count = m_filelistLines.size();
        m_indexedFilelist.resize(count);

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

            futures.push_back(pool.Enqueue([this, version, start, end]() {
                for (size_t i = start; i < end; ++i) {
                    uint64_t h = ArchiveFactory::ComputeRawNameHash(
                        version, m_filelistLines[i]);
                    m_indexedFilelist[i] = {h, static_cast<uint32_t>(i)};
                }
            }));
        }

        for (auto &f : futures) {
            f.get();
        }

        std::sort(m_indexedFilelist.begin(), m_indexedFilelist.end(),
                  [](const FilelistIndexEntry &a, const FilelistIndexEntry &b) {
                      return a.hash < b.hash;
                  });

        m_indexedVersion = version;
    }

    std::optional<std::string_view>
    ProjectConfig::FindFilelistPath(uint64_t hash,
                                    ArchiveVersion version) const {
        if (m_indexedFilelist.empty() || m_indexedVersion != version) {
            return std::nullopt;
        }

        auto it = std::lower_bound(
            m_indexedFilelist.begin(), m_indexedFilelist.end(), hash,
            [](const FilelistIndexEntry &item, uint64_t targetHash) {
                return item.hash < targetHash;
            });

        if (it != m_indexedFilelist.end() && it->hash == hash) {
            return m_filelistLines[it->lineIndex];
        }
        return std::nullopt;
    }

    bool ProjectConfig::LoadConfigFile(const std::filesystem::path &jsonPath) {
        std::ifstream file(jsonPath);
        if (!file.is_open()) {
            return false;
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        return LoadConfigJson(buffer.str());
    }

    bool ProjectConfig::LoadConfigJson(std::string_view jsonContent) {
        try {
            auto j = nlohmann::json::parse(jsonContent);

            if (j.contains("name") && j["name"].is_string()) {
                m_name = j["name"].get<std::string>();
            }

            if (j.contains("fatVersion")) {
                if (j["fatVersion"].is_number()) {
                    uint32_t v = j["fatVersion"].get<uint32_t>();
                    if (v == 2)
                        m_fatVersion = ArchiveVersion::FATv2;
                    else if (v == 3)
                        m_fatVersion = ArchiveVersion::FATv3;
                    else if (v == 5)
                        m_fatVersion = ArchiveVersion::FATv5;
                } else if (j["fatVersion"].is_string()) {
                    std::string vStr = j["fatVersion"].get<std::string>();
                    m_fatVersion = ParseVersionString(vStr);
                    auto vPos = vStr.find("(v");
                    if (vPos != std::string::npos) {
                        try {
                            m_rawVersion = std::stoul(vStr.substr(vPos + 2));
                        } catch (...) {
                        }
                    }
                }
            }

            if (j.contains("fatRawVersion") && j["fatRawVersion"].is_number()) {
                m_rawVersion = j["fatRawVersion"].get<uint32_t>();
            } else if (j.contains("rawVersion") &&
                       j["rawVersion"].is_number()) {
                m_rawVersion = j["rawVersion"].get<uint32_t>();
            }

            if (j.contains("overrides") && j["overrides"].is_object()) {
                for (auto it = j["overrides"].begin();
                     it != j["overrides"].end(); ++it) {
                    auto hashOpt = ParseHashString(it.key());
                    if (hashOpt.has_value() && it.value().is_string()) {
                        AddOverride(*hashOpt, it.value().get<std::string>());
                    }
                }
            }

            return true;
        } catch (...) {
            return false;
        }
    }

    bool
    ProjectConfig::SaveConfigFile(const std::filesystem::path &jsonPath) const {
        try {
            nlohmann::ordered_json j;
            j["name"] = m_name;

            switch (m_fatVersion) {
            case ArchiveVersion::FATv2:
                j["fatVersion"] = 2;
                break;
            case ArchiveVersion::FATv3:
                j["fatVersion"] = 3;
                break;
            case ArchiveVersion::FATv5:
                j["fatVersion"] = 5;
                break;
            default:
                break;
            }

            if (m_rawVersion != 0) {
                j["fatRawVersion"] = m_rawVersion;
            }

            nlohmann::ordered_json overridesObj =
                nlohmann::ordered_json::object();
            std::vector<uint64_t> sortedHashes;
            sortedHashes.reserve(m_hashToPath.size());
            for (const auto &[hash, _] : m_hashToPath) {
                sortedHashes.push_back(hash);
            }
            std::sort(sortedHashes.begin(), sortedHashes.end());

            for (uint64_t hash : sortedHashes) {
                std::string key =
                    (m_fatVersion == ArchiveVersion::FATv3)
                        ? std::format("0x{:08X}", static_cast<uint32_t>(hash))
                        : std::format("0x{:016X}", hash);
                overridesObj[key] = m_hashToPath.at(hash);
            }
            j["overrides"] = overridesObj;

            std::ofstream file(jsonPath);
            if (!file.is_open())
                return false;
            file << j.dump(2) << "\n";
            return true;
        } catch (...) {
            return false;
        }
    }

    static bool LoadGzipFilelist(const std::filesystem::path &path,
                                 std::deque<std::string> &buffers,
                                 std::vector<std::string_view> &lines) {
        ZoneScopedN("ProjectConfig::LoadGzipFilelist");
        std::ifstream in(path, std::ios::binary);
        if (!in.is_open())
            return false;

        in.seekg(0, std::ios::end);
        std::streamsize fileSize = in.tellg();
        if (fileSize <= 0)
            return false;
        in.seekg(0, std::ios::beg);

        std::vector<uint8_t> compressed(static_cast<size_t>(fileSize));
        if (!in.read(reinterpret_cast<char *>(compressed.data()), fileSize)) {
            return false;
        }

        z_stream strm{};
        if (inflateInit2(&strm, 32 + MAX_WBITS) != Z_OK) {
            return false;
        }

        std::string &buffer = buffers.emplace_back();
        buffer.resize(32 * 1024 * 1024);
        size_t inOffset = 0;
        size_t totalProduced = 0;

        while (inOffset < compressed.size() || strm.avail_in > 0) {
            if (strm.avail_in == 0 && inOffset < compressed.size()) {
                size_t remaining = compressed.size() - inOffset;
                uInt feed = static_cast<uInt>(
                    std::min<size_t>(remaining, 64 * 1024 * 1024));
                strm.next_in = compressed.data() + inOffset;
                strm.avail_in = feed;
                inOffset += feed;
            }

            if (totalProduced + 16 * 1024 * 1024 > buffer.size()) {
                buffer.resize(buffer.size() * 2);
            }

            strm.next_out =
                reinterpret_cast<Bytef *>(buffer.data() + totalProduced);
            strm.avail_out = static_cast<uInt>(buffer.size() - totalProduced);

            int ret = inflate(&strm, Z_NO_FLUSH);
            if (ret != Z_OK && ret != Z_STREAM_END && ret != Z_BUF_ERROR) {
                inflateEnd(&strm);
                buffers.pop_back();
                return false;
            }

            totalProduced = buffer.size() - strm.avail_out;
            if (ret == Z_STREAM_END) {
                if (strm.avail_in > 0 || inOffset < compressed.size()) {
                    if (inflateReset(&strm) == Z_OK) {
                        continue;
                    }
                }
                break;
            }
        }

        inflateEnd(&strm);
        buffer.resize(totalProduced);
        buffer.shrink_to_fit();

        const char *data = buffer.data();
        size_t lineCount = 0;
        for (size_t i = 0; i < totalProduced; ++i) {
            if (data[i] == '\n')
                ++lineCount;
        }

        lines.reserve(lines.size() + lineCount + 1);

        size_t lineStart = 0;
        for (size_t i = 0; i <= totalProduced; ++i) {
            if (i == totalProduced || data[i] == '\n') {
                size_t lineEnd = i;
                while (lineEnd > lineStart && (data[lineEnd - 1] == '\r' ||
                                               data[lineEnd - 1] == ' ')) {
                    --lineEnd;
                }
                while (lineStart < lineEnd && data[lineStart] == ' ') {
                    ++lineStart;
                }
                if (lineEnd > lineStart) {
                    lines.emplace_back(data + lineStart, lineEnd - lineStart);
                }
                lineStart = i + 1;
            }
        }

        return true;
    }

    static bool LoadPlainFilelist(const std::filesystem::path &path,
                                  std::deque<std::string> &buffers,
                                  std::vector<std::string_view> &lines) {
        ZoneScopedN("ProjectConfig::LoadPlainFilelist");
        std::ifstream fl(path, std::ios::binary);
        if (!fl.is_open())
            return false;
        fl.seekg(0, std::ios::end);
        std::streamsize fileSize = fl.tellg();
        if (fileSize <= 0)
            return false;
        fl.seekg(0, std::ios::beg);

        std::string &buffer = buffers.emplace_back();
        buffer.resize(static_cast<size_t>(fileSize));
        if (!fl.read(buffer.data(), fileSize)) {
            buffers.pop_back();
            return false;
        }

        const char *data = buffer.data();
        size_t totalBytes = static_cast<size_t>(fileSize);
        size_t lineCount = 0;
        for (size_t i = 0; i < totalBytes; ++i) {
            if (data[i] == '\n')
                ++lineCount;
        }

        lines.reserve(lines.size() + lineCount + 1);

        size_t lineStart = 0;
        for (size_t i = 0; i <= totalBytes; ++i) {
            if (i == totalBytes || data[i] == '\n') {
                size_t lineEnd = i;
                while (lineEnd > lineStart && (data[lineEnd - 1] == '\r' ||
                                               data[lineEnd - 1] == ' ')) {
                    --lineEnd;
                }
                while (lineStart < lineEnd && data[lineStart] == ' ') {
                    ++lineStart;
                }
                if (lineEnd > lineStart) {
                    lines.emplace_back(data + lineStart, lineEnd - lineStart);
                }
                lineStart = i + 1;
            }
        }
        return true;
    }

    bool
    ProjectConfig::LoadFilelist(const std::filesystem::path &filelistPath) {
        ZoneScopedN("ProjectConfig::LoadFilelist");
        ClearFilelists();
        std::string ext = filelistPath.extension().string();
        std::string fname = filelistPath.filename().string();

        if (ext == ".gz" || fname.ends_with(".filelist.gz") ||
            fname.ends_with(".txt.gz")) {
            return LoadGzipFilelist(filelistPath, m_filelistBuffers,
                                    m_filelistLines);
        }
        return LoadPlainFilelist(filelistPath, m_filelistBuffers,
                                 m_filelistLines);
    }

    bool
    ProjectConfig::LoadFromDirectory(const std::filesystem::path &projectDir) {
        m_projectPath = projectDir;
        bool anyLoaded = false;

        std::filesystem::path configPath = projectDir / "projectconfig.json";
        if (std::filesystem::exists(configPath)) {
            if (LoadConfigFile(configPath)) {
                anyLoaded = true;
            }
        }

        std::filesystem::path gzPath = projectDir / "filelist.txt.gz";
        if (std::filesystem::exists(gzPath)) {
            if (LoadFilelist(gzPath)) {
                anyLoaded = true;
            }
        } else {
            std::filesystem::path plainPath = projectDir / "filelist.txt";
            if (std::filesystem::exists(plainPath)) {
                if (LoadFilelist(plainPath)) {
                    anyLoaded = true;
                }
            }
        }

        if (!anyLoaded) {
            for (const auto &entry :
                 std::filesystem::directory_iterator(projectDir)) {
                if (entry.is_regular_file()) {
                    auto ext = entry.path().extension();
                    std::string fn = entry.path().filename().string();
                    if (ext == ".filelist" || fn.ends_with(".filelist.gz")) {
                        if (LoadFilelist(entry.path())) {
                            anyLoaded = true;
                            break;
                        }
                    }
                }
            }
        }

        return anyLoaded;
    }

} // namespace Nomad::Archives
