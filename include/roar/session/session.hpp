#pragma once

#include <functional>
#include <roar/error.hpp>
#include <roar/detail/type_equal_compare.hpp>
#include <roar/websocket/websocket_session.hpp>
#include <roar/response.hpp>
#include <roar/beast/forward.hpp>
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
            template <typename OriginalBodyT, typename... Forwards>
            SendIntermediate(Session& session, Request<OriginalBodyT> const& req, Forwards&&... forwards)
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
            template <typename RequestBodyT>
            SendIntermediate&
            enableCors(Request<RequestBodyT> const& req, std::optional<CorsSettings> cors = std::nullopt)
            {
                response_.enableCors(req, std::move(cors));
                return *this;
            }

            SendIntermediate& preparePayload()
            {
                response_.preparePayload();
                return *this;
            }

            /**
             * @brief Sends the response and invalidates this object
             */
            Detail::PromiseTypeBind<Detail::PromiseTypeBindThen<bool>, Detail::PromiseTypeBindFail<Error>> commit()
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

            template <typename T = BodyT>
            std::enable_if_t<typeof(T{}) == typeof(RangeFileBody{}), SendIntermediate&> suffix(std::string const& str)
            {
                response_.body().suffix(str);
                return *this;
            }

            template <typename OriginalBodyT, typename T = BodyT>
            std::enable_if_t<
                typeof(T{}) == typeof(RangeFileBody{}),
                Detail::PromiseTypeBind<Detail::PromiseTypeBindThen<bool>, Detail::PromiseTypeBindFail<Error>>>
            commitRanges(Request<OriginalBodyT> const& req, std::string partBodyType = "text/plain")
            {
                const auto maybeRanges = req.ranges();
                if (!maybeRanges)
                    throw std::invalid_argument("No ranges supplied");

                auto ranges = *maybeRanges;

                if (ranges.ranges.size() == 0)
                    throw std::invalid_argument("No ranges supplied");

                if (ranges.ranges.size() == 1)
                {
                    setHeader(
                        boost::beast::http::field::content_range,
                        std::string{"bytes "} + ranges.ranges[0].toString() + "/" +
                            std::to_string(response_.body().realSize()));
                    setHeader(boost::beast::http::field::content_length, std::to_string(ranges.ranges[0].size()));
                    status(boost::beast::http::status::partial_content);
                    response_.body().setReadRange(ranges.ranges[0].start, ranges.ranges[0].end);
                    return commit();
                }
                else
                {
                    return promise::newPromise([&, this](promise::Defer d) {
                        const auto totalSize = std::accumulate(
                            std::begin(ranges.ranges),
                            std::end(ranges.ranges),
                            std::uint64_t{0},
                            [](std::uint64_t acc, Ranges::Range const& range) {
                                return acc + range.size();
                            });

                        auto sendSingleRange = std::make_shared<std::function<void(Ranges ranges)>>();
                        *sendSingleRange = [self = this->shared_from_this(),
                                            req,
                                            sendSingleRange,
                                            d,
                                            partBodyType = std::move(partBodyType),
                                            realSizeSuffix = std::string{"/"} +
                                                std::to_string(response_.body().realSize())](Ranges ranges) {
                            const auto path = self->response_.body().originalPath();
                            self->response_.body().close();
                            RangeFileBody::value_type file;
                            std::error_code ec;
                            file.open(path, std::ios_base::in, ec);
                            if (ec)
                            {
                                d.reject(Error{.error = ec.message()});
                                return;
                            }
                            file.setReadRange(ranges.ranges[0].start, ranges.ranges[0].end);
                            ranges.ranges.pop_front();
                            self->session_->template send<RangeFileBody>(req, std::move(file))
                                ->setHeader(
                                    boost::beast::http::field::content_range,
                                    std::string{"bytes "} + ranges.ranges[0].toString() + realSizeSuffix)
                                .setHeader(boost::beast::http::field::content_type, partBodyType)
                                .keepAlive()
                                .suffix("--ROAR_MULTIPART_BOUNDARY")
                                .commit()
                                .fail([d](Error e) {
                                    d.reject(e);
                                })
                                .then(
                                    [self, d, ranges = std::move(ranges), sendSingleRange = std::move(sendSingleRange)](
                                        bool wasClosed) {
                                        if (wasClosed)
                                        {
                                            d.reject(Error{.additionalInfo = "Connection was closed"});
                                            return;
                                        }
                                        if (ranges.ranges.size() > 0)
                                            (*sendSingleRange)(std::move(ranges));
                                        else
                                            d.resolve(true);
                                    });
                        };

                        session_->send<boost::beast::http::string_body>(req)
                            ->status(boost::beast::http::status::partial_content)
                            .setHeader(
                                boost::beast::http::field::content_type,
                                "multipart/byteranges; boundary=ROAR_MULTIPART_BOUNDARY")
                            .setHeader(boost::beast::http::field::content_length, std::to_string(totalSize))
                            .keepAlive()
                            .body("--ROAR_MULTIPART_BOUNDARY")
                            .commit()
                            .fail([d](Error e) {
                                d.reject(e);
                            })
                            .then([d,
                                   self = this->shared_from_this(),
                                   ranges = std::move(ranges),
                                   sendSingleRange = std::move(sendSingleRange)](bool wasClosed) {
                                if (wasClosed)
                                {
                                    d.reject(Error{.additionalInfo = "Connection was closed"});
                                    return;
                                }
                                (*sendSingleRange)(std::move(ranges));
                            });
                    });
                }
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

                    serializer_ = std::make_unique<boost::beast::http::serializer<false, BodyT>>(response_.response());

                    boost::beast::http::async_write_header(
                        stream,
                        *serializer_,
                        [self = this->shared_from_this()](boost::beast::error_code ec, std::size_t) {
                            if (ec)
                                self->promise_->fail(ec);
                            else
                                self->promise_->resolve();
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