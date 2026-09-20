#include "test_utils.hpp"
#include "Nomad/Archives/ArchiveFactory.hpp"
#include "Nomad/Archives/FATv2Archive.hpp"
#include "Nomad/Archives/FATv3Archive.hpp"
#include "Nomad/Archives/FATv5Archive.hpp"
#include "Nomad/Archives/ProjectManager.hpp"
#include <gtest/gtest.h>
#include <sstream>
#include <zlib.h>

namespace Nomad::Archives::Test {

class ArchiveFormatTest : public ::testing::Test {
protected:
    std::filesystem::path dataDir = GetTestDataDir();
};

static void WriteGzipFile(const std::filesystem::path& path, const std::string& content) {
    z_stream strm{};
    deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 16 + MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
    strm.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(content.data()));
    strm.avail_in = static_cast<uInt>(content.size());

    std::vector<uint8_t> outBuf(content.size() + 1024);
    strm.next_out = outBuf.data();
    strm.avail_out = static_cast<uInt>(outBuf.size());

    deflate(&strm, Z_FINISH);
    size_t compressedSize = outBuf.size() - strm.avail_out;
    deflateEnd(&strm);

    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(outBuf.data()), compressedSize);
}

TEST_F(ArchiveFormatTest, DetectVersions) {
    EXPECT_EQ(ArchiveFactory::DetectVersion(dataDir / "wdlegion/lz4lw/archive.fat"), ArchiveVersion::FATv5);
    EXPECT_EQ(ArchiveFactory::DetectVersion(dataDir / "wd1/uncompressed/archive.fat"), ArchiveVersion::FATv3);
    EXPECT_EQ(ArchiveFactory::DetectVersion(dataDir / "wd1/xcompress/archive.fat"), ArchiveVersion::FATv3);
    EXPECT_EQ(ArchiveFactory::DetectVersion(dataDir / "non_existent.fat"), ArchiveVersion::Unknown);
}

TEST_F(ArchiveFormatTest, OpenNonExistentAndCorrupt) {
    std::error_code ec;
    auto archive = ArchiveFactory::Open(dataDir / "does_not_exist.fat", ec);
    EXPECT_TRUE(ec);
    EXPECT_EQ(archive, nullptr);

    
    TempDirectory tempDir("nomad_test_corrupt_");
    auto corruptFat = tempDir.GetPath() / "corrupt.fat";
    {
        std::ofstream f(corruptFat, std::ios::binary);
        f.write("BADC", 4);
    }
    ec.clear();
    archive = ArchiveFactory::Open(corruptFat, ec);
    EXPECT_TRUE(ec);
    EXPECT_EQ(archive, nullptr);
}

TEST_F(ArchiveFormatTest, FATv2SerializationRoundTrip) {
    auto archive = ArchiveFactory::Create(ArchiveVersion::FATv2);
    ASSERT_NE(archive, nullptr);

    archive->AddOrUpdateEntry("worlds/city.xml", 2000, 500, CompressionScheme::LZO1x, 0);
    archive->AddOrUpdateEntry("ui/hud.bin", 400, 400, CompressionScheme::None, 512);

    std::stringstream ss;
    ASSERT_TRUE(archive->Write(ss));

    auto loaded = ArchiveFactory::Create(ArchiveVersion::FATv2);
    ASSERT_NE(loaded, nullptr);
    ASSERT_TRUE(loaded->Read(ss));

    ASSERT_EQ(loaded->GetEntries().size(), 2);
    EXPECT_EQ(loaded->GetEntries()[0].offset, 0);
    EXPECT_EQ(loaded->GetEntries()[1].offset, 512);
}

TEST_F(ArchiveFormatTest, FATv3SerializationRoundTrip) {
    auto archive = ArchiveFactory::Create(ArchiveVersion::FATv3);
    ASSERT_NE(archive, nullptr);

    archive->AddOrUpdateEntry("config/input.xml", 1024, 256, CompressionScheme::XMemCompress, 16);
    archive->AddOrUpdateEntry("sound/music.spk", 5000, 5000, CompressionScheme::None, 320);

    std::stringstream ss;
    ASSERT_TRUE(archive->Write(ss));

    auto loaded = ArchiveFactory::Create(ArchiveVersion::FATv3);
    ASSERT_NE(loaded, nullptr);
    ASSERT_TRUE(loaded->Read(ss));

    ASSERT_EQ(loaded->GetEntries().size(), 2);
    EXPECT_EQ(loaded->GetEntries()[0].offset, 16);
    EXPECT_EQ(loaded->GetEntries()[1].offset, 320);
}

TEST_F(ArchiveFormatTest, FATv5SerializationRoundTripV11AndV13) {
    for (uint32_t rawVer : {11u, 13u}) {
        auto archive = ArchiveFactory::Create(ArchiveVersion::FATv5);
        ASSERT_NE(archive, nullptr);
        archive->SetRawVersion(rawVer);
        archive->SetPlatform(Platform::Win64);

        archive->AddOrUpdateEntry("engine/settings/game.xml", 1200, 450, CompressionScheme::LZ4LW, 64);
        archive->AddOrUpdateEntry("engine/settings/render.xml", 800, 800, CompressionScheme::None, 528);

        std::stringstream ss;
        ASSERT_TRUE(archive->Write(ss));

        auto loaded = ArchiveFactory::Create(ArchiveVersion::FATv5);
        ASSERT_NE(loaded, nullptr);
        ASSERT_TRUE(loaded->Read(ss));

        ASSERT_EQ(loaded->GetEntries().size(), 2);
        EXPECT_EQ(loaded->GetRawVersion(), rawVer);
        EXPECT_EQ(loaded->GetEntries()[0].offset, 64);
        EXPECT_EQ(loaded->GetEntries()[1].offset, 528);
    }
}

TEST_F(ArchiveFormatTest, FATv5Extended64BitOffsetRoundTrip) {
    auto archive = ArchiveFactory::Create(ArchiveVersion::FATv5);
    ASSERT_NE(archive, nullptr);
    archive->SetRawVersion(13u);
    archive->SetPlatform(Platform::Win64);

    
    archive->AddOrUpdateEntry("engine/settings/game.xml", 1200, 450, CompressionScheme::LZ4LW, 1024);
    
    uint64_t hugeOffset = 17926444922ULL;
    archive->AddOrUpdateEntry("worlds/london/huge_asset.dat", 2073780, 917018, CompressionScheme::LZ4LW, hugeOffset);

    std::stringstream ss;
    ASSERT_TRUE(archive->Write(ss));

    auto loaded = ArchiveFactory::Create(ArchiveVersion::FATv5);
    ASSERT_NE(loaded, nullptr);
    ASSERT_TRUE(loaded->Read(ss));

    ASSERT_EQ(loaded->GetEntries().size(), 2);
    EXPECT_EQ(loaded->GetRawVersion(), 13u);

    auto fat5 = dynamic_cast<FATv5Archive*>(loaded.get());
    ASSERT_NE(fat5, nullptr);
    EXPECT_EQ(fat5->GetDuplicates().size(), 1);

    bool foundHuge = false;
    for (const auto& entry : loaded->GetEntries()) {
        if (entry.offset == hugeOffset) {
            foundHuge = true;
            EXPECT_EQ(entry.compressed_size, 917018);
            EXPECT_EQ(entry.uncompressed_size, 2073780);
            EXPECT_EQ(entry.compression, CompressionScheme::LZ4LW);
        }
    }
    EXPECT_TRUE(foundHuge);
}

TEST_F(ArchiveFormatTest, SerializeNfo) {
    auto archive = ArchiveFactory::Create(ArchiveVersion::FATv3);
    ASSERT_NE(archive, nullptr);
    archive->AddOrUpdateEntry("my/test/file.txt", 100, 80, CompressionScheme::LZO1x, 0);

    std::stringstream ss;
    ASSERT_TRUE(archive->SerializeNfo(ss));

    std::string nfoStr = ss.str();
    EXPECT_NE(nfoStr.find("<Root>"), std::string::npos);
    EXPECT_NE(nfoStr.find("my\\test\\file.txt"), std::string::npos);
    EXPECT_NE(nfoStr.find("</Root>"), std::string::npos);
}

TEST_F(ArchiveFormatTest, ProjectManagerResolution) {
    auto& pm = ProjectManager::GetInstance();
    pm.SetProjectsRoot("");
    auto defaultRoot = pm.GetProjectsRoot();
    EXPECT_FALSE(defaultRoot.empty());

    TempDirectory tempDir("nomad_test_projects_");
    pm.SetProjectsRoot(tempDir.GetPath());
    EXPECT_EQ(pm.GetProjectsRoot(), tempDir.GetPath());

    pm.SetProjectsRoot("");
    EXPECT_EQ(pm.GetProjectsRoot(), defaultRoot);
}

TEST_F(ArchiveFormatTest, GzipFilelistDecompressInMemory) {
    TempDirectory tempDir("nomad_test_gz_filelist_");
    auto projectDir = tempDir.GetPath();

    std::string filelistText =
        "worlds/city/props/bench.xml\r\n"
        "ui/textures/minimap.xbt\n"
        "   engine/shaders/water.bin   \r\n"
        "\n"
        "scripts/main.lua\n";

    WriteGzipFile(projectDir / "filelist.txt.gz", filelistText);

    ProjectConfig config;
    EXPECT_TRUE(config.LoadFromDirectory(projectDir));

    const auto& lines = config.GetFilelistLines();
    ASSERT_EQ(lines.size(), 4);
    EXPECT_EQ(lines[0], "worlds/city/props/bench.xml");
    EXPECT_EQ(lines[1], "ui/textures/minimap.xbt");
    EXPECT_EQ(lines[2], "engine/shaders/water.bin");
    EXPECT_EQ(lines[3], "scripts/main.lua");

    
    ProjectConfig configDirect;
    EXPECT_TRUE(configDirect.LoadFilelist(projectDir / "filelist.txt.gz"));
    EXPECT_EQ(configDirect.GetFilelistLines().size(), 4);
}


TEST_F(ArchiveFormatTest, CapacityLimitsAndValidation) {
    
    {
        auto v3v8 = ArchiveFactory::Create(ArchiveVersion::FATv3);
        v3v8->SetRawVersion(8);
        auto limits = v3v8->GetCapacityLimits();
        EXPECT_EQ(limits.maxUncompressedSize, 0x1FFFFFFFu);
        EXPECT_EQ(limits.maxCompressedSize, 0x1FFFFFFFu);
        EXPECT_EQ(limits.maxOffset, (1ULL << 35) - 1);

        std::string err;
        EXPECT_TRUE(v3v8->Validate(err));

        
        v3v8->AddOrUpdateEntry("large_file.dat", 600ULL * 1024 * 1024, 100ULL, CompressionScheme::None, 0);
        EXPECT_FALSE(v3v8->Validate(err));
        EXPECT_TRUE(err.find("large_file.dat") != std::string::npos);
        EXPECT_TRUE(err.find("512.00 MB") != std::string::npos);

        
        std::stringstream ss;
        EXPECT_FALSE(v3v8->Write(ss));

        
        v3v8->AddOrUpdateEntry("large_file.dat", 500ULL * 1024 * 1024, 100ULL, CompressionScheme::None, 0);
        EXPECT_TRUE(v3v8->Validate(err));
        EXPECT_TRUE(v3v8->Write(ss));
    }

    
    {
        auto v3v7 = ArchiveFactory::Create(ArchiveVersion::FATv3);
        v3v7->SetRawVersion(7);
        auto limits = v3v7->GetCapacityLimits();
        EXPECT_EQ(limits.maxUncompressedSize, 0x3FFFFFFFu);
        EXPECT_EQ(limits.maxCompressedSize, 0x3FFFFFFFu);

        std::string err;
        
        v3v7->AddOrUpdateEntry("file_600mb.dat", 600ULL * 1024 * 1024, 100ULL, CompressionScheme::None, 0);
        EXPECT_TRUE(v3v7->Validate(err));

        
        v3v7->AddOrUpdateEntry("file_1200mb.dat", 1200ULL * 1024 * 1024, 100ULL, CompressionScheme::None, 0);
        EXPECT_FALSE(v3v7->Validate(err));
    }

    
    {
        auto v2 = ArchiveFactory::Create(ArchiveVersion::FATv2);
        v2->SetRawVersion(8);
        auto limits = v2->GetCapacityLimits();
        EXPECT_EQ(limits.maxUncompressedSize, 0x1FFFFFFFu);

        std::string err;
        v2->AddOrUpdateEntry("oversized_v2.dat", 600ULL * 1024 * 1024, 100ULL, CompressionScheme::None, 0);
        EXPECT_FALSE(v2->Validate(err));

        std::stringstream ss;
        EXPECT_FALSE(v2->Write(ss));
    }

    
    {
        auto v5v11 = ArchiveFactory::Create(ArchiveVersion::FATv5);
        v5v11->SetRawVersion(11);
        auto limits = v5v11->GetCapacityLimits();
        EXPECT_EQ(limits.maxUncompressedSize, 0x3FFFFFFFu);

        std::string err;
        v5v11->AddOrUpdateEntry("oversized_v5_v11.dat", 1200ULL * 1024 * 1024, 100ULL, CompressionScheme::None, 0);
        EXPECT_FALSE(v5v11->Validate(err));
    }

    
    {
        auto v5v13 = ArchiveFactory::Create(ArchiveVersion::FATv5);
        v5v13->SetRawVersion(13);
        auto limits = v5v13->GetCapacityLimits();
        EXPECT_EQ(limits.maxUncompressedSize, 0xFFFFFFFFULL);

        std::string err;
        
        v5v13->AddOrUpdateEntry("valid_2gb.dat", 2ULL * 1024 * 1024 * 1024, 100ULL, CompressionScheme::None, 0);
        EXPECT_TRUE(v5v13->Validate(err));

        
        v5v13->AddOrUpdateEntry("oversized_5gb.dat", 5ULL * 1024 * 1024 * 1024, 100ULL, CompressionScheme::None, 0);
        EXPECT_FALSE(v5v13->Validate(err));
    }
}

} 

