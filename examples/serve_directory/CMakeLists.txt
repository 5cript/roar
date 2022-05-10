add_executable(file_server main.cpp file_server.cpp)

target_link_libraries(file_server PRIVATE roar)

target_compile_definitions(file_server PRIVATE ROAR_ENABLE_NLOHMANN_JSON=1)

set_target_outputs(file_server)