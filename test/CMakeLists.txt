add_executable(roar-tests tests.cpp)

target_link_libraries(roar-tests PRIVATE roar gtest gtest_main gmock test-settings)

target_compile_definitions(roar-tests PRIVATE ROAR_ENABLE_NLOHMANN_JSON=1)

set_test_target_outputs(roar-tests)

# If msys2, copy dynamic libraries to executable directory, visual studio does this automatically.
# And there is no need on linux.
if (DEFINED ENV{MSYSTEM})
    add_custom_command(TARGET roar-tests POST_BUILD
        COMMAND bash -c "ldd $<TARGET_FILE:roar-tests>" | "grep" "clang" | awk "NF == 4 { system(\"${CMAKE_COMMAND} -E copy \" \$3 \" $<TARGET_FILE_DIR:roar-tests>\") }"
        VERBATIM
    )
endif()