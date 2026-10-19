# Toolchain file for cross-compiling to Windows 64-bit using MinGW-w64 and Clang
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(TOOLCHAIN_PREFIX "x86_64-w64-mingw32")

set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

set(CMAKE_RC_COMPILER ${TOOLCHAIN_PREFIX}-windres)

set(SYSROOT_CANDIDATES
    "/usr/${TOOLCHAIN_PREFIX}"
    "/usr/lib/gcc/${TOOLCHAIN_PREFIX}"
)

file(GLOB QT_TARGET_SEARCH_PATHS
    "$ENV{HOME}/Qt/*/*mingw*"
    "/opt/Qt/*/*mingw*"
    "/opt/qt*/*mingw*"
)
foreach(candidate ${QT_TARGET_SEARCH_PATHS})
    if(IS_DIRECTORY "${candidate}" AND EXISTS "${candidate}/lib/cmake/Qt6")
        list(APPEND SYSROOT_CANDIDATES "${candidate}")
        list(APPEND CMAKE_PREFIX_PATH "${candidate}")
    endif()
endforeach()

if(NOT CMAKE_C_COMPILER MATCHES "clang")
    set(CMAKE_C_FLAGS_INIT "-mcrtdll=msvcrt-os -D__LARGE_MBSTATE_T")
    set(CMAKE_CXX_FLAGS_INIT "-mcrtdll=msvcrt-os -D__LARGE_MBSTATE_T -include ${CMAKE_CURRENT_LIST_DIR}/msvcrt_compat.h")
    set(CMAKE_EXE_LINKER_FLAGS_INIT "-mcrtdll=msvcrt-os -Wl,--whole-archive -lmingwex -Wl,--no-whole-archive")
    set(CMAKE_SHARED_LINKER_FLAGS_INIT "-mcrtdll=msvcrt-os -Wl,--whole-archive -lmingwex -Wl,--no-whole-archive")
else()
    set(CMAKE_C_FLAGS_INIT "-D__LARGE_MBSTATE_T")
    set(CMAKE_CXX_FLAGS_INIT "-D__LARGE_MBSTATE_T")
    set(CMAKE_EXE_LINKER_FLAGS_INIT "-pthread -Wl,--whole-archive -lmingwex -Wl,--no-whole-archive -lmsvcrt-os")
    set(CMAKE_SHARED_LINKER_FLAGS_INIT "-pthread -Wl,--whole-archive -lmingwex -Wl,--no-whole-archive -lmsvcrt-os")
endif()

if(NOT DEFINED QT_HOST_PATH)
    file(GLOB QT_HOST_SEARCH_PATHS
        "$ENV{HOME}/Qt/*/*gcc*"
        "/opt/Qt/*/*gcc*"
        "/opt/qt*/*gcc*"
    )
    foreach(candidate ${QT_HOST_SEARCH_PATHS})
        if(IS_DIRECTORY "${candidate}" AND (EXISTS "${candidate}/libexec/moc" OR EXISTS "${candidate}/bin/moc"))
            set(QT_HOST_PATH "${candidate}" CACHE PATH "Qt host tools path" FORCE)
            break()
        endif()
    endforeach()
endif()

if(DEFINED QT_HOST_PATH AND EXISTS "${QT_HOST_PATH}")
    if(EXISTS "${QT_HOST_PATH}/lib/cmake/Qt6CoreTools")
        set(Qt6CoreTools_DIR "${QT_HOST_PATH}/lib/cmake/Qt6CoreTools" CACHE PATH "Qt6 Core Tools dir" FORCE)
    endif()
    if(EXISTS "${QT_HOST_PATH}/lib/cmake/Qt6GuiTools")
        set(Qt6GuiTools_DIR "${QT_HOST_PATH}/lib/cmake/Qt6GuiTools" CACHE PATH "Qt6 Gui Tools dir" FORCE)
    endif()
    if(EXISTS "${QT_HOST_PATH}/lib/cmake/Qt6WidgetsTools")
        set(Qt6WidgetsTools_DIR "${QT_HOST_PATH}/lib/cmake/Qt6WidgetsTools" CACHE PATH "Qt6 Widgets Tools dir" FORCE)
    endif()
endif()

set(CMAKE_FIND_ROOT_PATH ${SYSROOT_CANDIDATES})
# Prevent CMake from converting sysroot include directory into -isystem,
# which breaks clang/gcc's C++ stdlib include_next hierarchy.
list(APPEND CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES "/usr/${TOOLCHAIN_PREFIX}/include")
list(APPEND CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES "/usr/${TOOLCHAIN_PREFIX}/include")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
