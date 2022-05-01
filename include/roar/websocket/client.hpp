#pragma once

#include <roar/error.hpp>
#include <roar/detail/pimpl_special_functions.hpp>
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
    class WebSocketClient
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

        WebSocketClient(ConstructionArguments&& args);
        ROAR_PIMPL_SPECIAL_FUNCTIONS(WebSocketClient);

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

            // Additional WebSocket handshake headers.
            std::unordered_map<boost::beast::http::field, std::string> headers = {};
        };

        /**
         * @brief Connects the websocket to a server.
         *
         * @param connectParameters see ConnectParameters
         * @return promise::Promise Promise::then is called with no parameter, Promise::fail with a
         * "Roar::Error".
         */
        promise::Promise connect(ConnectParameters&& connectParameters);

        /**
         * @brief Sends a string to the server.
         *
         * @param message A message to send.
         * @return promise::Promise Promise::then is called with the amount of bytes sent, Promise::fail with a
         * Roar::Error.
         */
        promise::Promise send(std::string message);

        /**
         * @brief Reads something from the server.
         *
         * @return promise::Promise Promise::then is called with a std::string, Promise::fail with
         * a Roar::Error.
         */
        promise::Promise read();

        /**
         * @brief Closes the websocket connection.
         */
        void close();

      private:
        struct Implementation;
        std::shared_ptr<Implementation> impl_;
    };
}