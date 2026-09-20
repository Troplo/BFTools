#include "test_utils.hpp"
#include "Nomad/Archives/ArchiveFactory.hpp"
#include "Nomad/Archives/Defragmenter.hpp"
#include "Nomad/Archives/Extractor.hpp"
#include <gtest/gtest.h>

namespace Nomad::Archives::Test {

class DefragmenterTest : public ::testing::Test {
protected:
    std::filesystem::path dataDir = GetTestDataDir();
};

TEST_F(DefragmenterTest, AnalyzeAndDefragmentArchive) {
    auto origFat = dataDir / "wd1/uncompressed/archive.fat";
    auto origDat = dataDir / "wd1/uncompressed/archive.dat";
    auto origNfo = dataDir / "wd1/uncompressed/archive.nfo";
    ASSERT_TRUE(std::filesystem::exists(origFat));
    ASSERT_TRUE(std::filesystem::exists(origDat));
    ASSERT_TRUE(std::filesystem::exists(origNfo));

    TempDirectory tempDir("nomad_test_defrag_");
    auto testFat = tempDir.GetPath() / "archive.fat";
    auto testDat = tempDir.GetPath() / "archive.dat";
    auto testNfo = tempDir.GetPath() / "archive.nfo";
    std::filesystem::copy_file(origFat, testFat);
    std::filesystem::copy_file(origDat, testDat);
    std::filesystem::copy_file(origNfo, testNfo);

    std::error_code ec;
    auto archive = ArchiveFactory::Open(testFat, ec);
    ASSERT_FALSE(ec);
    ASSERT_NE(archive, nullptr);

    auto statsBefore = Defragmenter::Analyze(testFat, *archive);
    EXPECT_EQ(statsBefore.entryCount, archive->GetEntries().size());
    EXPECT_GT(statsBefore.currentDatSize, 0);

    std::string err;
    bool defragOk = Defragmenter::Defragment(testFat, *archive, err);
    ASSERT_TRUE(defragOk) << "Defragment failed: " << err;

    
    ec.clear();
    auto reloadedArchive = ArchiveFactory::Open(testFat, ec);
    ASSERT_FALSE(ec);
    ASSERT_NE(reloadedArchive, nullptr);
    ASSERT_EQ(reloadedArchive->GetEntries().size(), statsBefore.entryCount);

    TempDirectory extractDir("nomad_test_defrag_extract_");
    bool extractOk = Extractor::ExtractAll(*reloadedArchive, testFat, extractDir.GetPath(), nullptr, 0);
    ASSERT_TRUE(extractOk);

    CompareDirectoriesByteForByte(extractDir.GetPath(), dataDir / "wd1/uncompressed/archive", true);
}

} 
