project(fmt-git NONE)

include(FetchContent)
FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG        ae963e444fcdf1ad45f662e0c706568f34506528
)

FetchContent_MakeAvailable(fmt)