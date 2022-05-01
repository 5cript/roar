project(promise-git NONE)

include(FetchContent)
FetchContent_Declare(
    promise
    GIT_REPOSITORY https://github.com/5cript/promise-cpp.git
    GIT_TAG        06378ef7b587f5d97215325e42a5ed544bc49fe2
)

FetchContent_MakeAvailable(promise)

if (WIN32)
  # MS SOCK
  target_link_libraries(promise PUBLIC -lws2_32 -lmswsock -lbcrypt)
endif()