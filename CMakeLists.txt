cmake_minimum_required(VERSION 3.16)

project(roar VERSION 0.1.0)

if(PROJECT_IS_TOP_LEVEL)
    if (NOT EXISTS ${CMAKE_BINARY_DIR}/CMakeCache.txt)
        if (NOT CMAKE_BUILD_TYPE)
            set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
        endif()
    endif()
endif()

include(./cmake/warnings.cmake)
include(./cmake/example_target.cmake)
include(./cmake/options.cmake)
include(./cmake/brew.cmake)

# Deps
include(./cmake/dependencies/boost.cmake)
include(./cmake/dependencies/ssl.cmake)
include(./cmake/dependencies/curl.cmake)
include(./cmake/dependencies/cryptopp.cmake)
include(./cmake/dependencies/xhawk18_promise.cmake)

if (${ROAR_ENABLE_NLOHMANN_JSON})
    include(./cmake/dependencies/nlohmann_json.cmake)
endif()

add_subdirectory(src/roar)

if (${ROAR_BUILD_EXAMPLES})
    add_subdirectory(examples)
endif()

if (${ROAR_BUILD_TESTS})
    include(./cmake/test_target.cmake)
    include(./cmake/dependencies/gtest.cmake)
    add_subdirectory(test)
endif()

if (${ROAR_BUILD_DOCUMENTATION})
    add_subdirectory(docs_source)
endif()
