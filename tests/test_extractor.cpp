#include "test_utils.hpp"
#include "Nomad/Archives/ArchiveFactory.hpp"
#include "Nomad/Archives/Extractor.hpp"
#include <gtest/gtest.h>

namespace Nomad::Archives::Test {

class ExtractorTest : public ::testing::Test {
protected:
    std::filesystem::path dataDir = GetTestDataDir();
};

TEST_F(ExtractorTest, ExtractWDLegionLZ4LWByteForByte) {
    auto fatPath = dataDir / "wdlegion/lz4lw/archive.fat";
    auto expectedDir = dataDir / "wdlegion/lz4lw/archive";

    ASSERT_TRUE(std::filesystem::exists(fatPath)) << "Test archive not found: " << fatPath;
    ASSERT_TRUE(std::filesystem::exists(expectedDir)) << "Expected dir not found: " << expectedDir;

    std::error_code ec;
    auto archive = ArchiveFactory::Open(fatPath, ec);
    ASSERT_FALSE(ec) << "Failed to open archive: " << ec.message();
    ASSERT_NE(archive, nullptr);
    ASSERT_EQ(archive->GetVersion(), ArchiveVersion::FATv5);
    ASSERT_EQ(archive->GetEntries().size(), 40);

    TempDirectory tempDir("nomad_test_wdlegion_");
    bool ok = Extractor::ExtractAll(*archive, fatPath, tempDir.GetPath(), nullptr, 0);
    ASSERT_TRUE(ok) << "Extraction failed for " << fatPath;

    CompareDirectoriesByteForByte(tempDir.GetPath(), expectedDir);
}

TEST_F(ExtractorTest, ExtractWD1XCompressByteForByte) {
    auto fatPath = dataDir / "wd1/xcompress/archive.fat";
    auto expectedDir = dataDir / "wd1/xcompress/archive";

    ASSERT_TRUE(std::filesystem::exists(fatPath)) << "Test archive not found: " << fatPath;
    ASSERT_TRUE(std::filesystem::exists(expectedDir)) << "Expected dir not found: " << expectedDir;

    std::error_code ec;
    auto archive = ArchiveFactory::Open(fatPath, ec);
    ASSERT_FALSE(ec) << "Failed to open archive: " << ec.message();
    ASSERT_NE(archive, nullptr);
    ASSERT_EQ(archive->GetVersion(), ArchiveVersion::FATv3);
    ASSERT_EQ(archive->GetEntries().size(), 4);

    TempDirectory tempDir("nomad_test_wd1_xcompress_");
    bool ok = Extractor::ExtractAll(*archive, fatPath, tempDir.GetPath(), nullptr, 0);
    ASSERT_TRUE(ok) << "Extraction failed for " << fatPath;

    CompareDirectoriesByteForByte(tempDir.GetPath(), expectedDir);
}

TEST_F(ExtractorTest, ExtractWD1UncompressedByteForByte) {
    auto fatPath = dataDir / "wd1/uncompressed/archive.fat";
    auto expectedDir = dataDir / "wd1/uncompressed/archive";

    ASSERT_TRUE(std::filesystem::exists(fatPath)) << "Test archive not found: " << fatPath;
    ASSERT_TRUE(std::filesystem::exists(expectedDir)) << "Expected dir not found: " << expectedDir;

    std::error_code ec;
    auto archive = ArchiveFactory::Open(fatPath, ec);
    ASSERT_FALSE(ec) << "Failed to open archive: " << ec.message();
    ASSERT_NE(archive, nullptr);
    ASSERT_EQ(archive->GetVersion(), ArchiveVersion::FATv3);
    ASSERT_EQ(archive->GetEntries().size(), 4);

    TempDirectory tempDir("nomad_test_wd1_uncompressed_");
    bool ok = Extractor::ExtractAll(*archive, fatPath, tempDir.GetPath(), nullptr, 0);
    ASSERT_TRUE(ok) << "Extraction failed for " << fatPath;

    
    CompareDirectoriesByteForByte(tempDir.GetPath(), expectedDir, true);
}

TEST_F(ExtractorTest, ExtractWD1FATv2ByteForByte) {
    auto fatPath = dataDir / "wd1_fat2/archive.fat";
    auto expectedDir = dataDir / "wd1_fat2/archive";

    ASSERT_TRUE(std::filesystem::exists(fatPath)) << "Test archive not found: " << fatPath;
    ASSERT_TRUE(std::filesystem::exists(expectedDir)) << "Expected dir not found: " << expectedDir;

    std::error_code ec;
    auto archive = ArchiveFactory::Open(fatPath, ec);
    ASSERT_FALSE(ec) << "Failed to open archive: " << ec.message();
    ASSERT_NE(archive, nullptr);
    ASSERT_EQ(archive->GetVersion(), ArchiveVersion::FATv2);
    ASSERT_EQ(archive->GetEntries().size(), 4);

    TempDirectory tempDir("nomad_test_wd1_fat2_");
    bool ok = Extractor::ExtractAll(*archive, fatPath, tempDir.GetPath(), nullptr, 0);
    ASSERT_TRUE(ok) << "Extraction failed for " << fatPath;

    CompareDirectoriesByteForByte(tempDir.GetPath(), expectedDir, true);
}

TEST_F(ExtractorTest, ExtractSingleEntry) {
    auto fatPath = dataDir / "wdlegion/lz4lw/archive.fat";
    auto expectedFilePath = dataDir / "wdlegion/lz4lw/archive/engine/settings/defaultnetworkconfig.xml";

    std::error_code ec;
    auto archive = ArchiveFactory::Open(fatPath, ec);
    ASSERT_FALSE(ec);
    ASSERT_NE(archive, nullptr);

    const ArchiveEntry* targetEntry = nullptr;
    for (const auto& entry : archive->GetEntries()) {
        if (entry.path == "engine/settings/defaultnetworkconfig.xml") {
            targetEntry = &entry;
            break;
        }
    }
    ASSERT_NE(targetEntry, nullptr);

    auto datPath = fatPath;
    datPath.replace_extension(".dat");
    std::ifstream datStream(datPath, std::ios::binary);
    ASSERT_TRUE(datStream.is_open());

    TempDirectory tempDir("nomad_test_single_");
    auto outPath = tempDir.GetPath() / "extracted.xml";
    bool ok = Extractor::ExtractEntry(*targetEntry, datStream, outPath);
    ASSERT_TRUE(ok);

    EXPECT_TRUE(FilesAreEqual(outPath, expectedFilePath));
}

} 
