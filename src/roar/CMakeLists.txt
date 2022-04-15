add_library(roar STATIC
  dummy.cpp
)

target_include_directories(roar PUBLIC ${CMAKE_SOURCE_DIR}/include)

set_target_outputs(roar)