//
// Created by troplo on 09/21/26.
//

#ifndef NOMADARCHIVES_LZX_COMPRESS_API_H
#define NOMADARCHIVES_LZX_COMPRESS_API_H

#include <cstddef>
#include <cstdint>

extern "C" {
struct compressor_ops {
    uint64_t (*get_needed_memory)(size_t max_block_size,
                                  unsigned int compression_level,
                                  bool destructive);
    int (*create_compressor)(size_t max_block_size,
                             unsigned int compression_level, bool destructive,
                             void** private_ret);
    size_t (*compress)(const void* uncompressed_data, size_t uncompressed_size,
                       void* compressed_data, size_t compressed_size_avail,
                       void* priv);
    void (*free_compressor)(void* priv);
};
extern const struct compressor_ops lzx_compressor_ops;
}

#endif // NOMADARCHIVES_LZX_COMPRESS_API_H
