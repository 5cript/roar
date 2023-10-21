#pragma once

#include <roar/client.hpp>
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

        std::shared_ptr<Client> makeClient(std::string const& scheme = "http")
        {
            if (scheme == "http")
                return std::make_shared<Client>(Client::ConstructionArguments{.executor = executor_});
            else if (scheme == "https")
            {
                return std::make_shared<Client>(Client::ConstructionArguments{
                    .executor = executor_,
                    .sslContext = boost::asio::ssl::context{boost::asio::ssl::context::tlsv12_client},
                });
            }
            else
                throw std::runtime_error{"Unknown scheme: " + scheme};
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

    // TEST_F(AsyncClientTests, CanReceiveServerSentEvents)
    // {
    //     auto client = makeClient("http");
    //     std::string host = "::1";
    //     // std::string host = "sse.dev";

    //     std::cout << server_->getLocalEndpoint().port() << std::endl;
    //     // std::this_thread::sleep_for(std::chrono::seconds(100));

    //     auto req = Request<boost::beast::http::empty_body>{};
    //     req.method(boost::beast::http::verb::get);

    //     req.host(host);
    //     req.target("/sse");
    //     req.port(server_->getLocalEndpoint().port());
    //     // req.host(host);
    //     // req.target("/test");
    //     // req.port(443);

    //     req.setHeader(boost::beast::http::field::cache_control, "no-cache");
    //     req.setHeader(boost::beast::http::field::host, host);
    //     req.setHeader(boost::beast::http::field::accept, "text/event-stream");
    //     req.setHeader(boost::beast::http::field::connection, "keep-alive");
    //     req.version(11);

    //     std::vector<std::pair<std::string, std::string>> chunks;

    //     std::promise<bool> awaitCompletion;
    //     client->request(std::move(req), std::chrono::seconds(5))
    //         .then([&awaitCompletion, client, &chunks]() {
    //             client
    //                 ->readServerSentEvents(
    //                     [&chunks, client](auto type, auto payload) {
    //                         std::cout << "chunk: " << type << " " << payload << std::endl;
    //                         // chunks.emplace_back(std::string{type}, std::string{payload});
    //                         // if (chunks.size() >= 2)
    //                         //     return false;
    //                         return true;
    //                     },
    //                     [&awaitCompletion](auto e) {
    //                         if (e)
    //                         {
    //                             std::cerr << *e << std::endl;
    //                             awaitCompletion.set_value(false);
    //                         }
    //                         else
    //                             awaitCompletion.set_value(true);
    //                     })
    //                 .then([](auto& parser, auto& shallContinue) {
    //                     std::cout << parser.get().result_int() << "\n";
    //                     shallContinue = true;
    //                 })
    //                 .fail([&awaitCompletion](auto e) {
    //                     std::cerr << e << std::endl;
    //                     awaitCompletion.set_value(false);
    //                 });
    //         })
    //         .fail([&awaitCompletion](auto e) {
    //             std::cerr << e << std::endl;
    //             awaitCompletion.set_value(false);
    //         });

    //     ASSERT_TRUE(awaitCompletion.get_future().get());
    //     ASSERT_EQ(chunks.size(), 2);
    //     EXPECT_EQ(chunks[0].first, "message");
    //     EXPECT_EQ(chunks[0].second, "Hello");
    //     EXPECT_EQ(chunks[1].first, "message");
    //     EXPECT_EQ(chunks[1].second, "World");
    // }
}