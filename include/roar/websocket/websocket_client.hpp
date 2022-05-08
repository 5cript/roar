#pragma once

#include "roar/detail/promise_compat.hpp"
#include <roar/detail/pimpl_special_functions.hpp>
#include <roar/detail/shared_from_base.hpp>
#include <roar/websocket/websocket_base.hpp>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/beast/http/field.hpp>
#include <promise-cpp/promise.hpp>

#include <memory>
#include <optional>
#include <chrono>
#include <unordered_map>

namespace Roar
{
    class WebsocketClient : public Detail::SharedFromBase<WebsocketBase, WebsocketClient>
    {
      public:
        constexpr static std::chrono::seconds defaultTimeout{10};

        struct ConstructionArguments
        {
            /// Required io executor for boost::asio.
            boost::asio::any_io_executor executor;

            /// Supply for SSL support.
            std::optional<boost::asio::ssl::context> sslContext;
        };

        WebsocketClient(ConstructionArguments&& args);
        ~WebsocketClient();
        WebsocketClient(WebsocketClient&&) = delete;
        WebsocketClient& operator=(WebsocketClient&&) = delete;
        WebsocketClient(WebsocketClient const&) = delete;
        WebsocketClient& operator=(WebsocketClient const&) = delete;

        struct ConnectParameters
        {
            /// The host to connect to.
            std::string const& host;

            /// The port to connect
            std::string const& port;

            /// The path to connect to.
            std::string path = "/";

            /// A connect and ssl handshake timeout.
            std::chrono::seconds timeout = defaultTimeout;

            // Additional Websocket handshake headers.
            std::unordered_map<boost::beast::http::field, std::string> headers = {};
        };
        /**
         * @brief Connects the websocket to a server.
         *
         * @param connectParameters see ConnectParameters
         * @return A promise to continue after the connect.
         */
        Detail::PromiseTypeBind<Detail::PromiseTypeBindThen<>, Detail::PromiseTypeBindFail<Error>>
        connect(ConnectParameters&& connectParameters);

      private:
        struct Implementation;
        std::unique_ptr<Implementation> impl_;
    };
}