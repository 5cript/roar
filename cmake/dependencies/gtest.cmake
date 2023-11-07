option(ROAR_EXTERNAL_GTEST "Use an external gtest library (provide it manually)" OFF)
set(ROAR_GTEST_GIT_REPOSITORY "https://github.com/google/googletest.git" CACHE STRING "The URL from which to clone the gtest repository")
set(ROAR_GTEST_GIT_TAG "release-1.11.0" CACHE STRING "The git tag or commit hash to checkout from the gtest repository")

if (${ROAR_EXTERNAL_GTEST})
else()
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        release-1.11.0
    )

    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(BUILD_GMOCK ON CACHE BOOL "" FORCE)
    set(BUILD_GTEST ON CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(googletest)
endif()