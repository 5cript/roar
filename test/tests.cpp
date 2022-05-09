#include "curl/test_request.hpp"
#include "test_cors.hpp"
#include "test_reading.hpp"
#include "test_http_server.hpp"
#include "test_web_socket.hpp"
#include "test_serve.hpp"

#include <gtest/gtest.h>

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}