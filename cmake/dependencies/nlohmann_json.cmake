option(ROAR_EXTERNAL_NLOHMANN_JSON "Use an external nlohmann_json library (provide it manually)" OFF)
set(ROAR_NLOHMANN_JSON_GIT_REPOSITORY "https://github.com/nlohmann/json.git" CACHE STRING "The URL from which to clone the nlohmann_json repository")
set(ROAR_NLOHMANN_JSON_GIT_TAG "9dfa7226693012ed5bcf5ab3bc5d8e69d58006ab" CACHE STRING "The git tag or commit hash to checkout from the nlohmann_json repository")

if (ROAR_EXTERNAL_NLOHMANN_JSON)
else()
    include(FetchContent)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY ${ROAR_NLOHMANN_JSON_GIT_REPOSITORY}
        GIT_TAG        ${ROAR_NLOHMANN_JSON_GIT_TAG}
    )

    FetchContent_MakeAvailable(nlohmann_json)
endif()