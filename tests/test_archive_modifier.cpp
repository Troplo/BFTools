#include "test_utils.hpp"
#include "Nomad/Archives/ArchiveFactory.hpp"
#include "Nomad/Archives/ArchiveModifier.hpp"
#include "Nomad/Archives/Extractor.hpp"
#include <gtest/gtest.h>

namespace Nomad::Archives::Test {

class ArchiveModifierTest : public ::testing::Test {
protected:
    std::filesystem::path dataDir = GetTestDataDir();
};

TEST_F(ArchiveModifierTest, AppendFileAndReload) {
    auto origFat = dataDir / "wd1/uncompressed/archive.fat";
    auto origDat = dataDir / "wd1/uncompressed/archive.dat";
    ASSERT_TRUE(std::filesystem::exists(origFat));
    ASSERT_TRUE(std::filesystem::exists(origDat));

    TempDirectory tempDir("nomad_test_mod_");
    auto testFat = tempDir.GetPath() / "archive.fat";
    auto testDat = tempDir.GetPath() / "archive.dat";
    std::filesystem::copy_file(origFat, testFat);
    std::filesystem::copy_file(origDat, testDat);

    std::error_code ec;
    auto archive = ArchiveFactory::Open(testFat, ec);
    ASSERT_FALSE(ec);
    ASSERT_NE(archive, nullptr);
    size_t initialCount = archive->GetEntries().size();

    
    auto newFile = tempDir.GetPath() / "added_file.txt";
    std::string addedContent = "Nomad.Archives AppendFile test payload 12345!";
    {
        std::ofstream f(newFile);
        f << addedContent;
    }

    std::string err;
    bool appendOk = ArchiveModifier::AppendFile(testFat, *archive, "test/appended.txt", newFile, CompressionScheme::None, err);
    ASSERT_TRUE(appendOk) << "AppendFile failed: " << err;
    EXPECT_EQ(archive->GetEntries().size(), initialCount + 1);

    
    bool saveOk = ArchiveModifier::SaveMetadata(testFat, *archive, err);
    ASSERT_TRUE(saveOk) << "SaveMetadata failed: " << err;

    
    ec.clear();
    auto reloadedArchive = ArchiveFactory::Open(testFat, ec);
    ASSERT_FALSE(ec);
    ASSERT_NE(reloadedArchive, nullptr);
    ASSERT_EQ(reloadedArchive->GetEntries().size(), initialCount + 1);

    TempDirectory extractDir("nomad_test_mod_extract_");
    bool extractOk = Extractor::ExtractAll(*reloadedArchive, testFat, extractDir.GetPath(), nullptr, 0);
    ASSERT_TRUE(extractOk);

    auto extractedFile = extractDir.GetPath() / "test/appended.txt";
    ASSERT_TRUE(std::filesystem::exists(extractedFile));
    std::ifstream in(extractedFile);
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    EXPECT_EQ(content, addedContent);
}

TEST_F(ArchiveModifierTest, MergeArchives) {
    auto srcFat = dataDir / "wd1/xcompress/archive.fat";
    auto srcDat = dataDir / "wd1/xcompress/archive.dat";
    auto targetFatOrig = dataDir / "wd1/uncompressed/archive.fat";
    auto targetDatOrig = dataDir / "wd1/uncompressed/archive.dat";

    TempDirectory tempDir("nomad_test_merge_");
    auto targetFat = tempDir.GetPath() / "merged.fat";
    auto targetDat = tempDir.GetPath() / "merged.dat";
    std::filesystem::copy_file(targetFatOrig, targetFat);
    std::filesystem::copy_file(targetDatOrig, targetDat);

    std::error_code ec;
    auto targetArchive = ArchiveFactory::Open(targetFat, ec);
    ASSERT_FALSE(ec);
    auto sourceArchive = ArchiveFactory::Open(srcFat, ec);
    ASSERT_FALSE(ec);

    size_t mergedCount = 0;
    std::string err;
    bool mergeOk = ArchiveModifier::MergeArchive(targetFat, *targetArchive, srcFat, *sourceArchive, mergedCount, err);
    ASSERT_TRUE(mergeOk) << "MergeArchive failed: " << err;
}

} 
