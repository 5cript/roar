add_library(roar STATIC
  server.cpp
  ssl/make_ssl_context.cpp
)

find_package(Boost 1.78.0 REQUIRED COMPONENTS system)

target_include_directories(roar PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(roar PUBLIC Boost::system)

set_target_outputs(roar)