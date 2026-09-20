find_package(GTest QUIET)
if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

add_executable(NomadArchivesTests
    test_extractor.cpp
    test_packer.cpp
    test_compression.cpp
    test_archive_formats.cpp
    test_archive_modifier.cpp
    test_defragmenter.cpp
    test_directory_tree.cpp
)

target_compile_definitions(NomadArchivesTests PRIVATE
    TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
)

target_link_libraries(NomadArchivesTests PRIVATE
    Nomad::Archives
    GTest::gtest
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(NomadArchivesTests DISCOVERY_TIMEOUT 30)
