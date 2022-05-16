project(promise-git NONE)

include(FetchContent)
FetchContent_Declare(
    promise
    GIT_REPOSITORY https://github.com/5cript/promise-cpp.git
    GIT_TAG        24a69748249ccd130eef2814548362ef3fb2999f
)

FetchContent_MakeAvailable(promise)

if (WIN32)
  # MS SOCK
  target_link_libraries(promise PUBLIC -lws2_32 -lmswsock -lbcrypt)

  target_compile_definitions(promise PUBLIC -DPROMISE_MULTITHREAD=1 -DPROMISE_BUILD_EXAMPLES=off)
endif()