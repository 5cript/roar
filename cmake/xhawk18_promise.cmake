project(promise-git NONE)

set(PROMISE_BUILD_EXAMPLES off CACHE BOOL "Promise build examples" FORCE)
set(PROMISE_MULTITHREAD on CACHE BOOL "Promise multithreading on")
include(FetchContent)
FetchContent_Declare(
    promise
    GIT_REPOSITORY https://github.com/5cript/promise-cpp.git
    GIT_TAG        affb386031896805c198485f10fd56215b1a4460
)

FetchContent_MakeAvailable(promise)

if (WIN32)
  # MS SOCK
  target_link_libraries(promise PUBLIC -lws2_32 -lmswsock -lbcrypt)
endif()