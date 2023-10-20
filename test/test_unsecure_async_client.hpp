#pragma once

#include <roar/client.hpp>

#include "util/common_server_setup.hpp"
#include "util/common_listeners.hpp"
#include "util/test_sources.hpp"
#include "util/temporary_directory.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <future>

namespace Roar::Tests
{
    class AsyncClientTests
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

        std::shared_ptr<Client> makeClient()
        {
            return std::make_shared<Client>(Client::ConstructionArguments{.executor = executor_});
        }

      protected:
        std::shared_ptr<SimpleRoutes> listener_;
    };

    TEST_F(AsyncClientTests, CanSendRequestToServerAndCompleteViaThen)
    {
        auto client = makeClient();

        auto req = Request<boost::beast::http::empty_body>{};
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
                std::cerr << e << std::endl;
                awaitCompletion.set_value(false);
            });

        EXPECT_TRUE(awaitCompletion.get_future().get());
    }

    TEST_F(AsyncClientTests, FailedResolveWillFailTheRequest)
    {
        auto client = makeClient();

        auto req = Request<boost::beast::http::empty_body>{};
        req.method(boost::beast::http::verb::get);
        req.host("::inval");
        req.port(12345);
        req.target("/index.txt");
        req.version(11);

        std::promise<bool> awaitCompletion;
        client->request(std::move(req))
            .then([&awaitCompletion]() {
                awaitCompletion.set_value(true);
            })
            .fail([&awaitCompletion](auto e) {
                awaitCompletion.set_value(false);
            });

        EXPECT_FALSE(awaitCompletion.get_future().get());
    }

    TEST_F(AsyncClientTests, InabilityToConnectWillFailTheRequest)
    {
        auto client = makeClient();

        auto req = Request<boost::beast::http::empty_body>{};
        req.method(boost::beast::http::verb::get);
        req.host("::1");
        // hoping the port is unused:
        req.port(64250);
        req.target("/index.txt");
        req.version(11);

        std::promise<bool> awaitCompletion;
        client->request(std::move(req), std::chrono::seconds(1))
            .then([&awaitCompletion]() {
                awaitCompletion.set_value(true);
            })
            .fail([&awaitCompletion](auto e) {
                awaitCompletion.set_value(false);
            });

        EXPECT_FALSE(awaitCompletion.get_future().get());
    }

    TEST_F(AsyncClientTests, CanReadResponse)
    {
        auto client = makeClient();

        auto req = Request<boost::beast::http::empty_body>{};
        req.method(boost::beast::http::verb::get);
        req.host("::1");
        req.port(server_->getLocalEndpoint().port());
        req.target("/index.txt");
        req.version(11);

        std::promise<bool> awaitCompletion;
        client->request(std::move(req), std::chrono::seconds(5))
            .then([&awaitCompletion, client]() {
                client->readResponse<boost::beast::http::string_body>()
                    .then([&awaitCompletion](auto const&) {
                        awaitCompletion.set_value(true);
                    })
                    .fail([&awaitCompletion](auto e) {
                        awaitCompletion.set_value(false);
                    });
            })
            .fail([&awaitCompletion](auto e) {
                awaitCompletion.set_value(false);
            });

        EXPECT_TRUE(awaitCompletion.get_future().get());
    }

    TEST_F(AsyncClientTests, ResponseContainsExpectedBody)
    {
        auto client = makeClient();

        auto req = Request<boost::beast::http::empty_body>{};
        req.method(boost::beast::http::verb::get);
        req.host("::1");
        req.port(server_->getLocalEndpoint().port());
        req.target("/index.txt");
        req.version(11);

        std::promise<std::optional<std::string>> awaitCompletion;
        client->request(std::move(req), std::chrono::seconds(5))
            .then([&awaitCompletion, client]() {
                client->readResponse<boost::beast::http::string_body>()
                    .then([&awaitCompletion](auto const& resp) {
                        awaitCompletion.set_value(resp.body());
                    })
                    .fail([&awaitCompletion](auto e) {
                        awaitCompletion.set_value(std::nullopt);
                    });
            })
            .fail([&awaitCompletion](auto e) {
                awaitCompletion.set_value(std::nullopt);
            });

        auto result = awaitCompletion.get_future().get();
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(*result, "Hello");
    }

    TEST_F(AsyncClientTests, ResponseHasExpectedResponseCode)
    {
        auto client = makeClient();

        auto req = Request<boost::beast::http::empty_body>{};
        req.method(boost::beast::http::verb::get);
        req.host("::1");
        req.port(server_->getLocalEndpoint().port());
        req.target("/index.txt");
        req.version(11);

        std::promise<std::optional<boost::beast::http::status>> awaitCompletion;
        client->request(std::move(req), std::chrono::seconds(5))
            .then([&awaitCompletion, client]() {
                client->readResponse<boost::beast::http::string_body>()
                    .then([&awaitCompletion](auto const& resp) {
                        awaitCompletion.set_value(resp.result());
                    })
                    .fail([&awaitCompletion](auto e) {
                        awaitCompletion.set_value(std::nullopt);
                    });
            })
            .fail([&awaitCompletion](auto e) {
                awaitCompletion.set_value(std::nullopt);
            });

        auto result = awaitCompletion.get_future().get();
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(*result, boost::beast::http::status::ok);
    }

    TEST_F(AsyncClientTests, CanReadUsingResponseParser)
    {
        auto client = makeClient();

        auto req = Request<boost::beast::http::empty_body>{};
        req.method(boost::beast::http::verb::get);
        req.host("::1");
        req.port(server_->getLocalEndpoint().port());
        req.target("/index.txt");
        req.version(11);

        boost::beast::http::response_parser<boost::beast::http::string_body> parser{};
        std::promise<std::optional<std::string>> awaitCompletion;
        client->request(std::move(req), std::chrono::seconds(5))
            .then([&awaitCompletion, client, &parser]() {
                client->readResponse(parser)
                    .then([&awaitCompletion](auto const& parser) {
                        awaitCompletion.set_value(parser.get().body());
                    })
                    .fail([&awaitCompletion](auto e) {
                        awaitCompletion.set_value(std::nullopt);
                    });
            })
            .fail([&awaitCompletion](auto e) {
                awaitCompletion.set_value(std::nullopt);
            });

        auto result = awaitCompletion.get_future().get();
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(*result, "Hello");
    }

    TEST_F(AsyncClientTests, CanReceiveServerSentEvents)
    {
        auto client = makeClient();

        auto req = Request<boost::beast::http::empty_body>{};
        req.method(boost::beast::http::verb::get);
        req.host("::1");
        req.port(server_->getLocalEndpoint().port());
        req.target("/sse");
        req.version(11);

        std::vector<std::pair<std::string, std::string>> chunks;
        std::vector<std::string> headers;

        std::promise<bool> awaitCompletion;
        client->request(std::move(req), std::chrono::seconds(5))
            .then([&awaitCompletion, client, &chunks, &headers]() {
                client
                    ->readServerSentEvents(
                        [&chunks](auto type, auto payload) {
                            chunks.emplace_back(std::string{type}, std::string{payload});
                            return true;
                        },
                        [&awaitCompletion](auto e) {
                            if (e)
                            {
                                std::cerr << *e << std::endl;
                                awaitCompletion.set_value(false);
                            }
                            else
                                awaitCompletion.set_value(true);
                        },
                        [&headers](auto, auto head) {
                            headers.emplace_back(head);
                            return true;
                        })
                    .then([](auto& parser, auto& shallContinue) {
                        shallContinue = true;
                    })
                    .fail([&awaitCompletion](auto e) {
                        awaitCompletion.set_value(false);
                    });
            })
            .fail([&awaitCompletion](auto e) {
                awaitCompletion.set_value(false);
            });

        ASSERT_TRUE(awaitCompletion.get_future().get());
        ASSERT_EQ(chunks.size(), 2);
        ASSERT_EQ(headers.size(), 2);
        EXPECT_EQ(chunks[0].first, "message");
        EXPECT_EQ(chunks[0].second, "Hello");
        EXPECT_EQ(chunks[1].first, "message");
        EXPECT_EQ(chunks[1].second, "World");
        EXPECT_EQ(headers[0], "id: 1");
    }
}