add_library(
  roar STATIC
  server.cpp
  client.cpp
  mime_type.cpp
  mechanics/ranges.cpp
  mechanics/cookie.cpp
  authorization/authorization.cpp
  authorization/basic_auth.cpp
  authorization/digest_auth.cpp
  filesystem/jail.cpp
  filesystem/special_paths.cpp
  routing/route.cpp
  routing/router.cpp
  session/factory.cpp
  session/session.cpp
  ssl/make_ssl_context.cpp
  websocket/websocket_session.cpp
  websocket/websocket_client.cpp
  websocket/websocket_base.cpp
  curl/sources/file_source.cpp
  curl/sources/string_source.cpp
  curl/global_curl_context.cpp
  curl/request.cpp
  curl/response.cpp
  url/ipv4.cpp
  url/ipv6.cpp
  url/url.cpp
  url/encode.cpp
  utility/base64.cpp
  utility/shutdown_barrier.cpp
  utility/sha.cpp
  utility/date.cpp)

include(../../cmake/warnings.cmake)

target_compile_features(roar PUBLIC cxx_std_20)
target_include_directories(roar PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../include
                                       ${CURL_INCLUDE_DIR})
target_link_libraries(
  roar
  PUBLIC Boost::system
         promise
         roar-cryptopp
         CURL::libcurl
         OpenSSL::SSL
         OpenSSL::Crypto
)

set_target_warnings(roar)

if(${ROAR_ENABLE_NLOHMANN_JSON})
  target_link_libraries(roar PUBLIC nlohmann_json::nlohmann_json)
  target_compile_definitions(roar PUBLIC ROAR_ENABLE_NLOHMANN_JSON=1)
endif()

if (${Boost_VERSION_MAJOR} GREATER_EQUAL 1 AND ${Boost_VERSION_MINOR} GREATER_EQUAL 84)
else()
  target_compile_definitions(roar PUBLIC BOOST_ASIO_DISABLE_CONCEPTS=1)
endif()

if(WIN32)
  # MS SOCK
  target_link_libraries(roar PUBLIC -lws2_32 -lmswsock -lbcrypt)

  if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    # target_compile_definitions(roar PUBLIC BOOST_USE_WINDOWS_H=1)
  else()
    target_link_libraries(roar PUBLIC -latomic)
  endif()
else()

endif()

set_target_outputs(roar)
