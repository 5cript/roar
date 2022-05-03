#pragma once

#include <roar/detail/pimpl_special_functions.hpp>

#include <boost/beast/http/empty_body.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <promise-cpp/promise.hpp>

#include <memory>
#include <variant>

namespace Roar
{
    template <typename>
    class Request;

    /**
     * @brief A WebSocketSession allows you to interact with a client via websocket protocol.
     */
    class WebSocketSession : public std::enable_shared_from_this<WebSocketSession>
    {
      public:
        WebSocketSession(
            std::variant<boost::beast::tcp_stream, boost::beast::ssl_stream<boost::beast::tcp_stream>>&& stream);
        ROAR_PIMPL_SPECIAL_FUNCTIONS(WebSocketSession);

        /**
         * @brief Accept an upgrade request and performs the necessary websocket handshake.
         *
         * @param req An http->ws upgrade request.
         */
        promise::Promise accept(Request<boost::beast::http::empty_body> const& req);

        /**
         * @brief Sends a string to the client.
         *
         * @param message A message to send.
         * @return promise::Promise Promise::then is called with the amount of bytes sent, Promise::fail with a
         * Roar::Error.
         */
        promise::Promise send(std::string message);

        /**
         * @brief Reads something from the client.
         *
         * @return promise::Promise Promise::then is called with {beast::flat_buffer, bytesReveived}, Promise::fail with
         * a Roar::Error.
         */
        promise::Promise read();

        /**
         * @brief Closes the websocket session.
         */
        void close();

      private:
        struct Implementation;
        std::unique_ptr<Implementation> impl_;
    };
}