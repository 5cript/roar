#include "util/executeable_path.hpp"
#include "curl/test_request.hpp"
#include "test_cors.hpp"
#include "test_reading.hpp"
#include "test_http_server.hpp"
#include "test_web_socket.hpp"
#include "test_serve.hpp"
#include "test_url.hpp"
#include "test_secure_async_client.hpp"
#include "test_unsecure_async_client.hpp"

#include <gtest/gtest.h>

std::filesystem::path programDirectory;

int main(int argc, char** argv)
{
    programDirectory = std::filesystem::path{argv[0]}.parent_path();

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}