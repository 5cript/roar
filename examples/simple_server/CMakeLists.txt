add_executable(simple_server main.cpp request_listener.cpp)

target_link_libraries(simple_server PRIVATE roar)

target_compile_definitions(simple_server PRIVATE ROAR_ENABLE_NLOHMANN_JSON=1)

set_target_outputs(simple_server)