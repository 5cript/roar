cmake_minimum_required(VERSION 3.21)

project(roar VERSION 0.1.0)

if(PROJECT_IS_TOP_LEVEL)
    if (NOT EXISTS ${CMAKE_BINARY_DIR}/CMakeCache.txt)
        if (NOT CMAKE_BUILD_TYPE)
            set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
        endif()
    endif()
endif()

include (./cmake/warnings.cmake)
include (./cmake/example_target.cmake)
include (./cmake/options.cmake)

include (./cmake/xhawk18_promise.cmake)

if (${ROAR_ENABLE_NLOHMANN_JSON})
    include(./cmake/nlohmann_json.cmake)
endif()

add_subdirectory(src/roar)

if (${ROAR_BUILD_EXAMPLES})
    add_subdirectory(examples)
endif()

if (${ROAR_BUILD_TESTS})
    include(./cmake/test_target.cmake)
    include(./cmake/gtest.cmake)
    add_subdirectory(test)
endif()

if (${ROAR_BUILD_DOCUMENTATION})
    add_subdirectory(docs_source)
endif()