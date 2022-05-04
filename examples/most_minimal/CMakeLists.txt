add_executable(most_minimal main.cpp)

target_link_libraries(most_minimal PRIVATE roar)

target_compile_definitions(most_minimal PRIVATE ROAR_ENABLE_NLOHMANN_JSON=1)

set_target_outputs(most_minimal)