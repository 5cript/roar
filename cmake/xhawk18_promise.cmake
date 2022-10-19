project(promise-git NONE)

set(PROMISE_BUILD_EXAMPLES off CACHE BOOL "Promise build examples" FORCE)
set(PROMISE_MULTITHREAD on CACHE BOOL "Promise multithreading on")
include(FetchContent)
FetchContent_Declare(
    promise
    GIT_REPOSITORY https://github.com/5cript/promise-cpp.git
    GIT_TAG        9a891eb600db71b5ecaf0fefbc3a503a10cb024b
)

FetchContent_MakeAvailable(promise)

if (WIN32)
  # MS SOCK
  target_link_libraries(promise PUBLIC -lws2_32 -lmswsock -lbcrypt)
endif()