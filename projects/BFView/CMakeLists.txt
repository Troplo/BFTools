cmake_minimum_required(VERSION 3.25)
project(BFView LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS
    Core
    Gui
    Widgets
    OPTIONAL_COMPONENTS
    Svg
    REQUIRED
)

set(BFVIEW_PLATFORM_SOURCES "")
if (WIN32)
    enable_language(RC)
    list(APPEND BFVIEW_PLATFORM_SOURCES resources/BFView.rc)
endif()

add_executable(BFView
    ${BFVIEW_PLATFORM_SOURCES}
    include/BFView/Models/ArchiveTreeModel.hpp
    include/BFView/Models/ArchiveSortFilterProxyModel.hpp
    include/BFView/Models/NavigationHistory.hpp
    include/BFView/Views/FolderContentsModel.hpp
    include/BFView/Views/LocationBreadcrumb.hpp
    include/BFView/Views/FileManagerView.hpp
    include/BFView/Dialogs/DefragProgressDialog.hpp
    include/BFView/Dialogs/ExtractProgressDialog.hpp
    include/BFView/Dialogs/LoadArchiveProgressDialog.hpp
    include/BFView/Dialogs/NewArchiveDialog.hpp
    include/BFView/Dialogs/ProjectSelectDialog.hpp
    include/BFView/Dialogs/SettingsDialog.hpp
    include/BFView/Platform/FileAssociation.hpp
    include/BFView/Settings/SettingsManager.hpp
    include/BFView/Theme/ThemeManager.hpp
    include/BFView/Handlers/IFileHandler.hpp
    include/BFView/Handlers/XbtFileHandler.hpp
    include/BFView/Handlers/FileHandlerManager.hpp
    include/BFView/MainWindow.hpp
    src/Models/ArchiveTreeModel.cpp
    src/Models/ArchiveSortFilterProxyModel.cpp
    src/Models/NavigationHistory.cpp
    src/Views/FolderContentsModel.cpp
    src/Views/LocationBreadcrumb.cpp
    src/Views/FileManagerView.cpp
    src/Dialogs/DefragProgressDialog.cpp
    src/Dialogs/ExtractProgressDialog.cpp
    src/Dialogs/LoadArchiveProgressDialog.cpp
    src/Dialogs/NewArchiveDialog.cpp
    src/Dialogs/ProjectSelectDialog.cpp
    src/Dialogs/SettingsDialog.cpp
    src/Platform/FileAssociation.cpp
    src/Settings/SettingsManager.cpp
    src/Theme/ThemeManager.cpp
    src/Handlers/XbtFileHandler.cpp
    src/Handlers/FileHandlerManager.cpp
    src/MainWindow.cpp
    src/main.cpp
    resources/bfview.qrc
)

target_include_directories(BFView PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/resources
)

target_link_libraries(BFView PRIVATE
    NomadArchives
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    tracy
)

if (TARGET Qt6::Svg)
    target_link_libraries(BFView PRIVATE Qt6::Svg)
endif()

if (WIN32)
    target_link_libraries(BFView PRIVATE advapi32 shell32 psapi dwmapi)
    # Hide console window in GUI builds on Windows
    set_target_properties(BFView PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

if (UNIX AND NOT APPLE)
    set_target_properties(BFView PROPERTIES
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "$ORIGIN/lib"
    )
endif()
