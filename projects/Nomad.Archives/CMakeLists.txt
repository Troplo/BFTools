cmake_minimum_required(VERSION 3.25)
project(NomadArchives LANGUAGES C CXX VERSION 1.0.0)

include(../../cmake/git.cmake)
include(../../cmake/json.cmake)
include(../../cmake/lzo2.cmake)
include(../../cmake/pugixml.cmake)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(BUILD_TYPE_INT 1)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    set(BUILD_TYPE_INT 2)
else()
    set(BUILD_TYPE_INT 0)
endif()

string(TIMESTAMP PROJECT_BUILD_DATE "%Y-%m-%d %H:%M:%S")

add_library(NomadArchives
    src/Types.cpp
    src/FATv2Archive.cpp
    src/FATv3Archive.cpp
    src/FATv5Archive.cpp
    src/DirectoryTree.cpp
    src/ArchiveFactory.cpp
    src/Extractor.cpp
    src/Defragmenter.cpp
    src/ArchiveModifier.cpp
    src/ProjectConfig.cpp
    src/ProjectManager.cpp
    src/MemoryManager.cpp
    src/compression/xcompress/lzx.cpp
    src/compression/xcompress/lzx_comp/lzx_compress.c
    src/compression/xcompress/lzx_comp/lzx_common.c
    src/compression/xcompress/lzx_comp/compress_common.c
    src/compression/lzma/lzma.cpp
    src/compression/Compression.cpp
    src/features.h
    src/Errors.h
    include/NomadArchives.hpp
    include/Nomad/Archives/HashingUtils.hpp
    src/NomadArchives.cpp
)

add_library(Nomad::Archives ALIAS NomadArchives)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_options(NomadArchives INTERFACE
        "-Wl,--wrap=malloc"
        "-Wl,--wrap=free"
        "-Wl,--wrap=calloc"
        "-Wl,--wrap=realloc"
    )
endif()

target_include_directories(NomadArchives
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/compression/xcompress/lzx_comp
        "${CMAKE_CURRENT_BINARY_DIR}/src"
        $<$<BOOL:${lzo_SOURCE_DIR}>:${lzo_SOURCE_DIR}/include>
)

set_source_files_properties(
    src/compression/xcompress/lzx_comp/lzx_compress.c
    src/compression/xcompress/lzx_comp/lzx_common.c
    src/compression/xcompress/lzx_comp/compress_common.c
    PROPERTIES
        SKIP_PRECOMPILE_HEADERS ON
)

target_link_libraries(NomadArchives
    PUBLIC
        ZLIB::ZLIB
        LibLZMA::LibLZMA
        ${LZO2_LIBRARY}
        nlohmann_json::nlohmann_json
        ${PUGIXML_TARGET}
        tracy
)

target_compile_definitions(NomadArchives PUBLIC
    BF_DEFAULT_PROJECTS_DIR=\"${CMAKE_SOURCE_DIR}/release/common/projects\"
)

target_compile_features(NomadArchives PUBLIC cxx_std_23)

configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/src/version.h.in"
        "${CMAKE_CURRENT_BINARY_DIR}/src/version.h"
        @ONLY
)

target_precompile_headers(NomadArchives PRIVATE src/features.h src/Errors.h)
