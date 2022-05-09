#pragma once

#include "util/common_server_setup.hpp"
#include "util/common_listeners.hpp"

#include <roar/routing/request_listener.hpp>
#include <roar/curl/request.hpp>
#include <roar/literals/memory.hpp>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace Roar::Tests
{
    class HttpServerTests
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

    TEST_F(HttpServerTests, StringPathIsCorrectlyRouted)
    {
        auto res = Curl::Request{}.get(url("/index.txt"));
        EXPECT_EQ(res.code(), boost::beast::http::status::ok);
    }

    TEST_F(HttpServerTests, RegexPathIsCorrectlyRouted)
    {
        using namespace ::testing;

        nlohmann::json body;
        auto res = Curl::Request{}.sink(body).get(url("/something/here"));
        EXPECT_EQ(res.code(), boost::beast::http::status::ok);
        EXPECT_EQ(body["path"], "/something/here");
        ASSERT_THAT(body["matches"], ElementsAre("something", "here"));
    }

    TEST_F(HttpServerTests, StringPathsTakePrecedenceOverRegexPaths)
    {
        using namespace ::testing;

        std::string body;
        auto res = Curl::Request{}.sink(body).get(url("/a/b"));
        EXPECT_EQ(res.code(), boost::beast::http::status::ok);
        EXPECT_EQ(body, "AB");
    }

    TEST_F(HttpServerTests, EncryptedServerAcceptsEncryptedConnection)
    {
        auto res =
            Curl::Request{}.verifyPeer(false).verifyHost(false).get(urlEncryptedServer("/index.txt", {.secure = true}));
        EXPECT_EQ(res.code(), boost::beast::http::status::ok);
    }

    TEST_F(HttpServerTests, EncryptedServerDoesNotAcceptUnencryptedConnection)
    {
        std::unordered_map<std::string, std::string> headers;
        auto res = Curl::Request{}.headerSink(headers).verifyPeer(false).verifyHost(false).get(
            urlEncryptedServer("/index.txt", {.secure = false}));
        EXPECT_NE(headers.find("Strict-Transport-Security"), std::end(headers));
        EXPECT_EQ(res.code(), boost::beast::http::status::forbidden);
    }

    TEST_F(HttpServerTests, UnencryptedServerDoesNotAcceptEncryptedConnection)
    {
        auto res = Curl::Request{}.get(url("/a/b", {.secure = true}));
        EXPECT_NE(res.result(), CURLE_OK);
        EXPECT_NE(res.code(), boost::beast::http::status::ok);
    }

    TEST_F(HttpServerTests, EncryptedServerDoesAcceptUnencryptedConnectionWhenExplicitlyAllowed)
    {
        auto res = Curl::Request{}.get(urlEncryptedServer("/unsecure", {.secure = false}));
        EXPECT_EQ(res.code(), boost::beast::http::status::no_content);
    }

    TEST_F(HttpServerTests, CanSendUsingSendIntermediate)
    {
        std::string body;
        auto res = Curl::Request{}.sink(body).get(url("/sendIntermediate", {.secure = false}));
        EXPECT_EQ(res.code(), boost::beast::http::status::ok);
        EXPECT_EQ(body, "Hi");
    }
}