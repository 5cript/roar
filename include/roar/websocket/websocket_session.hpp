#pragma once

#include <roar/detail/pimpl_special_functions.hpp>
#include <roar/detail/shared_from_base.hpp>
#include <roar/websocket/websocket_base.hpp>
#include <roar/detail/stream_type.hpp>

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
     * @brief A WebsocketSession allows you to interact with a client via websocket protocol.
     */
    class WebsocketSession : public Detail::SharedFromBase<WebsocketBase, WebsocketSession>
    {
      public:
        WebsocketSession(std::variant<Detail::StreamType, boost::beast::ssl_stream<Detail::StreamType>>&& stream);
        ROAR_PIMPL_SPECIAL_FUNCTIONS_NO_MOVE(WebsocketSession);

        /**
         * @brief Accept an upgrade request and performs the necessary websocket handshake.
         *
         * @param req An http->ws upgrade request.
         */
        Detail::PromiseTypeBind<Detail::PromiseTypeBindThen<>, Detail::PromiseTypeBindFail<Error const&>>
        accept(Request<boost::beast::http::empty_body> const& req);

      private:
        struct Implementation;
        std::unique_ptr<Implementation> impl_;
    };
}