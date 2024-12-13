#pragma once

#include <roar/mechanics/cookie.hpp>
#include <functional>
#include <roar/error.hpp>
#include <roar/websocket/websocket_session.hpp>
#include <roar/response.hpp>
#include <roar/detail/pimpl_special_functions.hpp>
#include <roar/literals/memory.hpp>
#include <roar/routing/proto_route.hpp>
#include <roar/standard_response_provider.hpp>
#include <roar/detail/promise_compat.hpp>
#include <roar/body/void_body.hpp>
#include <roar/detail/stream_type.hpp>
#include <roar/body/range_file_body.hpp>

#include <boost/beast/http/message.hpp>
#include <boost/beast/http/empty_body.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <roar/ssl/make_ssl_context.hpp>
#include <promise-cpp/promise.hpp>

#include <memory>
#include <optional>
#include <chrono>
#include <variant>
#include <stdexcept>
#include <sstream>

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
            std::optional<std::variant<SslServerContext, boost::asio::ssl::context>>& sslContext,
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
            template <typename... Forwards, typename RequestBodyT>
            SendIntermediate(Session& session, Request<RequestBodyT> const& req, Forwards&&... forwards)
                : session_(session.shared_from_this())
                , response_{session_->prepareResponse<BodyT>(req, std::forward<Forwards>(forwards)...)}
            {}
            SendIntermediate(Session& session, boost::beast::http::response<BodyT> res)
                : session_(session.shared_from_this())
                , response_{std::move(res)}
            {}
            SendIntermediate(Session& session, Response<BodyT>&& res)
                : session_(session.shared_from_this())
                , response_{std::move(res)}
            {}
            SendIntermediate(Session& session)
                : session_(session.shared_from_this())
                , response_{session_->prepareResponse<BodyT>()}
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
                response_.body(std::forward<T>(toAssign));
                preparePayload();
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
            template <typename T>
            SendIntermediate& header(boost::beast::http::field field, T&& value)
            {
                return setHeader(field, std::forward<T>(value));
            }

            /**
             * @brief Set a callback function that is called whenever some data was written.
             *
             * @param onChunkFunc The function to be called. Is called with the buffer and amount of bytes
             * transferred. Return false in this function to stop writing.
             * @return ReadIntermediate& Returns itself for chaining.
             */
            SendIntermediate& onWriteSome(std::function<bool(std::size_t)> onChunkFunc)
            {
                onChunk_ = onChunkFunc;
                return *this;
            }

            /**
             * @brief Sets cors headers.
             * @param req A request to base the cors headers off of.
             * @param cors A cors settings object. If not supplied, a very permissive setting is used.
             *
             * @return Response& Returned for chaining.
             */
            template <typename RequestBodyT2>
            SendIntermediate&
            enableCors(Request<RequestBodyT2> const& req, std::optional<CorsSettings> cors = std::nullopt)
            {
                response_.enableCors(req, std::move(cors));
                return *this;
            }

            SendIntermediate& preparePayload()
            {
                response_.preparePayload();
                return *this;
            }

            SendIntermediate& modifyResponse(std::function<void(Response<BodyT>&)> func)
            {
                func(response_);
                return *this;
            }

          private:
            using CommitReturnType =
                Detail::PromiseTypeBind<Detail::PromiseTypeBindThen<bool>, Detail::PromiseTypeBindFail<Error>>;

          public:
            /**
             * @brief Sends the response and invalidates this object
             */
            template <typename T = BodyT, bool OverrideSfinae = false>
            std::enable_if_t<
                !(std::is_same_v<T, RangeFileBody> || std::is_same_v<T, boost::beast::http::empty_body>) ||
                    OverrideSfinae,
                CommitReturnType>
            commit()
            {
                if (overallTimeout_)
                {
                    session_->withStreamDo([this](auto& stream) {
                        boost::beast::get_lowest_layer(stream).expires_after(*overallTimeout_);
                    });
                }
                promise_ = std::make_unique<promise::Promise>(promise::newPromise());
                serializer_ = std::make_unique<boost::beast::http::serializer<false, BodyT>>(response_.response());
                writeChunk();
                return {*promise_};
            }

            /**
             * @brief Sends the response and invalidates this object
             */
            template <typename T = BodyT>
            std::enable_if_t<std::is_same_v<T, boost::beast::http::empty_body>, CommitReturnType> commit()
            {
                if (overallTimeout_)
                {
                    session_->withStreamDo([this](auto& stream) {
                        boost::beast::get_lowest_layer(stream).expires_after(*overallTimeout_);
                    });
                }
                promise_ = std::make_unique<promise::Promise>(promise::newPromise());
                serializer_ = std::make_unique<boost::beast::http::serializer<false, BodyT>>(response_.response());
                writeHeader();
                return {*promise_};
            }

            /**
             * @brief Sends the response for range request file bodies. Sets appropriate headers.
             */
            template <typename T = BodyT>
            std::enable_if_t<std::is_same_v<T, RangeFileBody>, CommitReturnType> commit()
            {
                using namespace boost::beast::http;
                preparePayload();
                if (response_.body().isMultipart())
                    setHeader(field::content_type, "multipart/byteranges; boundary=" + response_.body().boundary());
                else
                {
                    const auto first = response_.body().firstRange().first;
                    const auto second = response_.body().firstRange().second;
                    const auto fileSize = response_.body().fileSize();

                    setHeader(
                        field::content_range,
                        std::string{"bytes "} + std::to_string(first) + "-" + std::to_string(second) + "/" +
                            std::to_string(fileSize));
                }
                status(status::partial_content);
                return commit<BodyT, true>();
            }

            /**
             * @brief Sets WWW-Authenticate header and sets the status to unauthorized.
             *
             * @param wwwAuthenticate A string to be used as the value of the header. eg: "Basic realm=<realm>"
             */
            SendIntermediate& rejectAuthorization(std::string const& wwwAuthenticate)
            {
                if (!wwwAuthenticate.empty())
                    response_.setHeader(boost::beast::http::field::www_authenticate, wwwAuthenticate);
                response_.status(boost::beast::http::status::unauthorized);
                return *this;
            }

            /**
             * @brief Set keep alive.
             *
             * @param keepAlive
             * @return SendIntermediate&
             */
            SendIntermediate& keepAlive(bool keepAlive = true)
            {
                response_.keepAlive(keepAlive);
                return *this;
            }

            /**
             * @brief Sets a cookie.
             *
             * @param cookie A cookie object
             * @return SendIntermediate& Returns itself for chaining.
             */
            SendIntermediate& setCookie(Cookie const& cookie)
            {
                response_.setCookie(cookie);
                return *this;
            }

            /**
             * @brief Set a timeout for the whole write operation.
             *
             * @param timeout The timeout as a std::chrono::duration.
             * @return ReadIntermediate& Returns itself for chaining.
             */
            SendIntermediate& useFixedTimeout(std::chrono::milliseconds timeout)
            {
                overallTimeout_ = timeout;
                return *this;
            }

          private:
            void writeHeader()
            {
                session_->withStreamDo([this](auto& stream) {
                    if (!overallTimeout_)
                        boost::beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(sessionTimeout));

                    boost::beast::http::async_write_header(
                        stream,
                        *serializer_,
                        [self = this->shared_from_this()](boost::beast::error_code ec, std::size_t) {
                            if (ec)
                                self->promise_->fail(ec);
                            else
                                self->promise_->resolve(false);
                        });
                });
            }

            void writeChunk()
            {
                session_->withStreamDo([this](auto& stream) {
                    if (!overallTimeout_)
                        boost::beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(sessionTimeout));

                    boost::beast::http::async_write_some(
                        stream,
                        *serializer_,
                        [self = this->shared_from_this()](boost::beast::error_code ec, std::size_t bytesTransferred) {
                            if (!ec && !self->serializer_->is_done())
                                return self->writeChunk();

                            if (ec)
                            {
                                self->session_->close();
                                self->promise_->reject(Error{.error = ec, .additionalInfo = "Failed to send response"});
                                return;
                            }

                            if (self->onChunk_ && !self->onChunk_(bytesTransferred))
                                return;

                            try
                            {
                                self->promise_->resolve(self->session_->onWriteComplete(
                                    self->serializer_->get().need_eof(), ec, bytesTransferred));
                            }
                            catch (std::exception const& exc)
                            {
                                self->promise_->fail(
                                    Error{.error = exc.what(), .additionalInfo = "Failed to send response"});
                            }
                        });
                });
            }

          private:
            std::shared_ptr<Session> session_;
            Response<BodyT> response_;
            std::function<bool(std::size_t)> onChunk_;
            std::unique_ptr<promise::Promise> promise_;
            std::optional<std::chrono::milliseconds> overallTimeout_;
            std::unique_ptr<boost::beast::http::serializer<false, BodyT>> serializer_;
        };

        template <typename BodyT, typename OriginalBodyT, typename... Forwards>
        [[nodiscard]] std::shared_ptr<SendIntermediate<BodyT>>
        send(Request<OriginalBodyT> const& req, Forwards&&... forwards)
        {
            return std::shared_ptr<SendIntermediate<BodyT>>(
                new SendIntermediate<BodyT>{*this, req, std::forward<Forwards>(forwards)...});
        }

        template <typename BodyT>
        [[nodiscard]] std::shared_ptr<SendIntermediate<BodyT>> send(boost::beast::http::response<BodyT>&& res)
        {
            return std::shared_ptr<SendIntermediate<BodyT>>(new SendIntermediate<BodyT>{*this, std::move(res)});
        }

        template <typename BodyT>
        [[nodiscard]] std::shared_ptr<SendIntermediate<BodyT>> send(Response<BodyT>&& res)
        {
            return std::shared_ptr<SendIntermediate<BodyT>>(new SendIntermediate<BodyT>{*this, std::move(res)});
        }

        template <typename BodyT>
        [[nodiscard]] std::shared_ptr<SendIntermediate<BodyT>> send()
        {
            return std::shared_ptr<SendIntermediate<BodyT>>(new SendIntermediate<BodyT>{*this});
        }

        /**
         * @brief Send raw data. Use this after sending a proper response for sending additional data.
         *
         * @param data
         */
        Detail::PromiseTypeBind<Detail::PromiseTypeBindThen<std::size_t>, Detail::PromiseTypeBindFail<Error>>
        send(std::string message, std::chrono::seconds timeout = std::chrono::seconds{10});

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
                , req_{[&session, &forwardArgs...]() -> decltype(req_) {
                    if constexpr (std::is_same_v<BodyT, boost::beast::http::empty_body>)
                        throw std::runtime_error("Attempting to read with empty_body type.");
                    else
                    {
                        return boost::beast::http::request_parser<BodyT>{
                            std::move(*session.parser()), std::forward<Forwards>(forwardArgs)...};
                    }
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
             * @param onChunkFunc The function to be called. Is called with the buffer and amount of bytes
             * transferred. Return false in this function to stop reading.
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
             * @brief Start reading the header here.
             *
             * @return Promise
             */
            Detail::PromiseTypeBind<
                Detail::PromiseTypeBindThen<
                    Detail::PromiseReferenceWrap<Session>,
                    Detail::PromiseReferenceWrap<boost::beast::http::request_parser<BodyT> const>,
                    std::shared_ptr<ReadIntermediate<BodyT>>>,
                Detail::PromiseTypeBindFail<Error const&>>
            commitHeaderOnly()
            {
                if (overallTimeout_)
                {
                    session_->withStreamDo([this](auto& stream) {
                        boost::beast::get_lowest_layer(stream).expires_after(*overallTimeout_);
                    });
                }
                promise_ = std::make_unique<promise::Promise>(promise::newPromise());
                readHeader();
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
                                self->promise_->resolve(Detail::ref(*self->session_), Detail::cref(request));
                            }
                            catch (std::exception const& exc)
                            {
                                using namespace std::string_literals;
                                self->session_
                                    ->send(self->session_->standardResponseProvider().makeStandardResponse(
                                        *self->session_,
                                        boost::beast::http::status::internal_server_error,
                                        "An exception was thrown in the body read completion handler: "s + exc.what()))
                                    ->commit();
                            }
                        });
                });
            }

            void readHeader()
            {
                session_->withStreamDo([this](auto& stream) {
                    if (!overallTimeout_)
                        boost::beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(sessionTimeout));

                    boost::beast::http::async_read_header(
                        stream,
                        session_->buffer(),
                        req_,
                        [self = this->shared_from_this()](boost::beast::error_code ec, std::size_t) {
                            if (ec)
                            {
                                self->session_->close();
                                self->promise_->reject(Error{.error = ec});
                            }
                            else
                            {
                                try
                                {
                                    self->promise_->resolve(
                                        Detail::ref(*self->session_), Detail::cref(self->req_), self);
                                }
                                catch (std::exception const& exc)
                                {
                                    using namespace std::string_literals;
                                    self->session_
                                        ->send(self->session_->standardResponseProvider().makeStandardResponse(
                                            *self->session_,
                                            boost::beast::http::status::internal_server_error,
                                            "An exception was thrown in the header read completion handler: "s +
                                                exc.what()))
                                        ->commit();
                                }
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
         * @return std::shared_ptr<ReadIntermediate<BodyT>> A class that can be used to start the reading process
         * and set options.
         */
        template <typename BodyT, typename OriginalBodyT, typename... Forwards>
        [[nodiscard]] std::shared_ptr<ReadIntermediate<BodyT>>
        read(Request<OriginalBodyT> req, Forwards&&... forwardArgs)
        {
            return std::shared_ptr<ReadIntermediate<BodyT>>(
                new ReadIntermediate<BodyT>{*this, std::move(req), std::forward<Forwards>(forwardArgs)...});
        }

        template <typename BodyT, typename OriginalBodyT, typename... Forwards>
        [[nodiscard]] std::shared_ptr<ReadIntermediate<BodyT>>
        readHeader(Request<OriginalBodyT> req, Forwards&&... forwardArgs)
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
        template <typename BodyT = boost::beast::http::empty_body, typename RequestBodyT, typename... Forwards>
        [[nodiscard]] Response<BodyT> prepareResponse(Request<RequestBodyT> const& req, Forwards&&... forwardArgs)
        {
            auto res = Response<BodyT>{std::forward<Forwards>(forwardArgs)...};
            res.setHeader(boost::beast::http::field::server, "Roar+" BOOST_BEAST_VERSION_STRING);
            if (routeOptions().cors)
                res.enableCors(req, routeOptions().cors);
            return res;
        }
        template <typename BodyT = boost::beast::http::empty_body>
        [[nodiscard]] Response<BodyT> prepareResponse()
        {
            auto res = Response<BodyT>{};
            res.setHeader(boost::beast::http::field::server, "Roar+" BOOST_BEAST_VERSION_STRING);
            if (routeOptions().cors)
                res.enableCorsEverything();
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
         * To avoid this, this function can be called and the connection will be kept alive until the client closes
         * it or a timeout is reached.
         *
         * @return A promise that resolves to true when the connection was closed by the client.
         */
        template <typename OriginalBodyT>
        Detail::PromiseTypeBind<Detail::PromiseTypeBindThen<>, Detail::PromiseTypeBindFail<Error const&>>
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
        bool onWriteComplete(bool expectsClose, boost::beast::error_code ec, std::size_t);
        std::variant<Detail::StreamType, boost::beast::ssl_stream<Detail::StreamType>>& stream();
        std::shared_ptr<boost::beast::http::request_parser<boost::beast::http::empty_body>>& parser();
        boost::beast::flat_buffer& buffer();
        StandardResponseProvider const& standardResponseProvider();
        void startup(bool immediate = true);

      private:
        struct Implementation;
        std::unique_ptr<Implementation> impl_;
    };
}