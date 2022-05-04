#pragma once

#include <functional>
#include <roar/error.hpp>
#include <roar/websocket/websocket_session.hpp>
#include <roar/response.hpp>
#include <roar/beast/forward.hpp>
#include <roar/detail/pimpl_special_functions.hpp>
#include <roar/detail/literals/memory.hpp>
#include <roar/routing/proto_route.hpp>
#include <roar/standard_response_provider.hpp>
#include <roar/detail/promise_compat.hpp>
#include <roar/body/void_body.hpp>
#include <roar/detail/stream_type.hpp>

#include <boost/beast/http/message.hpp>
#include <boost/beast/http/empty_body.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <promise-cpp/promise.hpp>

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

        /**
         * @brief Calls shutdown on the socket.
         */
        void close();

        /**
         * @brief Helper class to prepare and send a response.
         */
        template <typename BodyT>
        class SendIntermediate : public std::enable_shared_from_this<SendIntermediate<BodyT>>
        {
          public:
            template <typename OriginalBodyT>
            SendIntermediate(Session& session, Request<OriginalBodyT> const& req)
                : session_(session.shared_from_this())
                , response_{session_->prepareResponse<BodyT>(req)}
            {}
            SendIntermediate(SendIntermediate&&) = default;
            SendIntermediate(SendIntermediate const&) = delete;
            SendIntermediate& operator=(SendIntermediate&&) = default;
            SendIntermediate& operator=(SendIntermediate const&) = delete;

            /**
             * @brief Sets the response status code.
             *
             * @param status A status code.
             */
            SendIntermediate& status(boost::beast::http::status status)
            {
                response_.status(status);
                return *this;
            }

            /**
             * @brief Retrieve the response body object.
             *
             * @return auto&
             */
            auto& body()
            {
                return response_.body();
            }

            /**
             * @brief This function can be used to assign something to the body.
             *
             * @tparam T
             * @param toAssign
             * @return Response& Returned for chaining.
             */
            template <typename T>
            SendIntermediate& body(T&& toAssign)
            {
                response_.body() = std::forward<T>(toAssign);
                return *this;
            }

            /**
             * @brief (De)Activate chunked encoding.
             *
             * @param activate
             * @return Response& Returned for chaining.
             */
            SendIntermediate& chunked(bool activate = true)
            {
                response_.chunked(activate);
                return *this;
            }

            /**
             * @brief For setting of the content type.
             *
             * @param type
             * @return Response& Returned for chaining.
             */
            template <typename T>
            SendIntermediate& contentType(T&& type)
            {
                response_.contentType(std::forward<T>(type));
                return *this;
            }

            /**
             * @brief Can be used to set a header field.
             *
             * @return Response& Returned for chaining.
             */
            template <typename T>
            SendIntermediate& setHeader(boost::beast::http::field field, T&& value)
            {
                response_.setHeader(field, std::forward<T>(value));
                return *this;
            }

            /**
             * @brief Sets cors headers.
             * @param req A request to base the cors headers off of.
             * @param cors A cors settings object. If not supplied, a very permissive setting is used.
             *
             * @return Response& Returned for chaining.
             */
            template <typename RequestBodyT>
            SendIntermediate&
            enableCors(Request<RequestBodyT> const& req, std::optional<CorsSettings> cors = std::nullopt)
            {
                response_.enableCors(req, std::move(cors));
                return *this;
            }

            /**
             * @brief Sends the response and invalidates this object
             */
            promise::Promise commit()
            {
                response_.preparePayload();
                return session_->send(std::move(response_));
            }

          private:
            std::shared_ptr<Session> session_;
            Response<BodyT> response_;
        };

        template <typename BodyT, typename OriginalBodyT>
        [[nodiscard]] std::shared_ptr<SendIntermediate<BodyT>> send(Request<OriginalBodyT> const& req)
        {
            return std::shared_ptr<SendIntermediate<BodyT>>(new SendIntermediate<BodyT>{*this, req});
        }

        /**
         * @brief Send a boost::beast::http response to the client.
         *
         * @tparam BodyT Body type of the response.
         * @param response A response object.
         * @return Returns a promise that resolves with whether or not the connection was auto-closed.
         */
        template <typename BodyT>
        promise::Promise send(boost::beast::http::response<BodyT>&& response)
        {
            return promise::newPromise([&, this](promise::Defer d) {
                withStreamDo([this, &response, &d](auto& stream) {
                    auto res = std::make_shared<boost::beast::http::response<BodyT>>(std::move(response));
                    boost::beast::http::async_write(
                        stream,
                        *res,
                        [self = shared_from_this(), res = std::move(res), d](
                            boost::beast::error_code ec, std::size_t bytesTransferred) {
                            auto wasClosed = self->onWriteComplete(res->need_eof(), ec, bytesTransferred);
                            if (!ec)
                                d.resolve(wasClosed);
                            else
                                d.reject(Error{.error = ec, .additionalInfo = "Failed to send response"});
                        });
                });
            });
        }

        /**
         * @brief Send a response to the client.
         *
         * @tparam BodyT Body type of the response.
         * @param response A response object.
         */
        template <typename BodyT>
        promise::Promise send(Response<BodyT>&& response)
        {
            return std::move(response).send(*this);
        }

        /**
         * @brief Utility class to build up read operations.
         *
         * @tparam BodyT The body type of this read.
         */
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
                , originalExtensions_{std::move(req).ejectExtensions()}
                , onChunk_{}
                , promise_{}
                , overallTimeout_{std::nullopt}
            {
                req_.body_limit(defaultBodyLimit);
            }

          public:
            ReadIntermediate(ReadIntermediate&&) = default;
            ReadIntermediate& operator=(ReadIntermediate&&) = default;

            /**
             * @brief Set a body limit.
             * @param The limit as an std::size_t
             *
             * @return ReadIntermediate& Returns itself for chaining.
             */
            ReadIntermediate& bodyLimit(std::size_t limit)
            {
                req_.body_limit(limit);
                return *this;
            }

            /**
             * @brief Set a body limit.
             * @param The limit as a string. Must be a number.
             *
             * @return ReadIntermediate& Returns itself for chaining.
             */
            ReadIntermediate& bodyLimit(boost::beast::string_view limit)
            {
                req_.body_limit(std::stoull(std::string{limit}));
                return *this;
            }

            /**
             * @brief Accept a body of any length.
             *
             * @return ReadIntermediate& Returns itself for chaining.
             */
            ReadIntermediate& noBodyLimit()
            {
                req_.body_limit(boost::none);
                return *this;
            }

            /**
             * @brief Set a callback function that is called whenever some data was read.
             *
             * @param onChunkFunc The function to be called. Is called with the buffer and amount of bytes transferred.
             * Return false in this function to stop reading.
             * @return ReadIntermediate& Returns itself for chaining.
             */
            ReadIntermediate& onReadSome(std::function<bool(boost::beast::flat_buffer&, std::size_t)> onChunkFunc)
            {
                onChunk_ = onChunkFunc;
                return *this;
            }

            /**
             * @brief Start reading here. If you dont call this function, nothing is read.
             *
             * @return Promise that can be used to await the read. Promise is called with <Session& session,
             * Request<BodyT> req>.
             */
            Detail::PromiseTypeBind<
                Detail::PromiseTypeBindThen<
                    Detail::PromiseReferenceWrap<Session>,
                    Detail::PromiseReferenceWrap<Request<BodyT> const>>,
                Detail::PromiseTypeBindFail<Error const&>>
            commit()
            {
                if (overallTimeout_)
                {
                    session_->withStreamDo([this](auto& stream) {
                        boost::beast::get_lowest_layer(stream).expires_after(*overallTimeout_);
                    });
                }
                promise_ = std::make_unique<promise::Promise>(promise::newPromise());
                readChunk();
                return {*promise_};
            }

            /**
             * @brief Set a timeout for the whole read operation.
             *
             * @param timeout The timeout as a std::chrono::duration.
             * @return ReadIntermediate& Returns itself for chaining.
             */
            ReadIntermediate& useFixedTimeout(std::chrono::milliseconds timeout)
            {
                overallTimeout_ = timeout;
                return *this;
            }

          private:
            /**
             * @brief Reads some bytes off of the stream.
             */
            void readChunk()
            {
                session_->withStreamDo([this](auto& stream) {
                    if (!overallTimeout_)
                        boost::beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(sessionTimeout));
                    boost::beast::http::async_read_some(
                        stream,
                        session_->buffer(),
                        req_,
                        [self = this->shared_from_this()](boost::beast::error_code ec, std::size_t bytesReceived) {
                            if (!ec && !self->req_.is_done())
                                return self->readChunk();

                            if (ec)
                            {
                                self->session_->close();
                                self->promise_->reject(Error{.error = ec});
                            }

                            if (self->onChunk_ && !self->onChunk_(self->session_->buffer(), bytesReceived))
                                return;

                            auto request = Request<BodyT>(self->req_.release(), std::move(self->originalExtensions_));
                            try
                            {
                                self->promise_->template resolve(Detail::ref(*self->session_), Detail::cref(request));
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

          private:
            std::shared_ptr<Session> session_;
            boost::beast::http::request_parser<BodyT> req_;
            Detail::RequestExtensions originalExtensions_;
            std::function<bool(boost::beast::flat_buffer&, std::size_t)> onChunk_;
            std::unique_ptr<promise::Promise> promise_;
            std::optional<std::chrono::milliseconds> overallTimeout_;
        };

        /**
         * @brief Sets a rate limit in bytes/second for this session for reading.
         *
         * @param bytesPerSecond
         */
        void readLimit(std::size_t bytesPerSecond);

        /**
         * @brief Sets a rate limit in bytes/second for this session for writing.
         *
         * @param bytesPerSecond
         */
        void writeLimit(std::size_t bytesPerSecond);

        /**
         * @brief Read data from the client.
         *
         * @tparam BodyT What body type to read?
         * @tparam OriginalBodyT Body of the request that came before the this read.
         * @tparam Forwards
         * @param req A request that was received
         * @param forwardArgs
         * @return std::shared_ptr<ReadIntermediate<BodyT>> A class that can be used to start the reading process and
         * set options.
         */
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
            res.setHeader(boost::beast::http::field::server, "Roar+" BOOST_BEAST_VERSION_STRING);
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
         * @return a promise resolving to std::shared_ptr<WebsocketSession> being the new websocket session or an
         */
        [[nodiscard]] Detail::PromiseTypeBind<
            Detail::PromiseTypeBindThen<std::shared_ptr<WebsocketSession>>,
            Detail::PromiseTypeBindFail<Error const&>>
        upgrade(Request<boost::beast::http::empty_body> const& req);

        /**
         * @brief Retrieve the options defined by the request listener class for this route.
         *
         * @return RouteOptions const& The route options.
         */
        [[nodiscard]] RouteOptions const& routeOptions();

        /**
         * @brief Returns whether or not this is an encrypted session.
         *
         * @return true Session is encrypted.
         * @return false Session is not encrypted.
         */
        bool isSecure() const;

        /**
         * @brief Sends a standard response like "404 not found".
         *
         * @param status The status to send.
         * @param additionalInfo Some additional info added into the body (depends on standardResponseProvider).
         */
        void sendStandardResponse(boost::beast::http::status status, std::string_view additionalInfo = "");

        /**
         * @brief Sends a 403 with Strict-Transport-Security. Used only for unencrypted request on enforced HTTPS.
         */
        void sendStrictTransportSecurityResponse();

        /**
         * @brief Some clients insist on closing the connection on their own even if they received a response.
         * To avoid this, this function can be called and the connection will be kept alive until the client closes it
         * or a timeout is reached.
         *
         * @return A promise that resolves to true when the connection was closed by the client.
         */
        template <typename OriginalBodyT>
        promise::Promise
        awaitClientClose(Request<OriginalBodyT> req, std::chrono::milliseconds timeout = std::chrono::seconds{3})
        {
            return promise::newPromise([this, &req, &timeout](promise::Defer d) {
                read<VoidBody>(std::move(req))
                    ->useFixedTimeout(timeout)
                    .commit()
                    .then([d](auto&, auto const&) {
                        d.resolve();
                    })
                    .fail([d](Error const& e) {
                        d.reject(e);
                    });
            });
        }

      private:
        void setupRouteOptions(RouteOptions options);

      private:
        void readHeader();
        void performSslHandshake();
        bool onWriteComplete(bool expectsClose, boost::beast::error_code ec, std::size_t bytesTransferred);
        std::variant<Detail::StreamType, boost::beast::ssl_stream<Detail::StreamType>>& stream();
        std::shared_ptr<boost::beast::http::request_parser<boost::beast::http::empty_body>>& parser();
        boost::beast::flat_buffer& buffer();
        StandardResponseProvider const& standardResponseProvider();
        void startup();

      private:
        struct Implementation;
        std::unique_ptr<Implementation> impl_;
    };
}