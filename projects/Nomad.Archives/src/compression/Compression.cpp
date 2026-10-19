//
// Created by troplo on 09/21/26.
//

#include "Nomad/Archives/Compression.hpp"
#include "lzma/lzma.h"
#include "xcompress/lzx.h"
#include "xcompress/lzx_comp/lzx_compress_api.h"
#include <algorithm>
#include <cstring>
#include <format>
#include <iomanip>
#include <iostream>
#include <lzo/lzo1x.h>
#include <sstream>
#include <zlib.h>

namespace Nomad::Archives {
    static uint32_t ReadU32BE(const uint8_t* p) {
        return (static_cast<uint32_t>(p[0]) << 24) |
               (static_cast<uint32_t>(p[1]) << 16) |
               (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
    }

    static uint64_t ReadU64BE(const uint8_t* p) {
        return (static_cast<uint64_t>(ReadU32BE(p)) << 32) | ReadU32BE(p + 4);
    }

#if __DEBUG
    static std::string HexDump(const uint8_t* data, size_t size) {
        std::ostringstream oss;
        size_t len = std::min<size_t>(size, 16);
        for (size_t i = 0; i < len; ++i) {
            oss << std::format("{:02X} ", data[i]);
        }
        return oss.str();
    }
#endif

    static bool DecompressXMemCompress(const uint8_t* srcBuf, size_t srcLen,
                                       uint8_t* dstBuf, size_t dstLen,
                                       std::string& errorMsg) {
        if (!srcBuf || !dstBuf) {
            errorMsg = "XMemCompress: null buffer pointer provided";
            return false;
        }

        if (srcLen < 48) {
            errorMsg = std::format(
                "XMemCompress: stream size ({}) is smaller than 48-byte header",
                srcLen);
            return false;
        }

        uint32_t magic = ReadU32BE(srcBuf + 0x00);
        uint32_t version = ReadU32BE(srcBuf + 0x04);
        uint32_t windowSize = ReadU32BE(srcBuf + 0x10);
        uint64_t uncompressedSize = ReadU64BE(srcBuf + 0x18);

        if (magic != 0x0FF512EE) {
            errorMsg = std::format(
                "XMemCompress: invalid magic 0x{:08X} (expected 0x0FF512EE)",
                magic);
            return false;
        }

        if (version != 0x01030000) {
            errorMsg = std::format(
                "XMemCompress: invalid version 0x{:08X} (expected 0x01030000)",
                version);
            return false;
        }

        int windowBits = 16;
        if (windowSize > 0) {
            int bits = 0;
            uint32_t temp = windowSize;
            while (temp > 1) {
                temp >>= 1;
                bits++;
            }
            if (bits >= 15 && bits <= 21) {
                windowBits = bits;
            }
        }

        struct lzx_state* strm = lzx_init(windowBits);
        if (!strm) {
            errorMsg = std::format("XMemCompress: failed to initialize LZX "
                                   "decoder with window bits {}",
                                   windowBits);
            return false;
        }

        size_t srcPos = 48; // Skip 48-byte header
        size_t dstPos = 0;
        uint64_t remaining = uncompressedSize;
        int chunkIdx = 0;

        while (remaining > 0 && srcPos < srcLen) {
            chunkIdx++;
            if (srcPos + 4 > srcLen) {
                errorMsg = std::format(
                    "[Chunk {}] Unexpected end of stream reading chunk size",
                    chunkIdx);
                lzx_teardown(strm);
                return false;
            }

            uint32_t compressedChunkSize = ReadU32BE(srcBuf + srcPos);
            srcPos += 4;

            if (compressedChunkSize == 0 ||
                srcPos + compressedChunkSize > srcLen) {
                errorMsg =
                    std::format("[Chunk {}] Invalid compressed chunk size ({}) "
                                "at offset 0x{:X}",
                                chunkIdx, compressedChunkSize, srcPos - 4);
                lzx_teardown(strm);
                return false;
            }

            const uint8_t* chunkBuf = srcBuf + srcPos;
            srcPos += compressedChunkSize;

            uint32_t blockDstSize = 0;
            uint32_t blockSrcSize = 0;
            size_t blockHeaderLen = 0;

            uint8_t hi = chunkBuf[0];
            if (hi == 0xFF) {
                if (compressedChunkSize < 5) {
                    errorMsg = std::format("[Chunk {}] Chunk size too small "
                                           "for 5-byte block header",
                                           chunkIdx);
                    lzx_teardown(strm);
                    return false;
                }
                uint8_t hiDst = chunkBuf[1];
                uint8_t loDst = chunkBuf[2];
                blockDstSize = (static_cast<uint32_t>(hiDst) << 8) | loDst;

                uint8_t hiSrc = chunkBuf[3];
                uint8_t loSrc = chunkBuf[4];
                blockSrcSize = (static_cast<uint32_t>(hiSrc) << 8) | loSrc;
                blockHeaderLen = 5;
            } else {
                if (compressedChunkSize < 2) {
                    errorMsg = std::format("[Chunk {}] Chunk size too small "
                                           "for 2-byte block header",
                                           chunkIdx);
                    lzx_teardown(strm);
                    return false;
                }
                blockDstSize = 0x8000;
                uint8_t loSrc = chunkBuf[1];
                blockSrcSize = (static_cast<uint32_t>(hi) << 8) | loSrc;
                blockHeaderLen = 2;
            }

            if (blockSrcSize == 0 || blockDstSize == 0) {
                break;
            }

            if (dstPos + blockDstSize > dstLen) {
                blockDstSize = static_cast<uint32_t>(dstLen - dstPos);
            }

            const uint8_t* blockData = chunkBuf + blockHeaderLen;
            size_t actualBlockDataLen = blockSrcSize;
            if (blockHeaderLen + actualBlockDataLen > compressedChunkSize) {
                errorMsg = std::format("[Chunk {}] Invalid block size (header "
                                       "{} + src {} > chunk {})",
                                       chunkIdx, blockHeaderLen, blockSrcSize,
                                       compressedChunkSize);
                lzx_teardown(strm);
                return false;
            }

            lzx_reset(strm);
            int lzxRes = lzx_decompress(
                strm, const_cast<unsigned char*>(blockData), dstBuf + dstPos,
                actualBlockDataLen, blockDstSize);

            if (lzxRes != 0) {
                if (actualBlockDataLen == blockDstSize ||
                    blockSrcSize == blockDstSize) {
                    std::memcpy(dstBuf + dstPos, blockData, blockDstSize);
                    dstPos += blockDstSize;
                    remaining = (remaining > blockDstSize)
                                    ? (remaining - blockDstSize)
                                    : 0;
                    continue;
                }
                errorMsg = std::format(
                    "[Chunk {}] LZX decompression failed with code {}",
                    chunkIdx, lzxRes);
                lzx_teardown(strm);
                return false;
            }

            dstPos += blockDstSize;
            if (remaining > blockDstSize) {
                remaining -= blockDstSize;
            } else {
                remaining = 0;
            }
        }

        lzx_teardown(strm);

        if (dstPos != dstLen && remaining != 0) {
            errorMsg = std::format("XMemCompress: decompressed size mismatch "
                                   "(expected {}, got {})",
                                   dstLen, dstPos);
            return false;
        }

        return true;
    }

    static bool DecompressLZO1x(const uint8_t* src, size_t srcSize,
                                uint8_t* dst, size_t dstSize,
                                std::string& errorMsg) {
        static bool lzoInitialized = false;
        if (!lzoInitialized) {
            if (lzo_init() != LZO_E_OK) {
                errorMsg = "LZO1x: failed to initialize lzo library";
                return false;
            }
            lzoInitialized = true;
        }

        lzo_uint uncompressedLen = static_cast<lzo_uint>(dstSize);
        int res = lzo1x_decompress_safe(src, static_cast<lzo_uint>(srcSize),
                                        dst, &uncompressedLen, nullptr);
        if (res != LZO_E_OK) {
            errorMsg = std::format(
                "LZO1x: decompression failed with error code {}", res);
            return false;
        }
        if (uncompressedLen != dstSize) {
            errorMsg = std::format(
                "LZO1x: decompressed size mismatch (expected {}, got {})",
                dstSize, uncompressedLen);
            return false;
        }
        return true;
    }

    static bool DecompressZlib(const uint8_t* src, size_t srcSize, uint8_t* dst,
                               size_t dstSize, std::string& errorMsg) {
        if (srcSize < 16) {
            errorMsg = "Zlib: compressed payload too small (< 16 bytes header)";
            return false;
        }

        size_t srcPos = 0;
        uint16_t sizes[8];
        for (int i = 0; i < 8; i++) {
            sizes[i] = static_cast<uint16_t>(src[srcPos]) |
                       (static_cast<uint16_t>(src[srcPos + 1]) << 8);
            srcPos += 2;
        }

        uint16_t blockCount = sizes[0];
        uint32_t maximumUncompressedBlockSize =
            16 * (static_cast<uint32_t>(sizes[1]) + 1);

        size_t dstPos = 0;
        size_t left = dstSize;

        for (uint16_t i = 0, c = 2; i < blockCount; i++, c++) {
            if (c == 8) {
                if (srcPos + 16 > srcSize) {
                    errorMsg = "Zlib: unexpected end of stream reading block "
                               "header sizes";
                    return false;
                }
                for (int j = 0; j < 8; j++) {
                    sizes[j] = static_cast<uint16_t>(src[srcPos]) |
                               (static_cast<uint16_t>(src[srcPos + 1]) << 8);
                    srcPos += 2;
                }
                c = 0;
            }

            uint32_t compressedBlockSize = sizes[c];
            if (compressedBlockSize != 0) {
                uint32_t uncompressedBlockSize =
                    (i + 1 < blockCount)
                        ? static_cast<uint32_t>(std::min<size_t>(
                              maximumUncompressedBlockSize, left))
                        : static_cast<uint32_t>(left);

                if (srcPos + compressedBlockSize > srcSize) {
                    errorMsg = std::format("Zlib: compressed block size ({}) "
                                           "exceeds stream length",
                                           compressedBlockSize);
                    return false;
                }

                z_stream strm{};
                strm.next_in = const_cast<Bytef*>(src + srcPos);
                strm.avail_in = compressedBlockSize;
                strm.next_out = dst + dstPos;
                strm.avail_out = uncompressedBlockSize;

                if (inflateInit2(&strm, -MAX_WBITS) != Z_OK) {
                    errorMsg = "Zlib: inflateInit2 failed";
                    return false;
                }

                int res = inflate(&strm, Z_FINISH);
                inflateEnd(&strm);

                if (res != Z_STREAM_END && res != Z_OK) {
                    errorMsg = std::format(
                        "Zlib: inflate failed with error code {}", res);
                    return false;
                }

                srcPos += compressedBlockSize;
                dstPos += uncompressedBlockSize;
                left -= uncompressedBlockSize;

                uint32_t padding = (16 - (compressedBlockSize % 16)) % 16;
                srcPos += padding;
            } else {
                errorMsg = "Zlib: compressed block size is 0";
                return false;
            }
        }

        return dstPos == dstSize;
    }

    static size_t ParseVarintHeaderOffset(const uint8_t* src, size_t srcSize,
                                          uint32_t& outCompSize) {
        if (srcSize == 0)
            return 0;
        uint32_t v7 = src[0];
        if ((v7 & 0x80u) != 0) {
            if (srcSize < 2)
                return 0;
            v7 = v7 + (static_cast<uint32_t>(src[1]) << 7) - 128;
            if (v7 < 0x4000) {
                outCompSize = v7;
                return 2;
            } else {
                if (srcSize < 3)
                    return 0;
                v7 = v7 + (static_cast<uint32_t>(src[2]) << 14) - 0x4000;
                if (v7 < 0x200000) {
                    outCompSize = v7;
                    return 3;
                } else {
                    if (srcSize < 4)
                        return 0;
                    v7 = v7 + (static_cast<uint32_t>(src[3]) << 21) - 0x200000;
                    outCompSize = v7;
                    return 4;
                }
            }
        } else {
            outCompSize = v7;
            return 1;
        }
    }

    static bool DecompressLZ4LW(const uint8_t* src, size_t srcSize,
                                uint8_t* dst, size_t dstSize,
                                std::string& errorMsg) {
        if (!src || !dst || srcSize < 2) {
            errorMsg = "LZ4LW: invalid buffer or size";
            return false;
        }

        uint32_t tailMargin = 0;
        size_t headerLen = ParseVarintHeaderOffset(src, srcSize, tailMargin);
        if (headerLen == 0 || tailMargin > dstSize) {
            errorMsg = "LZ4LW: corrupted varint header";
            return false;
        }

        const uint8_t* ip = src + headerLen;
        const uint8_t* const iend = src + srcSize;
        uint8_t* op = dst;
        uint8_t* const oend = dst + dstSize;

        const size_t inputShift =
            (dstSize >= srcSize) ? (dstSize - srcSize) : 0;

        auto checkInPlaceTermination = [&]() -> bool {
            if (ip >= iend)
                return true;
            if (tailMargin > 0 &&
                static_cast<size_t>(oend - op) <= tailMargin) {
                size_t currentOutPos = static_cast<size_t>(op - dst);
                size_t currentInPos =
                    inputShift + static_cast<size_t>(ip - src);
                return currentOutPos >= currentInPos;
            }
            return false;
        };

        while (op < oend) {
            if (checkInPlaceTermination()) {
                break;
            }

            uint8_t token = *ip++;

            size_t litLen = token >> 4;
            if (litLen == 15) {
                uint8_t s = 0;
                do {
                    if (ip >= iend) {
                        errorMsg =
                            "LZ4LW: unexpected EOF reading literal length";
                        return false;
                    }
                    s = *ip++;
                    litLen += s;
                } while (s == 0xFF);
            }

            if (litLen > 0) {
                if (ip + litLen > iend || op + litLen > oend) {
                    errorMsg = "LZ4LW: literal length out of bounds";
                    return false;
                }
                std::memcpy(op, ip, litLen);
                ip += litLen;
                op += litLen;
            }

            if (checkInPlaceTermination()) {
                break;
            }

            if (op >= oend)
                break;

            if (ip + 2 > iend) {
                errorMsg = "LZ4LW: unexpected EOF reading offset";
                return false;
            }

            uint32_t offset = static_cast<uint32_t>(ip[0]) |
                              (static_cast<uint32_t>(ip[1]) << 8);
            ip += 2;

            if (offset >= 0xE000) {
                if (ip >= iend) {
                    errorMsg = "LZ4LW: unexpected EOF reading extended offset";
                    return false;
                }
                uint32_t extra = *ip++;
                offset += extra * 0x2000;
            }

            if (offset == 0 || static_cast<size_t>(op - dst) < offset) {
                errorMsg = "LZ4LW: match offset out of bounds";
                return false;
            }

            size_t matchLen = (token & 0x0F) + 4;
            if (matchLen == 19) {
                uint8_t s = 0;
                do {
                    if (ip >= iend) {
                        errorMsg = "LZ4LW: unexpected EOF reading match length";
                        return false;
                    }
                    s = *ip++;
                    matchLen += s;
                } while (s == 0xFF);
            }

            if (op + matchLen > oend) {
                errorMsg = "LZ4LW: match length out of bounds";
                return false;
            }

            const uint8_t* matchPtr = op - offset;
            for (size_t i = 0; i < matchLen; ++i) {
                *op++ = *matchPtr++;
            }
        }

        if (op < oend) {
            size_t remaining = static_cast<size_t>(oend - op);
            if (srcSize < remaining) {
                errorMsg =
                    "LZ4LW: remaining output exceeds compressed input size for "
                    "tail copy";
                return false;
            }
            std::memcpy(op, src + (srcSize - remaining), remaining);
            op += remaining;
        }

        if (op != oend) {
            errorMsg = std::format(
                "LZ4LW: decompressed size mismatch (expected {}, got {})",
                dstSize, static_cast<size_t>(op - dst));
            return false;
        }

        return true;
    }

    static void* SzAlloc(void* p, size_t size) {
        (void)p;
        return std::malloc(size);
    }

    static void SzFree(void* p, void* address) {
        (void)p;
        std::free(address);
    }

    static ISzAlloc g_LzmaAlloc = {SzAlloc, SzFree};

    static bool DecompressLZMA(const uint8_t* src, size_t srcSize, uint8_t* dst,
                               size_t dstSize, std::string& errorMsg) {
        if (!src || !dst) {
            errorMsg = "LZMA: null buffer pointer provided";
            return false;
        }
        if (srcSize < 6) {
            errorMsg = std::format(
                "LZMA: stream size ({}) is smaller than 6 bytes minimum",
                srcSize);
            return false;
        }

        uint32_t varintVal = 0;
        size_t varintLen = ParseVarintHeaderOffset(src, srcSize, varintVal);
        if (varintLen == 0) {
            errorMsg = "LZMA: invalid varint header";
            return false;
        }

        if (srcSize < varintLen + 5 + varintVal) {
            errorMsg = "LZMA: stream size too small for varint header, LZMA "
                       "properties, and uncompressed tail";
            return false;
        }

        if (dstSize < varintVal) {
            errorMsg = std::format("LZMA: destination size ({}) smaller than "
                                   "varint tail size ({})",
                                   dstSize, varintVal);
            return false;
        }

        const uint8_t* lzmaProps = src + varintLen;
        const uint8_t* lzmaPayload = src + varintLen + 5;
        SizeT realPayloadLen = srcSize - varintLen - 5 - varintVal;
        SizeT outSize = dstSize - varintVal;

        ELzmaStatus status = LZMA_STATUS_NOT_SPECIFIED;
        SRes res =
            LzmaDecode(dst, &outSize, lzmaPayload, &realPayloadLen, lzmaProps,
                       5, LZMA_FINISH_ANY, &status, &g_LzmaAlloc);

        if (res != SZ_OK) {
            errorMsg = std::format(
                "LZMA: LzmaDecode failed with error code {} (status {})", res,
                static_cast<int>(status));
            return false;
        }

        if (outSize != dstSize - varintVal) {
            errorMsg = std::format(
                "LZMA: decompressed size mismatch (expected {}, got {})",
                dstSize - varintVal, outSize);
            return false;
        }

        if (varintVal > 0) {
            std::memcpy(dst + outSize, src + srcSize - varintVal, varintVal);
        }

        return true;
    }

    bool Compression::Decompress(CompressionScheme scheme, const uint8_t* src,
                                 size_t srcSize, uint8_t* dst, size_t dstSize,
                                 std::string& errorMsg) {
        errorMsg.clear();
        switch (scheme) {
        case CompressionScheme::None:
            if (srcSize > dstSize) {
                errorMsg = std::format("None: source size ({}) exceeds "
                                       "destination buffer size ({})",
                                       srcSize, dstSize);
                return false;
            }
            std::memcpy(dst, src, srcSize);
            return true;

        case CompressionScheme::XMemCompress:
            return DecompressXMemCompress(src, srcSize, dst, dstSize, errorMsg);

        case CompressionScheme::LZO1x:
            return DecompressLZO1x(src, srcSize, dst, dstSize, errorMsg);

        case CompressionScheme::Zlib:
            return DecompressZlib(src, srcSize, dst, dstSize, errorMsg);

        case CompressionScheme::LZMA:
            return DecompressLZMA(src, srcSize, dst, dstSize, errorMsg);

        case CompressionScheme::LZ4LW:
            return DecompressLZ4LW(src, srcSize, dst, dstSize, errorMsg);

        default:
            errorMsg =
                std::format("Decompression for scheme '{}' is unsupported, {}",
                            CompressionToString(scheme), (int)scheme);
            return false;
        }
    }

    bool Compression::Decompress(CompressionScheme scheme,
                                 const std::vector<uint8_t>& compressedData,
                                 std::vector<uint8_t>& uncompressedData,
                                 size_t uncompressedSize,
                                 std::string& errorMsg) {
        uncompressedData.resize(uncompressedSize);
        return Decompress(scheme, compressedData.data(), compressedData.size(),
                          uncompressedData.data(), uncompressedSize, errorMsg);
    }

    static bool CompressLZ4LW(const uint8_t* src, size_t srcSize,
                              std::vector<uint8_t>& dst,
                              std::string& errorMsg) {
        if (srcSize <= 31) {
            errorMsg = "LZ4LW: file size <= 31 bytes, compression skipped";
            return false;
        }

        dst.clear();
        dst.push_back(0x00);

        constexpr uint32_t MAX_OFFSET = 0xDFFF;
        constexpr size_t HASH_LOG = 14;
        constexpr size_t HASH_SIZE = 1 << HASH_LOG;
        std::vector<int32_t> hashTable(HASH_SIZE, -1);

        auto Hash4 = [](const uint8_t* p) -> uint32_t {
            uint32_t val = static_cast<uint32_t>(p[0]) |
                           (static_cast<uint32_t>(p[1]) << 8) |
                           (static_cast<uint32_t>(p[2]) << 16) |
                           (static_cast<uint32_t>(p[3]) << 24);
            return (val * 2654435761U) >> (32 - HASH_LOG);
        };

        const uint8_t* ip = src;
        const uint8_t* anchor = src;
        const uint8_t* const iend = src + srcSize;
        const uint8_t* const mflimit = iend - 5;

        while (ip < mflimit) {
            uint32_t h = Hash4(ip);
            int32_t refPos = hashTable[h];
            hashTable[h] = static_cast<int32_t>(ip - src);

            if (refPos >= 0) {
                uint32_t offset = static_cast<uint32_t>((ip - src) - refPos);
                if (offset > 0 && offset <= MAX_OFFSET) {
                    const uint8_t* ref = src + refPos;
                    if (ref[0] == ip[0] && ref[1] == ip[1] && ref[2] == ip[2] &&
                        ref[3] == ip[3]) {
                        size_t matchLen = 4;
                        while (ip + matchLen < iend &&
                               ref[matchLen] == ip[matchLen]) {
                            matchLen++;
                        }

                        size_t litLen = ip - anchor;
                        uint8_t tokenLit =
                            (litLen >= 15) ? 15 : static_cast<uint8_t>(litLen);
                        size_t matchExcess = matchLen - 4;
                        uint8_t tokenMatch =
                            (matchExcess >= 15)
                                ? 15
                                : static_cast<uint8_t>(matchExcess);

                        dst.push_back((tokenLit << 4) | tokenMatch);

                        if (litLen >= 15) {
                            size_t s = litLen - 15;
                            while (s >= 255) {
                                dst.push_back(255);
                                s -= 255;
                            }
                            dst.push_back(static_cast<uint8_t>(s));
                        }

                        dst.insert(dst.end(), anchor, anchor + litLen);

                        dst.push_back(static_cast<uint8_t>(offset & 0xFF));
                        dst.push_back(
                            static_cast<uint8_t>((offset >> 8) & 0xFF));

                        if (matchExcess >= 15) {
                            size_t s = matchExcess - 15;
                            while (s >= 255) {
                                dst.push_back(255);
                                s -= 255;
                            }
                            dst.push_back(static_cast<uint8_t>(s));
                        }

                        ip += matchLen;
                        anchor = ip;
                        continue;
                    }
                }
            }
            ip++;
        }

        size_t lastLitLen = iend - anchor;
        uint8_t tokenLit =
            (lastLitLen >= 15) ? 15 : static_cast<uint8_t>(lastLitLen);
        dst.push_back(tokenLit << 4);
        if (lastLitLen >= 15) {
            size_t s = lastLitLen - 15;
            while (s >= 255) {
                dst.push_back(255);
                s -= 255;
            }
            dst.push_back(static_cast<uint8_t>(s));
        }
        dst.insert(dst.end(), anchor, anchor + lastLitLen);
        return true;
    }

    static bool CompressLZO1x(const uint8_t* src, size_t srcSize,
                              std::vector<uint8_t>& dst,
                              std::string& errorMsg) {
        if (srcSize <= 31) {
            errorMsg = "LZO1x: file size <= 31 bytes, compression skipped";
            return false;
        }
        static bool lzoInitialized = false;
        if (!lzoInitialized) {
            if (lzo_init() != LZO_E_OK) {
                errorMsg = "LZO1x: failed to initialize lzo library";
                return false;
            }
            lzoInitialized = true;
        }

        lzo_uint outLen = srcSize + srcSize / 16 + 64 + 3;
        dst.resize(outLen);
        std::vector<uint8_t> wrkMem(LZO1X_1_MEM_COMPRESS);

        int res = lzo1x_1_compress(src, static_cast<lzo_uint>(srcSize),
                                   dst.data(), &outLen, wrkMem.data());
        if (res != LZO_E_OK) {
            errorMsg =
                std::format("LZO1x: compression failed with code {}", res);
            return false;
        }
        dst.resize(outLen);
        return true;
    }

    static void PutWordLE(std::vector<uint8_t>& buf, uint16_t w) {
        buf.push_back(static_cast<uint8_t>(w & 0xFF));
        buf.push_back(static_cast<uint8_t>((w >> 8) & 0xFF));
    }

    static void PutU32BE(std::vector<uint8_t>& buf, uint32_t val) {
        buf.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
        buf.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
        buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
        buf.push_back(static_cast<uint8_t>(val & 0xFF));
    }

    static void PutU64BE(std::vector<uint8_t>& buf, uint64_t val) {
        PutU32BE(buf, static_cast<uint32_t>(val >> 32));
        PutU32BE(buf, static_cast<uint32_t>(val & 0xFFFFFFFF));
    }

    static bool CompressXMemCompress(const uint8_t* src, size_t srcSize,
                                     std::vector<uint8_t>& dst,
                                     std::string& errorMsg) {
        if (srcSize <= 31) {
            errorMsg =
                "XMemCompress: file size <= 31 bytes, compression skipped";
            return false;
        }

        constexpr size_t CHUNK_SIZE = 32768; // 32KB
        dst.clear();

        void* compressor = nullptr;
        if (lzx_compressor_ops.create_compressor(CHUNK_SIZE, 50, false,
                                                 &compressor) != 0) {
            errorMsg = "XMemCompress: failed to create LZX compressor";
            return false;
        }

        PutU32BE(dst, 0x0FF512EE); // magic
        PutU32BE(dst, 0x01030000); // version
        PutU32BE(dst, 0);          // flags
        PutU32BE(dst, 0);          // idk
        PutU32BE(dst, 0x00008000); // window size (32KB)
        PutU32BE(dst, 0x00008000); // chunk size (32KB)
        PutU64BE(dst, srcSize);    // uncompressedSize
        PutU64BE(
            dst,
            0); // sum of all chunk compressed sizes placeholder (offset 0x20)
        PutU32BE(dst, 0x00008000); // largest uncompressed chunk
        PutU32BE(dst,
                 0); // largest chunk compressed size placeholder (offset 0x2C)

        uint32_t maxCompChunk = 0;
        uint64_t totalChunkBytes = 0;
        size_t offset = 0;
        std::vector<uint8_t> chunkCompBuf(CHUNK_SIZE + 1024);

        while (offset < srcSize) {
            size_t thisChunkUncomp =
                std::min<size_t>(CHUNK_SIZE, srcSize - offset);
            size_t lzxSize = lzx_compressor_ops.compress(
                src + offset, thisChunkUncomp, chunkCompBuf.data(),
                chunkCompBuf.size(), compressor);

            if (lzxSize == 0) {
                lzx_compressor_ops.free_compressor(compressor);
                errorMsg = "XMemCompress: LZX compression failed";
                return false;
            }

            uint32_t compChunkSize = static_cast<uint32_t>(lzxSize + 10);
            maxCompChunk = std::max(maxCompChunk, compChunkSize);
            totalChunkBytes += compChunkSize;

            PutU32BE(dst, compChunkSize);
            dst.push_back(0xFF);
            dst.push_back(static_cast<uint8_t>((thisChunkUncomp >> 8) & 0xFF));
            dst.push_back(static_cast<uint8_t>(thisChunkUncomp & 0xFF));
            dst.push_back(static_cast<uint8_t>((lzxSize >> 8) & 0xFF));
            dst.push_back(static_cast<uint8_t>(lzxSize & 0xFF));
            dst.insert(dst.end(), chunkCompBuf.begin(),
                       chunkCompBuf.begin() + lzxSize);
            for (int p = 0; p < 5; ++p) {
                dst.push_back(0);
            }

            offset += thisChunkUncomp;
        }

        lzx_compressor_ops.free_compressor(compressor);

        // Sum of all compressed chunk sizes at offset 0x20
        dst[0x20] = static_cast<uint8_t>((totalChunkBytes >> 56) & 0xFF);
        dst[0x21] = static_cast<uint8_t>((totalChunkBytes >> 48) & 0xFF);
        dst[0x22] = static_cast<uint8_t>((totalChunkBytes >> 40) & 0xFF);
        dst[0x23] = static_cast<uint8_t>((totalChunkBytes >> 32) & 0xFF);
        dst[0x24] = static_cast<uint8_t>((totalChunkBytes >> 24) & 0xFF);
        dst[0x25] = static_cast<uint8_t>((totalChunkBytes >> 16) & 0xFF);
        dst[0x26] = static_cast<uint8_t>((totalChunkBytes >> 8) & 0xFF);
        dst[0x27] = static_cast<uint8_t>(totalChunkBytes & 0xFF);

        // Max compressed chunk size at offset 0x2C
        dst[0x2C] = static_cast<uint8_t>((maxCompChunk >> 24) & 0xFF);
        dst[0x2D] = static_cast<uint8_t>((maxCompChunk >> 16) & 0xFF);
        dst[0x2E] = static_cast<uint8_t>((maxCompChunk >> 8) & 0xFF);
        dst[0x2F] = static_cast<uint8_t>(maxCompChunk & 0xFF);

        return true;
    }

    bool Compression::Compress(CompressionScheme scheme, const uint8_t* src,
                               size_t srcSize,
                               std::vector<uint8_t>& compressedData,
                               std::string& errorMsg) {
        errorMsg.clear();
        if (srcSize <= 31) {
            compressedData.assign(src, src + srcSize);
            return true;
        }

        switch (scheme) {
        case CompressionScheme::None:
            compressedData.assign(src, src + srcSize);
            return true;

        case CompressionScheme::XMemCompress:
            return CompressXMemCompress(src, srcSize, compressedData, errorMsg);

        case CompressionScheme::LZ4LW:
            return CompressLZ4LW(src, srcSize, compressedData, errorMsg);

        case CompressionScheme::LZO1x:
            return CompressLZO1x(src, srcSize, compressedData, errorMsg);

        default:
            errorMsg = std::format(
                "Compression for scheme index {} is not supported yet",
                std::to_string((int)scheme));
            return false;
        }
    }
} // namespace Nomad::Archives
