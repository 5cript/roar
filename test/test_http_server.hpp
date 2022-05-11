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
        boost::beast::error_code ec;
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
        auto res = Curl::Request{}.sink(body).setHeaderField("Range", "bytes=0-100").get(url("/slice"));
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
            Curl::Request{}.sink(body).headerSink(headers).setHeaderField("Range", "bytes=200-350").get(url("/slice"));
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
                       .setHeaderField("Range", "bytes=0-100, 200-350, 400-465")
                       .get(url("/slice"));
        std::string base;
        LetterGenerator gen;
        for (int i = 0; i != 1024; ++i)
            base.push_back(gen());
        EXPECT_EQ(res.code(), boost::beast::http::status::partial_content);
        auto pos = headers["Content-Type"].find("boundary=");
        ASSERT_NE(pos, std::string::npos);
        const auto boundary = headers["Content-Type"].substr(pos + 9);
        const auto inplaceFormatted = fmt::format(
            "{}\r\nContent-Type: plain/text\r\nContent-Range: bytes 0-100/25600\r\n\r\n{}\r\n"
            "{}\r\nContent-Type: plain/text\r\nContent-Range: bytes 200-350/25600\r\n\r\n{}\r\n"
            "{}\r\nContent-Type: plain/text\r\nContent-Range: bytes 400-465/25600\r\n\r\n{}\r\n"
            "{}",
            boundary,
            base.substr(0, 100),
            boundary,
            base.substr(200, 150),
            boundary,
            base.substr(400, 65),
            boundary);

        EXPECT_EQ(body, inplaceFormatted);
    }

    TEST_F(HttpServerTests, CanMakeMultipartRequest2)
    {
        std::string body;
        std::unordered_map<std::string, std::string> headers;
        auto res = Curl::Request{}
                       .sink(body)
                       .headerSink(headers)
                       .setHeaderField("Range", "bytes=200-6800, 0-10, 137-10521")
                       .get(url("/slice"));
        std::string base;
        LetterGenerator gen;
        for (int i = 0; i != 25000; ++i)
            base.push_back(gen());
        EXPECT_EQ(res.code(), boost::beast::http::status::partial_content);
        auto pos = headers["Content-Type"].find("boundary=");
        ASSERT_NE(pos, std::string::npos);
        const auto boundary = headers["Content-Type"].substr(pos + 9);
        const auto inplaceFormatted = fmt::format(
            "{}\r\nContent-Type: plain/text\r\nContent-Range: bytes 200-6800/25600\r\n\r\n{}\r\n"
            "{}\r\nContent-Type: plain/text\r\nContent-Range: bytes 0-10/25600\r\n\r\n{}\r\n"
            "{}\r\nContent-Type: plain/text\r\nContent-Range: bytes 137-10521/25600\r\n\r\n{}\r\n"
            "{}",
            boundary,
            base.substr(200, 6600),
            boundary,
            base.substr(0, 10),
            boundary,
            base.substr(137, 10521 - 137),
            boundary);

        EXPECT_EQ(body, inplaceFormatted);
    }

    TEST_F(HttpServerTests, InvalidRangeRequestIsRejected)
    {
        auto res = Curl::Request{}.setHeaderField("Range", "bytes?0-100").get(url("/slice"));
        EXPECT_EQ(res.code(), boost::beast::http::status::bad_request);

        res = Curl::Request{}.setHeaderField("Range", "=0-100").get(url("/slice"));
        EXPECT_EQ(res.code(), boost::beast::http::status::bad_request);

        res = Curl::Request{}.setHeaderField("Range", "asdf").get(url("/slice"));
        EXPECT_EQ(res.code(), boost::beast::http::status::bad_request);

        res = Curl::Request{}.setHeaderField("Range", "bytes=0").get(url("/slice"));
        EXPECT_EQ(res.code(), boost::beast::http::status::bad_request);

        res = Curl::Request{}.setHeaderField("Range", "bytes=x").get(url("/slice"));
        EXPECT_EQ(res.code(), boost::beast::http::status::bad_request);

        res = Curl::Request{}.setHeaderField("Range", "bytes=0-").get(url("/slice"));
        EXPECT_EQ(res.code(), boost::beast::http::status::bad_request);

        res = Curl::Request{}.setHeaderField("Range", "bytes=0-x").get(url("/slice"));
        EXPECT_EQ(res.code(), boost::beast::http::status::bad_request);

        res = Curl::Request{}.setHeaderField("Range", "bytes=100-0").get(url("/slice"));
        EXPECT_EQ(res.code(), boost::beast::http::status::bad_request);
    }
}