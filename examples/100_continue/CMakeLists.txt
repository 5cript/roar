add_executable(expect_continue main.cpp request_listener.cpp)

target_link_libraries(expect_continue PRIVATE roar)

target_compile_definitions(expect_continue PRIVATE ROAR_ENABLE_NLOHMANN_JSON=1)

set_target_outputs(expect_continue)