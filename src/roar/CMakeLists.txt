add_library(roar STATIC
  server.cpp
  routing/route.cpp
  routing/router.cpp
  session/factory.cpp
  session/session.cpp
  ssl/make_ssl_context.cpp
)

find_package(Boost 1.78.0 REQUIRED COMPONENTS system)
find_library(LSSL ssl)
find_library(LCRYPTO crypto)
find_library(LCRYPTOPP cryptopp)

target_include_directories(roar PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(roar PUBLIC Boost::system ${LSSL} ${LCRYPTOPP} ${LCRYPTO})

if (WIN32)
  # MS SOCK
  target_link_libraries(roar PUBLIC -lws2_32 -lmswsock -lbcrypt)

  if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    # target_compile_definitions(roar PUBLIC BOOST_USE_WINDOWS_H=1)
  else()
    target_link_libraries(roar PUBLIC -latomic)
  endif()
else()
endif()

set_target_outputs(roar)