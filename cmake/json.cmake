include(FetchContent)

message(STATUS "Setting up nlohmann::json")
FetchContent_Declare(nlohmann_json
        GIT_REPOSITORY "https://github.com/nlohmann/json/"
        GIT_TAG cdf52ae9bef77a0844e02e42df6d2df83a55c4b9
)
option(JSON_MultipleHeaders "Disable nlohmann JSON multi header default." OFF)
FetchContent_MakeAvailable(nlohmann_json)