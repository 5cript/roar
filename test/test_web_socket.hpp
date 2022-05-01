#pragma once

#include "util/common_server_setup.hpp"
#include "util/common_listeners.hpp"

#include <roar/routing/request_listener.hpp>
#include <roar/websocket/client.hpp>
#include <roar/curl/request.hpp>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <future>

namespace Roar::Tests
{
    using namespace std::literals;

    class WebSocketListener
    {
      private:
        ROAR_MAKE_LISTENER(WebSocketListener);

        ROAR_GET(wsRoute)
        ({.path = "/ws",
          .routeOptions = {
              .expectUpgrade = true,
          }});

        ROAR_GET(notWsRoute)
        ({
            .path = "/notWs",
        });

      public:
        std::shared_ptr<WebSocketSession> ws;

      private:
        BOOST_DESCRIBE_CLASS(WebSocketListener, (), (), (), (roar_wsRoute, roar_notWsRoute))
    };
    inline void WebSocketListener::wsRoute(Session& session, EmptyBodyRequest&& req)
    {
        ws = session.upgrade(req);
    }
    inline void WebSocketListener::notWsRoute(Session& session, EmptyBodyRequest&& req)
    {
        session.sendStandardResponse(boost::beast::http::status::ok);
    }

    class WebSocketTests
        : public CommonServerSetup
        , public ::testing::Test
    {
      protected:
        void SetUp() override
        {
            makeDefaultServer();
            makeSecureServer();
            listener_ = server_->installRequestListener<WebSocketListener>();
            secureServer_->installRequestListener<WebSocketListener>();
        }

      protected:
        std::shared_ptr<WebSocketListener> listener_;
    };

    TEST_F(WebSocketTests, RegularRequestsAreRejectedWhenWebSocketUpgradeIsExpected)
    {
        namespace http = boost::beast::http;
        auto res = Curl::Request{}.get(url("/ws"));
        EXPECT_EQ(res.code(), http::status::upgrade_required);
    }

    TEST_F(WebSocketTests, CanUpgradeToWebSocketSession)
    {
        std::promise<bool> synchronizer;

        WebSocketClient client{
            WebSocketClient::ConstructionArguments{.executor = server_->getExecutor(), .sslContext = std::nullopt}};
        client
            .connect({
                .host = "localhost",
                .port = std::to_string(server_->getLocalEndpoint().port()),
                .path = "/ws",
            })
            .then([&synchronizer]() {
                synchronizer.set_value(true);
            })
            .fail([&synchronizer](boost::beast::error_code ec) {
                synchronizer.set_value(false);
            });

        auto future = synchronizer.get_future();
        ASSERT_EQ(future.wait_for(std::chrono::seconds(3)), std::future_status::ready);
        EXPECT_TRUE(future.get()) << "Websocket client failed to connect.";
        EXPECT_TRUE(listener_->ws) << "Websocket session in listener was not valid.";
    }

    TEST_F(WebSocketTests, FailingToConnectCallsTheRejectionFunction)
    {
        std::promise<bool> synchronizer;

        WebSocketClient client{
            WebSocketClient::ConstructionArguments{.executor = server_->getExecutor(), .sslContext = std::nullopt}};
        client
            .connect({
                .host = "localhost",
                .port = "65000",
                .path = "/ws",
                .timeout = 3s,
            })
            .then([&synchronizer]() {
                synchronizer.set_value(true);
            })
            .fail([&synchronizer](Error const& err) {
                synchronizer.set_value(false);
            });

        auto future = synchronizer.get_future();
        ASSERT_EQ(future.wait_for(std::chrono::seconds(5)), std::future_status::ready);
        EXPECT_FALSE(future.get()) << "Websocket client failed to connect.";
    }

    TEST_F(WebSocketTests, FailingToUpgradeCallsTheRejectionFunction)
    {
        std::promise<bool> synchronizer;

        WebSocketClient client{
            WebSocketClient::ConstructionArguments{.executor = server_->getExecutor(), .sslContext = std::nullopt}};
        client
            .connect({
                .host = "localhost",
                .port = std::to_string(server_->getLocalEndpoint().port()),
                .path = "/",
            })
            .then([&synchronizer]() {
                synchronizer.set_value(true);
            })
            .fail([&synchronizer](Error const& err) {
                synchronizer.set_value(false);
            });

        auto future = synchronizer.get_future();
        ASSERT_EQ(future.wait_for(std::chrono::seconds(5)), std::future_status::ready);
        EXPECT_FALSE(future.get()) << "Websocket client failed to connect.";
    }

    TEST_F(WebSocketTests, HttpResponseCallsTheRejectionFunction)
    {
        std::promise<bool> synchronizer;

        WebSocketClient client{
            WebSocketClient::ConstructionArguments{.executor = server_->getExecutor(), .sslContext = std::nullopt}};
        client
            .connect({
                .host = "localhost",
                .port = std::to_string(server_->getLocalEndpoint().port()),
                .path = "/",
            })
            .then([&synchronizer]() {
                synchronizer.set_value(true);
            })
            .fail([&synchronizer](Error const& err) {
                synchronizer.set_value(false);
            });

        auto future = synchronizer.get_future();
        ASSERT_EQ(future.wait_for(std::chrono::seconds(5)), std::future_status::ready);
        EXPECT_FALSE(future.get()) << "Websocket client failed to connect.";
    }

    TEST_F(WebSocketTests, CanSendStringToServer)
    {
        std::string recv;
        std::promise<bool> synchronizer;

        WebSocketClient client{
            WebSocketClient::ConstructionArguments{.executor = server_->getExecutor(), .sslContext = std::nullopt}};
        client
            .connect({
                .host = "localhost",
                .port = std::to_string(server_->getLocalEndpoint().port()),
                .path = "/ws",
            })
            .then([&synchronizer, &client, &recv, this]() {
                listener_->ws->read().then([&recv, &synchronizer](std::string const& msg) {
                    recv = msg;
                    synchronizer.set_value(true);
                });
                client.send("Hello You!");
            });

        auto future = synchronizer.get_future();
        ASSERT_EQ(future.wait_for(std::chrono::seconds(10)), std::future_status::ready);
        EXPECT_EQ(recv, "Hello You!");
    }
}