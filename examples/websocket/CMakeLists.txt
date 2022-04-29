add_executable(websocket main.cpp request_listener.cpp)

target_link_libraries(websocket PRIVATE roar)

target_compile_definitions(websocket PRIVATE ROAR_ENABLE_NLOHMANN_JSON=1)

set_target_outputs(websocket)