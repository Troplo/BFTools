include(FetchContent)

set(LIB_NAME "tracy")
message(STATUS "Setting up ${LIB_NAME}")

FetchContent_Declare(
        ${LIB_NAME}
        GIT_REPOSITORY https://github.com/wolfpld/tracy
        # keep in sync with NexusTools
        GIT_TAG v0.13.1
        GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(${LIB_NAME})

file(GLOB SRC_TRACY
        "${tracy_SOURCE_DIR}/public/TracyClient.cpp"
)

add_library(tracy STATIC ${SRC_TRACY})
source_group(TREE ${tracy_SOURCE_DIR} PREFIX "tracy" FILES ${SRC_TRACY})

target_include_directories(tracy PUBLIC
        "${tracy_SOURCE_DIR}/public"
)

option(TRACY_ENABLE "Enable Tracy profiler" OFF)

if (TRACY_ENABLE)
    target_compile_definitions(tracy PUBLIC
            TRACY_ENABLE
            TRACY_ON_DEMAND
    )
    if (TRACY_NO_EXIT)
        target_compile_definitions(tracy PUBLIC TRACY_NO_EXIT)
    endif()
    if (TRACY_DELAYED_INIT)
        target_compile_definitions(tracy PUBLIC TRACY_DELAYED_INIT)
    endif()
    if (TRACY_NO_CALLSTACK)
        target_compile_definitions(tracy PUBLIC TRACY_NO_CALLSTACK)
    endif()
    message(STATUS "Tracy profiler is ENABLED (TRACY_ENABLE=ON)")
else()
    message(STATUS "Tracy profiler is DISABLED")
endif()

if (WIN32)
    target_link_libraries(tracy PUBLIC
            ws2_32
            dbghelp
    )
else()
    target_link_libraries(tracy PUBLIC
            pthread
            dl
    )
endif()
