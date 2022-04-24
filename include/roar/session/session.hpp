#pragma once

#include <roar/error.hpp>
#include <roar/response.hpp>
#include <roar/beast/forward.hpp>
#include <roar/detail/pimpl_special_functions.hpp>
#include <roar/detail/literals/memory.hpp>

#include <boost/beast/http/message.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <boost/beast/core/tcp_stream.hpp>

#include <memory>
#include <optional>
#include <chrono>
#include <variant>

namespace Roar
{
    class Router;
}

namespace Roar::Session
{
    class Session : public std::enable_shared_from_this<Session>
    {
      public:
        constexpr static uint64_t defaultHeaderLimit{8_MiB};
        constexpr static uint64_t defaultBodyLimit{8_MiB};
        constexpr static std::chrono::seconds sessionTimeout{10};

        Session(
            boost::asio::basic_stream_socket<boost::asio::ip::tcp>&& socket,
            boost::beast::basic_flat_buffer<std::allocator<char>>&& buffer,
            std::optional<boost::asio::ssl::context>& sslContext,
            bool isSecure,
            std::function<void(Error&&)> onError,
            std::weak_ptr<Router> router);
        ROAR_PIMPL_SPECIAL_FUNCTIONS(Session);

        void startup();
        void close();

        template <typename BodyT>
        void send(boost::beast::http::response<BodyT>&& response)
        {
            withStreamDo([this, &response](auto& stream) {
                auto res = std::make_shared<boost::beast::http::response<BodyT>>(std::move(response));
                boost::beast::http::async_write(
                    stream,
                    *res,
                    [self = shared_from_this(),
                     res = std::move(res)](boost::beast::error_code ec, std::size_t bytesTransferred) {
                        self->onWriteComplete(res->need_eof(), ec, bytesTransferred);
                    });
            });
        }

        template <typename BodyT>
        static Response<BodyT> prepareResponse()
        {
            return Response<BodyT>{};
        }

        template <typename FunctionT>
        void withStreamDo(FunctionT&& func)
        {
            std::visit(std::forward<FunctionT>(func), stream());
        }

      private:
        void readHeader();
        void performSslHandshake();
        void onWriteComplete(bool expectsClose, boost::beast::error_code ec, std::size_t bytesTransferred);
        std::variant<boost::beast::tcp_stream, boost::beast::ssl_stream<boost::beast::tcp_stream>>& stream();

      private:
        struct Implementation;
        std::unique_ptr<Implementation> impl_;
    };
}