find_library(LZO2_LIBRARY NAMES lzo2 lzo)
if(NOT LZO2_LIBRARY)
    message(STATUS "lzo2 library not found, fetching LZO...")
    include(FetchContent)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)
    set(ENABLE_STATIC ON CACHE BOOL "" FORCE)
    set(ENABLE_SHARED OFF CACHE BOOL "" FORCE)
    set(ENABLE_DOCS OFF CACHE BOOL "" FORCE)
    set(ENABLE_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
            lzo
            GIT_REPOSITORY https://github.com/nemequ/lzo.git
            GIT_TAG 0083878c235a89ef96a009d1ff0b500f3a364e4b
            GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(lzo)
    set(LZO2_LIBRARY lzo_static_lib)
    if(TARGET NomadArchives)
        target_include_directories(NomadArchives PRIVATE ${lzo_SOURCE_DIR}/include)
    endif()
endif()

find_package(ZLIB REQUIRED)
find_package(LibLZMA REQUIRED)
