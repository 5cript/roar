add_executable(request_listener main.cpp)

target_link_libraries(request_listener PRIVATE roar)

set_target_outputs(request_listener)