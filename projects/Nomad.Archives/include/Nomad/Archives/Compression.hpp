//
// Created by troplo on 09/21/26.
//

#ifndef NOMADARCHIVES_COMPRESSION_H
#define NOMADARCHIVES_COMPRESSION_H

#include "Nomad/Archives/Types.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace Nomad::Archives {
    class Compression {
      public:
        static bool Decompress(CompressionScheme scheme, const uint8_t* src,
                               size_t srcSize, uint8_t* dst, size_t dstSize,
                               std::string& errorMsg);

        static bool Decompress(CompressionScheme scheme,
                               const std::vector<uint8_t>& compressedData,
                               std::vector<uint8_t>& uncompressedData,
                               size_t uncompressedSize, std::string& errorMsg);

        static bool Compress(CompressionScheme scheme, const uint8_t* src,
                             size_t srcSize,
                             std::vector<uint8_t>& compressedData,
                             std::string& errorMsg);
    };

} // namespace Nomad::Archives

#endif // NOMADARCHIVES_COMPRESSION_H
