#ifndef TESTS_TEST_UTILS_H
#define TESTS_TEST_UTILS_H

#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <chrono>
#include <gtest/gtest.h>

#ifndef TEST_DATA_DIR
#define TEST_DATA_DIR "tests/data"
#endif

namespace Nomad::Archives::Test {

inline std::filesystem::path GetTestDataDir() {
    return std::filesystem::path(TEST_DATA_DIR);
}

class TempDirectory {
public:
    TempDirectory(const std::string& prefix = "nomad_test_") {
        auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        m_path = std::filesystem::temp_directory_path() / (prefix + std::to_string(now));
        std::filesystem::create_directories(m_path);
    }

    ~TempDirectory() {
        std::error_code ec;
        std::filesystem::remove_all(m_path, ec);
    }

    [[nodiscard]] const std::filesystem::path& GetPath() const { return m_path; }
    [[nodiscard]] std::string string() const { return m_path.string(); }
    operator const std::filesystem::path&() const { return m_path; }

    TempDirectory(const TempDirectory&) = delete;
    TempDirectory& operator=(const TempDirectory&) = delete;
    TempDirectory(TempDirectory&&) = default;
    TempDirectory& operator=(TempDirectory&&) = default;

private:
    std::filesystem::path m_path;
};

inline bool FilesAreEqual(const std::filesystem::path& p1, const std::filesystem::path& p2) {
    if (!std::filesystem::exists(p1) || !std::filesystem::exists(p2)) return false;
    if (std::filesystem::file_size(p1) != std::filesystem::file_size(p2)) return false;

    std::ifstream f1(p1, std::ios::binary);
    std::ifstream f2(p2, std::ios::binary);
    if (!f1.is_open() || !f2.is_open()) return false;

    constexpr size_t bufferSize = 64 * 1024;
    std::vector<char> b1(bufferSize);
    std::vector<char> b2(bufferSize);

    while (f1.good() && f2.good()) {
        f1.read(b1.data(), bufferSize);
        f2.read(b2.data(), bufferSize);
        std::streamsize bytes1 = f1.gcount();
        std::streamsize bytes2 = f2.gcount();
        if (bytes1 != bytes2) return false;
        if (std::memcmp(b1.data(), b2.data(), static_cast<size_t>(bytes1)) != 0) return false;
    }
    return true;
}

inline void CompareDirectoriesByteForByte(const std::filesystem::path& actualDir,
                                         const std::filesystem::path& expectedDir,
                                         bool caseInsensitiveFilenames = false) {
    ASSERT_TRUE(std::filesystem::exists(actualDir)) << "Actual directory does not exist: " << actualDir;
    ASSERT_TRUE(std::filesystem::exists(expectedDir)) << "Expected directory does not exist: " << expectedDir;

    
    auto toLower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
        return s;
    };

    std::vector<std::pair<std::string, std::filesystem::path>> expectedFiles;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(expectedDir)) {
        if (entry.is_regular_file()) {
            std::string rel = std::filesystem::relative(entry.path(), expectedDir).generic_string();
            expectedFiles.emplace_back(caseInsensitiveFilenames ? toLower(rel) : rel, entry.path());
        }
    }

    std::vector<std::pair<std::string, std::filesystem::path>> actualFiles;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(actualDir)) {
        if (entry.is_regular_file()) {
            std::string rel = std::filesystem::relative(entry.path(), actualDir).generic_string();
            actualFiles.emplace_back(caseInsensitiveFilenames ? toLower(rel) : rel, entry.path());
        }
    }

    ASSERT_EQ(actualFiles.size(), expectedFiles.size())
        << "File count mismatch between " << actualDir << " and " << expectedDir;

    
    for (const auto& [expectedRel, expectedPath] : expectedFiles) {
        auto it = std::find_if(actualFiles.begin(), actualFiles.end(),
            [&](const auto& item) { return item.first == expectedRel; });
        ASSERT_NE(it, actualFiles.end()) << "Expected file missing in extracted directory: " << expectedRel;

        const auto& actualPath = it->second;
        uintmax_t expSize = std::filesystem::file_size(expectedPath);
        uintmax_t actSize = std::filesystem::file_size(actualPath);
        ASSERT_EQ(actSize, expSize) << "File size mismatch for: " << expectedRel;

        EXPECT_TRUE(FilesAreEqual(actualPath, expectedPath))
            << "Byte-for-byte mismatch in file: " << expectedRel;
    }
}

} 

#endif 
