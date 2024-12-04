#pragma once

#include "util/common_server_setup.hpp"
#include "util/common_listeners.hpp"
#include "util/test_sources.hpp"
#include "util/temporary_directory.hpp"

#include <roar/body/range_file_body.hpp>
#include <roar/routing/request_listener.hpp>
#include <roar/curl/request.hpp>
#include <roar/literals/memory.hpp>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <future>
#include <sstream>

namespace Roar::Tests
{
    class RangeRoutes
    {
      private:
        ROAR_MAKE_LISTENER(RangeRoutes);
        ROAR_GET(slice)("/slice");

      private:
        BOOST_DESCRIBE_CLASS(RangeRoutes, (), (), (), (roar_slice))
        TemporaryDirectory tempDir_{TEST_TEMPORARY_DIRECTORY};
    };

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
            server_->installRequestListener<RangeRoutes>();
        }

      protected:
        std::shared_ptr<SimpleRoutes> listener_;
    };

    inline void RangeRoutes::slice(Session& session, EmptyBodyRequest&& req)
    {
        using namespace Roar::Literals;
        using namespace boost::beast::http;

        {
            std::ofstream writer{tempDir_.path() / "index.txt", std::ios::binary};
            LetterGenerator gen;
            for (int i = 0; i != 25_KiB; ++i)
                writer.put(gen());
        }
        const auto ranges = req.ranges();
        if (!ranges)
            return session.sendStandardResponse(status::bad_request, "Cannot parse ranges.");

        RangeFileBody::value_type body;
        std::error_code ec;
        body.open(tempDir_.path() / "index.txt", std::ios_base::in, ec);
        if (ec)
            return session.sendStandardResponse(status::internal_server_error, "Cannot open file for reading.");
        try
        {
            body.setReadRanges(*ranges, "plain/text");
        }
        catch (...)
        {
            return session.sendStandardResponse(status::bad_request, "Invalid ranges.");
        }

        session.send<RangeFileBody>(req, std::move(body))
            ->useFixedTimeout(std::chrono::seconds{10})
            .commit()
            .fail([](auto e) {
                std::cerr << e << std::endl;
            });
    }

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

    TEST_F(HttpServerTests, CanSendFilePartially)
    {
        std::string body;
        auto res = Curl::Request{}.sink(body).setHeader("Range", "bytes=0-100").get(url("/slice"));
        EXPECT_EQ(res.code(), boost::beast::http::status::partial_content);
        ASSERT_EQ(body.size(), 100);
        LetterGenerator gen;
        bool allEqual = true;
        for (int i = 0; i != 100; ++i)
        {
            allEqual &= body[i] == gen();
            if (!allEqual)
                break;
        }
        EXPECT_TRUE(allEqual);
    }

    TEST_F(HttpServerTests, CanMakeMultipartRequestForSingleRange)
    {
        std::string body;
        std::unordered_map<std::string, std::string> headers;
        auto res =
            Curl::Request{}.sink(body).headerSink(headers).setHeader("Range", "bytes=200-350").get(url("/slice"));
        std::string base;
        LetterGenerator gen;
        for (int i = 0; i != 1024; ++i)
            base.push_back(gen());
        auto pos = headers["Content-Type"].find("boundary=");
        EXPECT_EQ(pos, std::string::npos);
        EXPECT_EQ(res.code(), boost::beast::http::status::partial_content);
        EXPECT_EQ(body, base.substr(200, 150));
    }

    TEST_F(HttpServerTests, CanMakeMultipartRequest)
    {
        std::string body;
        std::unordered_map<std::string, std::string> headers;
        auto res = Curl::Request{}
                       .sink(body)
                       .headerSink(headers)
                       .setHeader("Range", "bytes=0-100, 200-350, 400-465")
                       .get(url("/slice"));
        std::string base;
        LetterGenerator gen;
        for (int i = 0; i != 1024; ++i)
            base.push_back(gen());
        EXPECT_EQ(res.code(), boost::beast::http::status::partial_content);
        auto pos = headers["Content-Type"].find("boundary=");
        ASSERT_NE(pos, std::string::npos);
        const auto boundary = headers["Content-Type"].substr(pos + 9);

        std::stringstream sstr;
        sstr << boundary << "\r\nContent-Type: plain/text\r\nContent-Range: bytes 0-100/25600\r\n\r\n"
             << base.substr(0, 100) << "\r\n"
             << boundary << "\r\nContent-Type: plain/text\r\nContent-Range: bytes 200-350/25600\r\n\r\n"
             << base.substr(200, 150) << "\r\n"
             << boundary << "\r\nContent-Type: plain/text\r\nContent-Range: bytes 400-465/25600\r\n\r\n"
             << base.substr(400, 65) << "\r\n"
             << boundary;
        const auto inplaceFormatted = sstr.str();

        EXPECT_EQ(body, inplaceFormatted);
    }

    TEST_F(HttpServerTests, CanMakeMultipartRequest2)
    {
        std::string body;
        std::unordered_map<std::string, std::string> headers;
        auto res = Curl::Request{}
                       .sink(body)
                       .headerSink(headers)
                       .setHeader("Range", "bytes=200-6800, 0-10, 137-10521")
                       .get(url("/slice"));
        std::string base;
        LetterGenerator gen;
        for (int i = 0; i != 25000; ++i)
            base.push_back(gen());
        EXPECT_EQ(res.code(), boost::beast::http::status::partial_content);
        auto pos = headers["Content-Type"].find("boundary=");
        ASSERT_NE(pos, std::string::npos);
        const auto boundary = headers["Content-Type"].substr(pos + 9);

        std::stringstream sstr;
        sstr << boundary << "\r\nContent-Type: plain/text\r\nContent-Range: bytes 200-6800/25600\r\n\r\n"
             << base.substr(200, 6600) << "\r\n"
             << boundary << "\r\nContent-Type: plain/text\r\nContent-Range: bytes 0-10/25600\r\n\r\n"
             << base.substr(0, 10) << "\r\n"
             << boundary << "\r\nContent-Type: plain/text\r\nContent-Range: bytes 137-10521/25600\r\n\r\n"
             << base.substr(137, 10521 - 137) << "\r\n"
             << boundary;
        const auto inplaceFormatted = sstr.str();

        EXPECT_EQ(body, inplaceFormatted);
    }

    TEST_F(HttpServerTests, InvalidRangeRequestIsRejected)
    {
        auto res = Curl::Request{}.setHeader("Range", "bytes?0-100").get(url("/slice"));
        EXPECT_EQ(res.code(), boost::beast::http::status::bad_request);

        res = Curl::Request{}.setHeader("Range", "=0-100").get(url("/slice"));
        EXPECT_EQ(res.code(), boost::beast::http::status::bad_request);

        res = Curl::Request{}.setHeader("Range", "asdf").get(url("/slice"));
        EXPECT_EQ(res.code(), boost::beast::http::status::bad_request);

        res = Curl::Request{}.setHeader("Range", "bytes=0").get(url("/slice"));
        EXPECT_EQ(res.code(), boost::beast::http::status::bad_request);

        res = Curl::Request{}.setHeader("Range", "bytes=x").get(url("/slice"));
        EXPECT_EQ(res.code(), boost::beast::http::status::bad_request);

        res = Curl::Request{}.setHeader("Range", "bytes=0-").get(url("/slice"));
        EXPECT_EQ(res.code(), boost::beast::http::status::bad_request);

        res = Curl::Request{}.setHeader("Range", "bytes=0-x").get(url("/slice"));
        EXPECT_EQ(res.code(), boost::beast::http::status::bad_request);

        res = Curl::Request{}.setHeader("Range", "bytes=100-0").get(url("/slice"));
        EXPECT_EQ(res.code(), boost::beast::http::status::bad_request);
    }

    TEST_F(HttpServerTests, CanExtractAuthorizationScheme)
    {
        using namespace boost::beast::http;

        std::promise<std::optional<AuthorizationScheme>> promise;
        listener_->dynamicGetFunc = [&promise](Session& session, EmptyBodyRequest&& req) {
            promise.set_value(req.authorizationScheme());
        };
        auto future = promise.get_future();
        auto res = Curl::Request{}.setHeader(field::authorization, "Basic dXNlcjpwYXNz").get(url("/dynamicGet"));
        ASSERT_EQ(future.wait_for(std::chrono::seconds(5)), std::future_status::ready);
        auto value = future.get();
        ASSERT_TRUE(value);
        EXPECT_EQ(*value, AuthorizationScheme::Basic);
    }

    TEST_F(HttpServerTests, CanExtractBasicAuthorization)
    {
        using namespace boost::beast::http;

        std::promise<std::optional<BasicAuth>> promise;
        listener_->dynamicGetFunc = [&promise](Session& session, EmptyBodyRequest&& req) {
            promise.set_value(req.basicAuth());
        };
        auto future = promise.get_future();
        auto res = Curl::Request{}.setHeader(field::authorization, "Basic dXNlcjpwYXNz").get(url("/dynamicGet"));
        ASSERT_EQ(future.wait_for(std::chrono::seconds(5)), std::future_status::ready);
        auto value = future.get();
        ASSERT_TRUE(value);
        EXPECT_EQ(value->user, "user");
        EXPECT_EQ(value->password, "pass");
    }

    TEST_F(HttpServerTests, CanExtractDigestAuthorization)
    {
        using namespace boost::beast::http;

        std::promise<std::optional<DigestAuth>> promise;
        listener_->dynamicGetFunc = [&promise](Session& session, EmptyBodyRequest&& req) {
            promise.set_value(req.digestAuth());
        };
        auto future = promise.get_future();
        auto res = Curl::Request{}
                       .setHeader(
                           field::authorization,
                           "Digest username=user, realm=\"wonderland\", uri=\"asdf.com\", response=\"bla\", "
                           "algorithm=md5, nonce=asdf, qop=\"auth\", opaque=xef, cnonce=50, nc=50")
                       .get(url("/dynamicGet"));
        ASSERT_EQ(future.wait_for(std::chrono::seconds(5)), std::future_status::ready);
        auto value = future.get();
        ASSERT_TRUE(value);
        EXPECT_EQ(value->username, "user");
        EXPECT_EQ(value->realm, "wonderland");
        EXPECT_EQ(value->uri, "asdf.com");
        EXPECT_EQ(value->response, "bla");
        EXPECT_EQ(value->algorithm, "md5");
        EXPECT_EQ(value->nonce, "asdf");
        EXPECT_EQ(value->qop, "auth");
        EXPECT_EQ(value->opaque, "xef");
        EXPECT_EQ(value->cnonce, "50");
        EXPECT_EQ(value->nc, "50");
    }

    // This does not mean that the digest is correct then.
    TEST_F(HttpServerTests, DigestParametersMayBeMissing)
    {
        using namespace boost::beast::http;

        std::promise<std::optional<DigestAuth>> promise;
        listener_->dynamicGetFunc = [&promise](Session& session, EmptyBodyRequest&& req) {
            promise.set_value(req.digestAuth());
        };
        auto future = promise.get_future();
        auto res = Curl::Request{}
                       .setHeader(
                           field::authorization,
                           "Digest username=user, realm=\"wonderland\", uri=\"asdf.com\", response=\"bla\", "
                           "algorithm=md5")
                       .get(url("/dynamicGet"));
        ASSERT_EQ(future.wait_for(std::chrono::seconds(5)), std::future_status::ready);
        auto value = future.get();
        ASSERT_TRUE(value);
        EXPECT_EQ(value->username, "user");
        EXPECT_EQ(value->realm, "wonderland");
        EXPECT_EQ(value->uri, "asdf.com");
        EXPECT_EQ(value->response, "bla");
        EXPECT_EQ(value->algorithm, "md5");
        EXPECT_EQ(value->nonce, "");
        EXPECT_EQ(value->qop, "");
        EXPECT_EQ(value->opaque, "");
        EXPECT_EQ(value->cnonce, "");
        EXPECT_EQ(value->nc, "");
    }

    TEST_F(HttpServerTests, CanExtractBearerAuthorization)
    {
        using namespace boost::beast::http;

        std::promise<std::optional<std::string>> promise;
        listener_->dynamicGetFunc = [&promise](Session& session, EmptyBodyRequest&& req) {
            promise.set_value(req.bearerAuth());
        };
        auto future = promise.get_future();
        auto res = Curl::Request{}.setHeader(field::authorization, "Bearer token").get(url("/dynamicGet"));
        ASSERT_EQ(future.wait_for(std::chrono::seconds(5)), std::future_status::ready);
        auto value = future.get();
        ASSERT_TRUE(value);
        EXPECT_EQ(*value, "token");
    }
}