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

    class WebsocketSession : public std::enable_shared_from_this<WebsocketSession>
    {
      public:
        WebsocketSession(
            std::variant<boost::beast::tcp_stream, boost::beast::ssl_stream<boost::beast::tcp_stream>>&& stream);
        ROAR_PIMPL_SPECIAL_FUNCTIONS(WebsocketSession);

        void accept(Request<boost::beast::http::empty_body> const& req);

      private:
        struct Implementation;
        std::unique_ptr<Implementation> impl_;
    };
}