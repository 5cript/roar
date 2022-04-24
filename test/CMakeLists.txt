add_executable(roar-tests tests.cpp)

target_link_libraries(roar-tests PRIVATE roar gtest gtest_main gmock test-settings)

target_compile_definitions(roar-tests PRIVATE ROAR_ENABLE_NLOHMANN_JSON=1)

set_test_target_outputs(roar-tests)