set(PROMISE_BUILD_EXAMPLES off CACHE BOOL "Promise build examples" FORCE)
set(PROMISE_MULTITHREAD on CACHE BOOL "Promise multithreading on")

option(ROAR_EXTERNAL_PROMISE "Use an external promise library (provide it manually)" OFF)
set(ROAR_PROMISE_GIT_REPOSITORY "https://github.com/5cript/promise-cpp.git" CACHE STRING "The URL from which to clone the promiselib repository")
set(ROAR_PROMISE_GIT_TAG "affb386031896805c198485f10fd56215b1a4460" CACHE STRING "The git tag or commit hash to checkout from the promiselib repository")

if (ROAR_EXTERNAL_PROMISE)
else()
    include(FetchContent)
    FetchContent_Declare(
        promise
        GIT_REPOSITORY ${ROAR_PROMISE_GIT_REPOSITORY}
        GIT_TAG        ${ROAR_PROMISE_GIT_TAG}
    )
    FetchContent_MakeAvailable(promise)

    if (WIN32)
        # MS SOCK
        target_link_libraries(promise PUBLIC -lws2_32 -lmswsock -lbcrypt)
    endif()
endif()