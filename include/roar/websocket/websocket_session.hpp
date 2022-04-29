#pragma once

#include <roar/detail/pimpl_special_functions.hpp>

#include <boost/beast/http/empty_body.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <boost/beast/core/tcp_stream.hpp>

#include <memory>
#include <variant>

namespace Roar
{
    template <typename>
    class Request;

    /**
     * @brief A WebsocketSession allows you to interact with a client via websocket protocol.
     */
    class WebsocketSession : public std::enable_shared_from_this<WebsocketSession>
    {
      public:
        WebsocketSession(
            std::variant<boost::beast::tcp_stream, boost::beast::ssl_stream<boost::beast::tcp_stream>>&& stream);
        ROAR_PIMPL_SPECIAL_FUNCTIONS(WebsocketSession);

        /**
         * @brief Accept an upgrade request and performs the necessary websocket handshake.
         *
         * @param req An http->ws upgrade request.
         */
        void accept(Request<boost::beast::http::empty_body> const& req);

      private:
        struct Implementation;
        std::unique_ptr<Implementation> impl_;
    };
}