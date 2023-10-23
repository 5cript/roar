#pragma once

#include <roar/request.hpp>
#include <roar/response.hpp>

#include "util/common_server_setup.hpp"
#include "util/common_listeners.hpp"
#include "util/test_sources.hpp"
#include "util/temporary_directory.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <future>

namespace Roar::Tests
{
    class SecureAsyncClientTests
        : public CommonServerSetup
        , public ::testing::Test
    {
      protected:
        void SetUp() override
        {
            makeDefaultServer();
            makeSecureServer();
            listener_ = server_->installRequestListener<SimpleRoutes>();
            secureServer_->installRequestListener<SimpleRoutes>();
        }

      protected:
        std::shared_ptr<SimpleRoutes> listener_;
    };

    TEST_F(SecureAsyncClientTests, ConnectingSecurelyToUnsecureServerFails)
    {
        auto client = makeClient("https");

        auto req = Roar::Request<boost::beast::http::empty_body>{};
        req.method(boost::beast::http::verb::get);
        req.host("::1");
        req.port(server_->getLocalEndpoint().port());
        req.target("/index.txt");
        req.version(11);

        std::promise<bool> awaitCompletion;
        client->request(std::move(req))
            .then([client, &awaitCompletion]() {
                awaitCompletion.set_value(true);
            })
            .fail([&awaitCompletion](auto e) {
                awaitCompletion.set_value(false);
            });

        EXPECT_FALSE(awaitCompletion.get_future().get());
    }

    TEST_F(SecureAsyncClientTests, CanSendRequestToSecureServerAndCompleteViaThen)
    {
        auto client = makeClient("https");

        auto req = Roar::Request<boost::beast::http::empty_body>{};
        req.method(boost::beast::http::verb::get);
        req.version(11);

        req.host("::1");
        req.port(secureServer_->getLocalEndpoint().port());
        req.target("/index.txt");

        std::promise<bool> awaitCompletion;
        client->request(std::move(req))
            .then([client, &awaitCompletion]() {
                awaitCompletion.set_value(true);
            })
            .fail([&awaitCompletion](auto e) {
                std::cerr << e << std::endl;
                awaitCompletion.set_value(false);
            });

        EXPECT_TRUE(awaitCompletion.get_future().get());
    }
}