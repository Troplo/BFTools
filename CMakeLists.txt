cmake_minimum_required(VERSION 3.25)
project(BFRoot LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_BFVIEW "Build BFView GUI application" ON)
option(BUILD_STATIC "Statically link C/C++ runtime and dependencies on Windows" ON)

if(WIN32 AND BUILD_STATIC)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libraries" FORCE)
    # Prefer static libraries (.a) over import libraries (.dll.a)
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a" ".lib")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_link_options(-static -static-libgcc -static-libstdc++ -pthread)
    endif()
endif()

include(cmake/tracy.cmake)

add_subdirectory(projects/Nomad.Archives)
add_subdirectory(projects/BFTools)

if(BUILD_BFVIEW)
    find_package(Qt6 COMPONENTS Core Gui Widgets QUIET)
    if(Qt6_FOUND)
        add_subdirectory(projects/BFView)
    else()
        message(STATUS "Qt6 not found; skipping BFView GUI application")
    endif()
endif()

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
