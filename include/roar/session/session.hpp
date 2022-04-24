#pragma once

#include <roar/error.hpp>
#include <roar/websocket/websocket_session.hpp>
#include <roar/response.hpp>
#include <roar/beast/forward.hpp>
#include <roar/detail/pimpl_special_functions.hpp>
#include <roar/detail/literals/memory.hpp>

#include <boost/beast/http/message.hpp>
#include <boost/beast/http/empty_body.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <boost/beast/core/tcp_stream.hpp>

#include <memory>
#include <optional>
#include <chrono>
#include <variant>

namespace Roar
{
    class Router;
    template <typename>
    class Request;
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
        class ReadIntermediate
        {
          private:
            friend Session;

            template <typename... Forwards>
            ReadIntermediate(Session& session, Forwards&&... forwardArgs)
                : session_{session}
                , req_{[&]() -> decltype(req_) {
                    if constexpr (std::is_same_v<BodyT, boost::beast::http::empty_body>)
                        return std::move(session.parser());
                    else
                        return std::make_shared<boost::beast::http::request_parser<BodyT>>(
                            std::move(*session.parser()), std::forward<Forwards>(forwardArgs)...);
                }()}
            {
                req_->body_limit(defaultBodyLimit);
            }

          public:
            ReadIntermediate& bodyLimit(std::size_t limit)
            {
                req_->body_limit(limit);
                return *this;
            }

            ReadIntermediate& bodyLimit(boost::beast::string_view limit)
            {
                req_->body_limit(std::stoull(std::string{limit}));
                return *this;
            }

            ReadIntermediate& noBodyLimit()
            {
                req_->body_limit(boost::none);
                return *this;
            }

            template <typename... Forwards>
            void start(std::function<void(Session& session, Request<BodyT>&&)> onReadComplete)
            {
                session_.withStreamDo([this, &onReadComplete](auto& stream) {
                    boost::beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(sessionTimeout));
                    boost::beast::http::async_read(
                        stream,
                        session_.buffer(),
                        *req_,
                        [session = session_.shared_from_this(), req = req_, onReadComplete = std::move(onReadComplete)](
                            boost::beast::error_code ec, std::size_t) {
                            if (ec)
                                return session->close();

                            onReadComplete(*session, req->release());
                        });
                });
            }

          private:
            Session& session_;
            std::shared_ptr<boost::beast::http::request_parser<BodyT>> req_;
        };

        template <typename BodyT, typename... Forwards>
        ReadIntermediate<BodyT> read(Forwards&&... forwardArgs)
        {
            return ReadIntermediate<BodyT>{*this, std::forward<Forwards>(forwardArgs)...};
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

        std::shared_ptr<WebsocketSession> upgrade(Request<boost::beast::http::empty_body> const& req);

      private:
        void readHeader();
        void performSslHandshake();
        void onWriteComplete(bool expectsClose, boost::beast::error_code ec, std::size_t bytesTransferred);
        std::variant<boost::beast::tcp_stream, boost::beast::ssl_stream<boost::beast::tcp_stream>>& stream();
        std::shared_ptr<boost::beast::http::request_parser<boost::beast::http::empty_body>>& parser();
        boost::beast::flat_buffer& buffer();

      private:
        struct Implementation;
        std::unique_ptr<Implementation> impl_;
    };
}