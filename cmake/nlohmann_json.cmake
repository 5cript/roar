project(nlohmann_json-git NONE)

include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        a8a547d7a212a6a39943bbd5b4220be504a1a33e
)

FetchContent_MakeAvailable(nlohmann_json)