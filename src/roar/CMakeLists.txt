add_library(roar STATIC
  server.cpp
  routing/route.cpp
  routing/router.cpp
  session/factory.cpp
  session/session.cpp
  ssl/make_ssl_context.cpp
  websocket/websocket_session.cpp
  curl/sources/file_source.cpp
  curl/sources/string_source.cpp
  curl/global_curl_context.cpp
  curl/request.cpp
  curl/response.cpp
  utility/url.cpp
  utility/base64.cpp
)

include(../../cmake/curl.cmake)

find_package(Boost 1.78.0 REQUIRED COMPONENTS system)
find_library(LSSL ssl)
find_library(LCRYPTO crypto)
find_library(LCRYPTOPP cryptopp)

target_include_directories(roar PUBLIC ${CMAKE_SOURCE_DIR}/include ${CURL_INCLUDE_DIR})
target_link_libraries(roar PUBLIC Boost::system ${CURL_LIBRARIES} ${LSSL} ${LCRYPTOPP} ${LCRYPTO})

if (${ROAR_ENABLE_NLOHMANN_JSON})
    target_link_libraries(roar PUBLIC nlohmann_json::nlohmann_json)
    target_compile_definitions(roar PUBLIC ROAR_ENABLE_NLOHMANN_JSON=1)
endif()

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