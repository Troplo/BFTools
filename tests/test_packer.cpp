#include "test_utils.hpp"
#include "Nomad/Archives/ArchiveFactory.hpp"
#include "Nomad/Archives/Extractor.hpp"
#include "Nomad/Archives/Compression.hpp"
#include <gtest/gtest.h>

namespace Nomad::Archives::Test {

static bool PackDirectory(
    const std::filesystem::path& inputDir,
    const std::filesystem::path& fatOutPath,
    ArchiveVersion version,
    uint32_t rawVersion,
    CompressionScheme scheme)
{
    auto archive = ArchiveFactory::Create(version);
    if (!archive) return false;
    if (rawVersion != 0) {
        archive->SetRawVersion(rawVersion);
    }

    std::filesystem::path datOutPath = fatOutPath;
    datOutPath.replace_extension(".dat");

    std::ofstream datStream(datOutPath, std::ios::binary);
    if (!datStream.is_open()) return false;

    for (const auto& dirEntry : std::filesystem::recursive_directory_iterator(inputDir)) {
        if (!dirEntry.is_regular_file()) continue;

        std::filesystem::path relPath = std::filesystem::relative(dirEntry.path(), inputDir);
        std::string relPathStr = NormalizePath(relPath.string());
        uintmax_t fileSize = dirEntry.file_size();

        std::ifstream srcFile(dirEntry.path(), std::ios::binary);
        if (!srcFile.is_open()) return false;

        std::vector<uint8_t> uncompressedData(fileSize);
        if (fileSize > 0) {
            srcFile.read(reinterpret_cast<char*>(uncompressedData.data()), fileSize);
        }

        CompressionScheme actualScheme = CompressionScheme::None;
        std::vector<uint8_t> payloadData;

        if (fileSize <= 31 || scheme == CompressionScheme::None) {
            actualScheme = CompressionScheme::None;
            payloadData = std::move(uncompressedData);
        } else {
            std::string compErr;
            if (Compression::Compress(scheme, uncompressedData.data(), uncompressedData.size(), payloadData, compErr) &&
                payloadData.size() < uncompressedData.size()) {
                actualScheme = scheme;
            } else {
                actualScheme = CompressionScheme::None;
                payloadData = std::move(uncompressedData);
            }
        }

        uint64_t currPos = static_cast<uint64_t>(datStream.tellp());
        uint64_t alignedOffset = AlignUp(currPos, 16ULL);
        if (alignedOffset > currPos) {
            char pad[16] = {0};
            datStream.write(pad, alignedOffset - currPos);
        }

        if (!payloadData.empty()) {
            datStream.write(reinterpret_cast<const char*>(payloadData.data()), payloadData.size());
        }

        archive->AddOrUpdateEntry(relPathStr, fileSize, payloadData.size(), actualScheme, alignedOffset);
    }

    datStream.close();

    std::ofstream fatStream(fatOutPath, std::ios::binary);
    if (!fatStream.is_open() || !archive->Write(fatStream)) return false;
    fatStream.close();

    std::filesystem::path nfoOutPath = fatOutPath;
    nfoOutPath.replace_extension(".nfo");
    std::ofstream nfoStream(nfoOutPath);
    if (nfoStream.is_open()) {
        archive->SerializeNfo(nfoStream);
        nfoStream.close();
    }

    return true;
}

class PackerTest : public ::testing::Test {
protected:
    std::filesystem::path dataDir = GetTestDataDir();
};

TEST_F(PackerTest, PackWDLegionFATv5LZ4LWRoundTrip) {
    auto srcDir = dataDir / "wdlegion/lz4lw/archive";
    ASSERT_TRUE(std::filesystem::exists(srcDir));

    TempDirectory packDir("nomad_test_pack_v5_");
    auto fatPath = packDir.GetPath() / "archive.fat";

    bool packOk = PackDirectory(srcDir, fatPath, ArchiveVersion::FATv5, 13, CompressionScheme::LZ4LW);
    ASSERT_TRUE(packOk) << "Packing FATv5 archive failed";

    std::error_code ec;
    auto archive = ArchiveFactory::Open(fatPath, ec);
    ASSERT_FALSE(ec) << "Failed to open newly packed archive: " << ec.message();
    ASSERT_NE(archive, nullptr);
    ASSERT_EQ(archive->GetVersion(), ArchiveVersion::FATv5);
    ASSERT_EQ(archive->GetEntries().size(), 40);

    TempDirectory unpackDir("nomad_test_unpack_v5_");
    bool extractOk = Extractor::ExtractAll(*archive, fatPath, unpackDir.GetPath(), nullptr, 0);
    ASSERT_TRUE(extractOk) << "Extracting newly packed archive failed";

    CompareDirectoriesByteForByte(unpackDir.GetPath(), srcDir);
}

TEST_F(PackerTest, PackWD1FATv3UncompressedRoundTrip) {
    auto srcDir = dataDir / "wd1/uncompressed/archive";
    ASSERT_TRUE(std::filesystem::exists(srcDir));

    TempDirectory packDir("nomad_test_pack_v3_");
    auto fatPath = packDir.GetPath() / "archive.fat";

    bool packOk = PackDirectory(srcDir, fatPath, ArchiveVersion::FATv3, 7, CompressionScheme::None);
    ASSERT_TRUE(packOk) << "Packing FATv3 archive failed";

    std::error_code ec;
    auto archive = ArchiveFactory::Open(fatPath, ec);
    ASSERT_FALSE(ec);
    ASSERT_NE(archive, nullptr);
    ASSERT_EQ(archive->GetVersion(), ArchiveVersion::FATv3);
    ASSERT_EQ(archive->GetEntries().size(), 4);

    TempDirectory unpackDir("nomad_test_unpack_v3_");
    bool extractOk = Extractor::ExtractAll(*archive, fatPath, unpackDir.GetPath(), nullptr, 0);
    ASSERT_TRUE(extractOk);

    CompareDirectoriesByteForByte(unpackDir.GetPath(), srcDir, true);
}

TEST_F(PackerTest, PackFATv2RoundTrip) {
    auto srcDir = dataDir / "wd1/uncompressed/archive";
    ASSERT_TRUE(std::filesystem::exists(srcDir));

    TempDirectory packDir("nomad_test_pack_v2_");
    auto fatPath = packDir.GetPath() / "archive.fat";

    bool packOk = PackDirectory(srcDir, fatPath, ArchiveVersion::FATv2, 2, CompressionScheme::None);
    ASSERT_TRUE(packOk) << "Packing FATv2 archive failed";

    std::error_code ec;
    auto archive = ArchiveFactory::Open(fatPath, ec);
    ASSERT_FALSE(ec);
    ASSERT_NE(archive, nullptr);
    ASSERT_EQ(archive->GetVersion(), ArchiveVersion::FATv2);
    ASSERT_EQ(archive->GetEntries().size(), 4);

    TempDirectory unpackDir("nomad_test_unpack_v2_");
    bool extractOk = Extractor::ExtractAll(*archive, fatPath, unpackDir.GetPath(), nullptr, 0);
    ASSERT_TRUE(extractOk);

    CompareDirectoriesByteForByte(unpackDir.GetPath(), srcDir, true);
}

} 
