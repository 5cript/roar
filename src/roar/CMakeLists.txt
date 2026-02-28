add_library(
  roar STATIC
    server.cpp
    client.cpp
    mechanics/ranges.cpp
    mechanics/cookie.cpp
    authorization/authorization.cpp
    authorization/basic_auth.cpp
    authorization/digest_auth.cpp
    routing/route.cpp
    routing/router.cpp
    session/factory.cpp
    session/session.cpp
    ssl/make_ssl_context.cpp
    websocket/websocket_session.cpp
    websocket/websocket_client.cpp
    websocket/websocket_base.cpp
    utility/shutdown_barrier.cpp
    utility/sha.cpp
    utility/date.cpp
)

add_library(
  roar-curl STATIC
    curl/sources/file_source.cpp
    curl/sources/string_source.cpp
    curl/global_curl_context.cpp
    curl/request.cpp
    curl/response.cpp
    url/encode.cpp
    url/ipv4.cpp
    url/ipv6.cpp
    url/url.cpp
)

add_library(
  roar-no-net STATIC
    mime_type.cpp
    filesystem/special_paths.cpp
    utility/base64.cpp
    filesystem/jail.cpp
)

include(../../cmake/warnings.cmake)

target_compile_features(roar-no-net PRIVATE cxx_std_20)
target_include_directories(roar-no-net PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(
  roar-no-net
  PUBLIC Boost::boost
)

target_compile_features(roar-curl PRIVATE cxx_std_20)
target_include_directories(roar-curl PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(
  roar-curl
  PUBLIC CURL::libcurl roar-no-net
)

target_compile_features(roar PRIVATE cxx_std_20)
target_include_directories(roar PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../include
                                       ${CURL_INCLUDE_DIR})
target_link_libraries(
  roar
  PUBLIC Boost::boost
         promise
         roar-cryptopp
         OpenSSL::SSL
         OpenSSL::Crypto
         roar-no-net
         roar-curl
)

add_library(roar::roar ALIAS roar)
add_library(roar::no-net ALIAS roar-no-net)
add_library(roar::curl ALIAS roar-curl)

set_target_warnings(roar)
set_target_warnings(roar-no-net)
set_target_warnings(roar-curl)

if(${ROAR_ENABLE_NLOHMANN_JSON})
  target_link_libraries(roar-curl PUBLIC nlohmann_json::nlohmann_json)
  target_compile_definitions(roar-curl PUBLIC ROAR_ENABLE_NLOHMANN_JSON=1)
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
  target_link_libraries(roar-curl PUBLIC -lbcrypt)

  if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    # target_compile_definitions(roar PUBLIC BOOST_USE_WINDOWS_H=1)
  else()
    target_link_libraries(roar PUBLIC -latomic)
    target_link_libraries(roar-curl PUBLIC -latomic)
  endif()
else()

endif()

set_target_outputs(roar)
set_target_outputs(roar-no-net)
set_target_outputs(roar-curl)
