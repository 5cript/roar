#pragma once

#include "util/common_server_setup.hpp"
#include "util/common_listeners.hpp"

#include <roar/routing/request_listener.hpp>
#include <roar/websocket/websocket_client.hpp>
#include <roar/curl/request.hpp>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <future>

namespace Roar::Tests
{
    using namespace std::literals;

    class WebSocketListener : public std::enable_shared_from_this<WebSocketListener>
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
        std::function<void()> onAccept;

      private:
        BOOST_DESCRIBE_CLASS(WebSocketListener, (), (), (), (roar_wsRoute, roar_notWsRoute))
    };
    inline void WebSocketListener::wsRoute(Session& session, EmptyBodyRequest&& req)
    {
        session.upgrade(req).then([weak = weak_from_this()](std::shared_ptr<WebSocketSession> ws) {
            auto self = weak.lock();
            if (!self)
                return;

            self->ws = ws;
            if (self->onAccept)
                self->onAccept();
        });
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
        std::promise<void> synchronizer2;

        listener_->onAccept = [&synchronizer2]() {
            synchronizer2.set_value();
        };

        auto client = std::make_shared<WebSocketClient>(
            WebSocketClient::ConstructionArguments{.executor = server_->getExecutor(), .sslContext = std::nullopt});
        client
            ->connect({
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
        auto future2 = synchronizer2.get_future();
        ASSERT_EQ(future.wait_for(std::chrono::seconds(10)), std::future_status::ready);
        ASSERT_EQ(future2.wait_for(std::chrono::seconds(10)), std::future_status::ready);
        EXPECT_TRUE(future.get()) << "Websocket client failed to connect.";
        EXPECT_TRUE(listener_->ws) << "Websocket session in listener was not valid.";
    }

    TEST_F(WebSocketTests, FailingToConnectCallsTheRejectionFunction)
    {
        std::promise<bool> synchronizer;

        auto client = std::make_shared<WebSocketClient>(
            WebSocketClient::ConstructionArguments{.executor = server_->getExecutor(), .sslContext = std::nullopt});
        client
            ->connect({
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

        auto client = std::make_shared<WebSocketClient>(
            WebSocketClient::ConstructionArguments{.executor = server_->getExecutor(), .sslContext = std::nullopt});
        client
            ->connect({
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
        ASSERT_EQ(future.wait_for(std::chrono::seconds(10)), std::future_status::ready);
        EXPECT_FALSE(future.get()) << "Websocket client failed to connect.";
    }

    TEST_F(WebSocketTests, HttpResponseCallsTheRejectionFunction)
    {
        std::promise<bool> synchronizer;

        auto client = std::make_shared<WebSocketClient>(
            WebSocketClient::ConstructionArguments{.executor = server_->getExecutor(), .sslContext = std::nullopt});
        client
            ->connect({
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
        ASSERT_EQ(future.wait_for(std::chrono::seconds(10)), std::future_status::ready);
        EXPECT_FALSE(future.get()) << "Websocket client failed to connect.";
    }

    TEST_F(WebSocketTests, CanSendStringToServer)
    {
        std::string recv;
        std::promise<bool> synchronizer;

        listener_->onAccept = [&, this]() {
            listener_->ws->read().then([&](auto const& msg) {
                recv = msg.message;
                synchronizer.set_value(true);
            });
        };

        auto client = std::make_shared<WebSocketClient>(
            WebSocketClient::ConstructionArguments{.executor = server_->getExecutor(), .sslContext = std::nullopt});
        client
            ->connect({
                .host = "localhost",
                .port = std::to_string(server_->getLocalEndpoint().port()),
                .path = "/ws",
            })
            .then([&synchronizer, client, &recv, this]() {
                client->send("Hello You!");
            });

        auto future = synchronizer.get_future();
        ASSERT_EQ(future.wait_for(std::chrono::seconds(10)), std::future_status::ready);
        EXPECT_EQ(recv, "Hello You!");
        EXPECT_EQ(future.get(), true);
    }
}