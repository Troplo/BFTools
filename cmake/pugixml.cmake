find_package(pugixml QUIET)
if(NOT pugixml_FOUND)
    message(STATUS "pugixml package not found, fetching...")
    include(FetchContent)
    FetchContent_Declare(
            pugixml
            URL https://github.com/zeux/pugixml/releases/download/v1.16/pugixml-1.16.tar.gz
    )
    FetchContent_MakeAvailable(pugixml)
endif()

if(TARGET pugixml::pugixml)
    set(PUGIXML_TARGET pugixml::pugixml)
else()
    set(PUGIXML_TARGET pugixml)
endif()
