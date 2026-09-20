#include "test_utils.hpp"
#include "Nomad/Archives/Compression.hpp"
#include <gtest/gtest.h>
#include <random>

namespace Nomad::Archives::Test {

static std::vector<uint8_t> GenerateTestData(size_t size) {
    std::vector<uint8_t> data(size);
    std::string pattern = "The quick brown fox jumps over the lazy dog. 1234567890! Special XML: <config attr=\"true\"/>";
    for (size_t i = 0; i < size; ++i) {
        data[i] = static_cast<uint8_t>(pattern[i % pattern.size()] ^ (i % 7));
    }
    return data;
}

class CompressionTest : public ::testing::Test {};

TEST_F(CompressionTest, LZ4LWRoundTrip) {
    const std::vector<size_t> testSizes = {32, 100, 1024, 8192, 65536, 262144};
    for (size_t size : testSizes) {
        auto original = GenerateTestData(size);
        std::vector<uint8_t> compressed;
        std::string err;

        bool compOk = Compression::Compress(CompressionScheme::LZ4LW, original.data(), original.size(), compressed, err);
        ASSERT_TRUE(compOk) << "Compression failed for size " << size << ": " << err;
        ASSERT_GT(compressed.size(), 0);

        std::vector<uint8_t> decompressed(original.size());
        bool decompOk = Compression::Decompress(CompressionScheme::LZ4LW, compressed.data(), compressed.size(), decompressed.data(), decompressed.size(), err);
        ASSERT_TRUE(decompOk) << "Decompression failed for size " << size << ": " << err;

        EXPECT_EQ(original, decompressed) << "Byte mismatch for size " << size;
    }
}

TEST_F(CompressionTest, LZ4LWBoundaryAndEdgeCases) {
    std::string err;
    
    std::vector<uint8_t> smallData = {1, 2, 3, 4, 5};
    std::vector<uint8_t> comp;
    EXPECT_TRUE(Compression::Compress(CompressionScheme::LZ4LW, smallData.data(), smallData.size(), comp, err));
    EXPECT_EQ(comp, smallData);

    
    std::vector<uint8_t> corrupt(64, 0xFF);
    std::vector<uint8_t> out(128);
    EXPECT_FALSE(Compression::Decompress(CompressionScheme::LZ4LW, corrupt.data(), corrupt.size(), out.data(), out.size(), err));

    
    EXPECT_FALSE(Compression::Decompress(CompressionScheme::LZ4LW, nullptr, 10, out.data(), out.size(), err));
    EXPECT_FALSE(Compression::Decompress(CompressionScheme::LZ4LW, corrupt.data(), corrupt.size(), nullptr, out.size(), err));
}

TEST_F(CompressionTest, LZO1xRoundTrip) {
    const std::vector<size_t> testSizes = {64, 1024, 32768};
    for (size_t size : testSizes) {
        auto original = GenerateTestData(size);
        std::vector<uint8_t> compressed;
        std::string err;

        bool compOk = Compression::Compress(CompressionScheme::LZO1x, original.data(), original.size(), compressed, err);
        ASSERT_TRUE(compOk) << "LZO1x compress failed: " << err;

        std::vector<uint8_t> decompressed(original.size());
        bool decompOk = Compression::Decompress(CompressionScheme::LZO1x, compressed.data(), compressed.size(), decompressed.data(), decompressed.size(), err);
        ASSERT_TRUE(decompOk) << "LZO1x decompress failed: " << err;

        EXPECT_EQ(original, decompressed);
    }
}

TEST_F(CompressionTest, XMemCompressRoundTrip) {
    const std::vector<size_t> testSizes = {64, 1024, 16384};
    for (size_t size : testSizes) {
        auto original = GenerateTestData(size);
        std::vector<uint8_t> compressed;
        std::string err;

        bool compOk = Compression::Compress(CompressionScheme::XMemCompress, original.data(), original.size(), compressed, err);
        ASSERT_TRUE(compOk) << "XMemCompress compress failed: " << err;

        std::vector<uint8_t> decompressed(original.size());
        bool decompOk = Compression::Decompress(CompressionScheme::XMemCompress, compressed.data(), compressed.size(), decompressed.data(), decompressed.size(), err);
        ASSERT_TRUE(decompOk) << "XMemCompress decompress failed: " << err;

        EXPECT_EQ(original, decompressed);
    }
}

TEST_F(CompressionTest, UnsupportedCompressionSchemesReportError) {
    auto original = GenerateTestData(64);
    std::vector<uint8_t> comp;
    std::string err;

    EXPECT_FALSE(Compression::Compress(CompressionScheme::Zlib, original.data(), original.size(), comp, err));
    EXPECT_FALSE(err.empty());

    err.clear();
    EXPECT_FALSE(Compression::Compress(CompressionScheme::LZMA, original.data(), original.size(), comp, err));
    EXPECT_FALSE(err.empty());
}

TEST_F(CompressionTest, NoneSchemeRoundTrip) {
    auto original = GenerateTestData(128);
    std::vector<uint8_t> compressed;
    std::string err;

    bool compOk = Compression::Compress(CompressionScheme::None, original.data(), original.size(), compressed, err);
    ASSERT_TRUE(compOk);
    EXPECT_EQ(original, compressed);

    std::vector<uint8_t> decompressed(original.size());
    bool decompOk = Compression::Decompress(CompressionScheme::None, compressed.data(), compressed.size(), decompressed.data(), decompressed.size(), err);
    ASSERT_TRUE(decompOk);
    EXPECT_EQ(original, decompressed);
}

} 
