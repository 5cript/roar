cmake_minimum_required(VERSION 3.21)

project(roar VERSION 0.1.0)

if(PROJECT_IS_TOP_LEVEL)
    if(NOT "${CMAKE_CXX_STANDARD}")
        set(CMAKE_CXX_STANDARD 20)
    endif()
    set(CMAKE_CXX_EXTENSIONS OFF)

    if (NOT EXISTS ${CMAKE_BINARY_DIR}/CMakeCache.txt)
        if (NOT CMAKE_BUILD_TYPE)
            set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
        endif()
    endif()
endif()

include (./cmake/example_target.cmake)
include (./cmake/options.cmake)

add_subdirectory(src/roar)

if (${ROAR_BUILD_EXAMPLES})
    add_subdirectory(examples)
endif()