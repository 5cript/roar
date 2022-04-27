#pragma once

#include <roar/error.hpp>
#include <roar/websocket/websocket_session.hpp>
#include <roar/response.hpp>
#include <roar/beast/forward.hpp>
#include <roar/detail/pimpl_special_functions.hpp>
#include <roar/detail/literals/memory.hpp>
#include <roar/routing/proto_route.hpp>
#include <roar/standard_response_provider.hpp>

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
#include <stdexcept>

namespace Roar
{
    class Router;
    template <typename>
    class Request;
    class Session : public std::enable_shared_from_this<Session>
    {
      public:
        friend class Route;
        friend class Factory;

        constexpr static uint64_t defaultHeaderLimit{8_MiB};
        constexpr static uint64_t defaultBodyLimit{8_MiB};
        constexpr static std::chrono::seconds sessionTimeout{10};

        Session(
            boost::asio::basic_stream_socket<boost::asio::ip::tcp>&& socket,
            boost::beast::basic_flat_buffer<std::allocator<char>>&& buffer,
            std::optional<boost::asio::ssl::context>& sslContext,
            bool isSecure,
            std::function<void(Error&&)> onError,
            std::weak_ptr<Router> router,
            std::shared_ptr<const StandardResponseProvider> standardResponseProvider);
        ROAR_PIMPL_SPECIAL_FUNCTIONS(Session);

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
        void send(Response<BodyT>&& response)
        {
            std::move(response).send(*this);
        }

        template <typename BodyT>
        class ReadIntermediate : public std::enable_shared_from_this<ReadIntermediate<BodyT>>
        {
          private:
            friend Session;

            template <typename OriginalBodyT, typename... Forwards>
            ReadIntermediate(Session& session, Request<OriginalBodyT> req, Forwards&&... forwardArgs)
                : session_{session.shared_from_this()}
                , req_{[&]() -> decltype(req_) {
                    if constexpr (std::is_same_v<BodyT, boost::beast::http::empty_body>)
                        throw std::runtime_error("Attempting to read with empty_body type.");
                    else
                        return boost::beast::http::request_parser<BodyT>{
                            std::move(*session.parser()), std::forward<Forwards>(forwardArgs)...};
                    session.parser().reset();
                }()}
                , onReadComplete_{}
                , originalExtensions_{std::move(req).ejectExtensions()}
            {
                req_.body_limit(defaultBodyLimit);
            }

          public:
            ReadIntermediate(ReadIntermediate&&) = default;
            ReadIntermediate& operator=(ReadIntermediate&&) = default;

            ReadIntermediate& bodyLimit(std::size_t limit)
            {
                req_.body_limit(limit);
                return *this;
            }

            ReadIntermediate& bodyLimit(boost::beast::string_view limit)
            {
                req_.body_limit(std::stoull(std::string{limit}));
                return *this;
            }

            ReadIntermediate& noBodyLimit()
            {
                req_.body_limit(boost::none);
                return *this;
            }

            void readChunk()
            {
                session_->withStreamDo([this](auto& stream) {
                    boost::beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(sessionTimeout));
                    boost::beast::http::async_read_some(
                        stream,
                        session_->buffer(),
                        req_,
                        [self = this->shared_from_this()](boost::beast::error_code ec, std::size_t) {
                            if (!ec && !self->req_.is_done())
                                return self->readChunk();

                            if (ec)
                                return self->session_->close();

                            auto request = Request<BodyT>(self->req_.release(), std::move(self->originalExtensions_));
                            try
                            {
                                self->onReadComplete_(*self->session_, request);
                            }
                            catch (std::exception const& exc)
                            {
                                using namespace std::string_literals;
                                self->session_->send(self->session_->standardResponseProvider().makeStandardResponse(
                                    *self->session_,
                                    boost::beast::http::status::internal_server_error,
                                    "An exception was thrown in the body read completion handler: "s + exc.what()));
                            }
                        });
                });
            }

            template <typename... Forwards>
            void start(std::function<void(Session& session, Request<BodyT> const&)> onReadComplete)
            {
                onReadComplete_ = std::move(onReadComplete);
                readChunk();
            }

          private:
            std::shared_ptr<Session> session_;
            boost::beast::http::request_parser<BodyT> req_;
            std::function<void(Session& session, Request<BodyT> const&)> onReadComplete_;
            Detail::RequestExtensions originalExtensions_;
        };

        template <typename BodyT, typename OriginalBodyT, typename... Forwards>
        [[nodiscard]] std::shared_ptr<ReadIntermediate<BodyT>>
        read(Request<OriginalBodyT> req, Forwards&&... forwardArgs)
        {
            return std::shared_ptr<ReadIntermediate<BodyT>>(
                new ReadIntermediate<BodyT>{*this, std::move(req), std::forward<Forwards>(forwardArgs)...});
        }

        /**
         * @brief Prepares a response with some header values already set.
         *
         * @tparam BodyT
         * @return Response<BodyT>
         */
        template <typename BodyT = boost::beast::http::empty_body, typename RequestBodyT>
        [[nodiscard]] Response<BodyT> prepareResponse(Request<RequestBodyT> const& req)
        {
            auto res = Response<BodyT>{};
            if (routeOptions().cors)
                res.enableCors(req, routeOptions().cors);
            return res;
        }

        /**
         * @brief std::visit for the underlying beast stream. Either ssl_stream or tcp_stream.
         *
         * @tparam FunctionT
         * @param func
         */
        template <typename FunctionT>
        void withStreamDo(FunctionT&& func)
        {
            std::visit(std::forward<FunctionT>(func), stream());
        }

        /**
         * @brief Turn this session into a websocket session. Will return an invalid shared_ptr if this is not an
         * upgrade request.
         *
         * @param req The request to perform this upgrade from.
         * @return std::shared_ptr<WebsocketSession> A websocket session or an invalid shared_ptr.
         */
        [[nodiscard]] std::shared_ptr<WebsocketSession> upgrade(Request<boost::beast::http::empty_body> const& req);

        /**
         * @brief Retrieve the options defined by the request listener class for this route.
         *
         * @return RouteOptions const& The route options.
         */
        [[nodiscard]] RouteOptions const& routeOptions();

      private:
        void setupRouteOptions(RouteOptions options);

      private:
        void readHeader();
        void performSslHandshake();
        void onWriteComplete(bool expectsClose, boost::beast::error_code ec, std::size_t bytesTransferred);
        std::variant<boost::beast::tcp_stream, boost::beast::ssl_stream<boost::beast::tcp_stream>>& stream();
        std::shared_ptr<boost::beast::http::request_parser<boost::beast::http::empty_body>>& parser();
        boost::beast::flat_buffer& buffer();
        StandardResponseProvider const& standardResponseProvider();
        void startup();

      private:
        struct Implementation;
        std::unique_ptr<Implementation> impl_;
    };
}