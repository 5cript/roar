#pragma once

#include "util/common_listeners.hpp"
#include "util/common_server_setup.hpp"
#include "util/test_sources.hpp"

#include <roar/routing/request_listener.hpp>
#include <roar/curl/request.hpp>
#include <roar/detail/literals/memory.hpp>

#include <gtest/gtest.h>

namespace Roar::Tests
{
    class ReadingTestListener
    {
      public:
        std::string body;
        std::size_t contentLength;

      private:
        ROAR_MAKE_LISTENER(ReadingTestListener);

        ROAR_POST(staticString)({.path = "/staticString"});
        ROAR_PUT(failPut)({.path = "/failPut"});

      private:
        BOOST_DESCRIBE_CLASS(ReadingTestListener, (), (), (), (roar_staticString, roar_failPut))
    };
    inline void ReadingTestListener::staticString(Session& session, EmptyBodyRequest&& req)
    {
        using namespace boost::beast::http;
        if (req.find(field::content_length) != std::end(req))
            contentLength = std::stoull(std::string{req[field::content_length]});
        session.template read<string_body>(std::move(req))
            ->noBodyLimit()
            .commit()
            .then([this](auto& session, auto const& req) {
                body = req.body();
            });
    }
    inline void ReadingTestListener::failPut(Session& session, EmptyBodyRequest&& req)
    {
        using namespace boost::beast::http;
        session.sendStandardResponse(boost::beast::http::status::bad_request);
        session.awaitClientClose(std::move(req));
    }

    class ReadingTests
        : public CommonServerSetup
        , public ::testing::Test
    {
      protected:
        using CommonServerSetup::url;

        void SetUp() override
        {
            makeDefaultServer();
            listener_ = server_->installRequestListener<ReadingTestListener>();
        }

      protected:
        std::shared_ptr<ReadingTestListener> listener_;
    };

    TEST_F(ReadingTests, CanReadStaticStringBody)
    {
        std::string body = "This is the body";
        Curl::Request{}.source(body).post(url("/staticString"));
        EXPECT_EQ(body, listener_->body);
        EXPECT_EQ(body.size(), listener_->contentLength);
    }

    TEST_F(ReadingTests, CanReadBigStaticStringBody)
    {
        using namespace Roar::Literals;
        Curl::Request{}.emplaceSource<BigSource>(1_MiB).post(url("/staticString"));
        LetterGenerator gen;
        bool allSame = true;
        for (auto const& i : listener_->body)
            allSame &= (i == gen());

        EXPECT_TRUE(allSame);
        EXPECT_EQ(1_MiB, listener_->contentLength);
    }

    TEST_F(ReadingTests, CanReadChunkedData)
    {
        using namespace Roar::Literals;
        Curl::Request{}.emplaceSource<ChunkedSource>(1_MiB).post(url("/staticString"));
        LetterGenerator gen;
        bool allSame = true;
        for (auto const& i : listener_->body)
            allSame &= (i == gen());

        EXPECT_TRUE(allSame);
    }

    TEST_F(ReadingTests, ImmediateErrorResponseResults)
    {
        using namespace Roar::Literals;
        auto now = std::chrono::system_clock::now();
        const auto res = Curl::Request{}.emplaceSource<ChunkedSource>(1_MiB).put(url("/failPut"));
        EXPECT_EQ(res.code(), boost::beast::http::status::bad_request);
        EXPECT_LT(std::chrono::system_clock::now() - now, std::chrono::seconds{1});
    }
}