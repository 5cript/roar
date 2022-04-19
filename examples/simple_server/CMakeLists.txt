add_executable(simple_server main.cpp)

target_link_libraries(simple_server PRIVATE roar)

set_target_outputs(simple_server)