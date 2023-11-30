#pragma once

#include <roar/detail/promise_compat.hpp>
#include <roar/detail/pimpl_special_functions.hpp>
#include <roar/error.hpp>
#include <roar/request.hpp>
#include <roar/response.hpp>
#include <roar/utility/visit_overloaded.hpp>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <promise-cpp/promise.hpp>

#include <memory>
#include <optional>
#include <variant>
#include <chrono>
#include <unordered_map>
#include <string_view>
#include <functional>
#include <type_traits>
#include <future>
#include <any>

namespace Roar
{
    class Client : public std::enable_shared_from_this<Client>
    {
      public:
        constexpr static std::chrono::seconds defaultTimeout{10};

        struct SslOptions
        {
            /// Supply for SSL support.
            boost::asio::ssl::context sslContext;

            /// SSL verify mode:
            boost::asio::ssl::verify_mode sslVerifyMode = boost::asio::ssl::verify_peer;

            /**
             * @brief sslVerifyCallback, you can use boost::asio::ssl::rfc2818_verification(host) most of the time.
             */
            std::function<bool(bool, boost::asio::ssl::verify_context&)> sslVerifyCallback;
        };

        struct ConstructionArguments
        {
            /// Required io executor for boost::asio.
            boost::asio::any_io_executor executor;
            std::optional<SslOptions> sslOptions = std::nullopt;
        };

        Client(ConstructionArguments&& args);

        ROAR_PIMPL_SPECIAL_FUNCTIONS_NO_MOVE(Client);

        /**
         * @brief Sends a string to the server.
         *
         * @param message A message to send.
         * @return A promise that is called with the amount of bytes sent, Promise::fail with a
         * Roar::Error.
         */
        Detail::PromiseTypeBind<Detail::PromiseTypeBindThen<std::size_t>, Detail::PromiseTypeBindFail<Error>>
        send(std::string message, std::chrono::seconds timeout = defaultTimeout);

        /**
         * @brief Reads something from the server.
         *
         * @return A promise to continue after reading
         */
        Detail::PromiseTypeBind<
            Detail::PromiseTypeBindThen<std::string_view, std::size_t>,
            Detail::PromiseTypeBindFail<Error>>
        read(std::chrono::seconds timeout = defaultTimeout);

        /**
         * @brief Attach some state to the client lifetime.
         *
         * @param tag A tag name, to retrieve it back with.
         * @param state The state.
         */
        template <typename T>
        void attachState(std::string const& tag, T&& state)
        {
            attachedState_[tag] = std::forward<T>(state);
        }

        /**
         * @brief Create state in place.
         *
         * @tparam ConstructionArgs
         * @param tag
         * @param args
         */
        template <typename T, typename... ConstructionArgs>
        void emplaceState(std::string const& tag, ConstructionArgs&&... args)
        {
            attachedState_[tag] = std::make_any<T>(std::forward<ConstructionArgs>(args)...);
        }

        /**
         * @brief Retrieve attached state by tag.
         *
         * @tparam T Type of the attached state.
         * @param tag The tag of the state.
         * @return T& Returns a reference to the held state.
         */
        template <typename T>
        T& state(std::string const& tag)
        {
            return std::any_cast<T&>(attachedState_.at(tag));
        }

        /**
         * @brief Remove attached state.
         *
         * @param tag The tag of the state to remove.
         */
        void removeState(std::string const& tag)
        {
            attachedState_.erase(tag);
        }

        /**
         * @brief Connects the client to a server and performs a request
         *
         * @param requestParameters see RequestParameters
         * @return A promise to continue after the connect.
         */
        template <typename BodyT>
        Detail::PromiseTypeBind<Detail::PromiseTypeBindThen<>, Detail::PromiseTypeBindFail<Error>>
        request(Request<BodyT>&& request, std::chrono::seconds timeout = defaultTimeout)
        {
            return promise::newPromise([&, this](promise::Defer d) mutable {
                const auto host = request.host();
                const auto port = request.port();
                std::shared_ptr<Request<BodyT>> requestPtr = std::make_shared<Request<BodyT>>(std::move(request));
                doResolve(
                    host,
                    port,
                    [weak = weak_from_this(), timeout, requestPtr, d = std::move(d)](
                        boost::beast::error_code ec, boost::asio::ip::tcp::resolver::results_type results) mutable {
                        auto self = weak.lock();
                        if (!self)
                            return d.reject(Error{.error = ec, .additionalInfo = "Client is no longer alive."});

                        if (ec)
                            return d.reject(Error{.error = ec, .additionalInfo = "DNS resolve failed."});

                        self->withLowerLayerDo([&](auto& socket) {
                            socket.expires_after(timeout);
                            socket.async_connect(
                                results,
                                [weak = self->weak_from_this(), d = std::move(d), requestPtr, timeout](
                                    boost::beast::error_code ec,
                                    boost::asio::ip::tcp::resolver::results_type::endpoint_type endpoint) mutable {
                                    auto self = weak.lock();
                                    if (!self)
                                        return d.reject(
                                            Error{.error = ec, .additionalInfo = "Client is no longer alive."});

                                    if (ec)
                                        return d.reject(Error{.error = ec, .additionalInfo = "TCP connect failed."});

                                    self->endpoint_ = endpoint;

                                    self->onConnect(std::move(*requestPtr), std::move(d), timeout);
                                });
                        });
                    });
            });
        }

        /**
         * @brief Reads only the header, will need be followed up by a readResponse.
         *
         * @tparam ResponseBodyT
         * @param parser
         * @param timeout
         * @return Detail::PromiseTypeBind<
         * Detail::PromiseTypeBindThen<Detail::PromiseReferenceWrap<
         * Detail::PromiseReferenceWrap<boost::beast::http::response_parser<ResponseBodyT>>>>,
         * Detail::PromiseTypeBindFail<Error>>
         */
        template <typename ResponseBodyT>
        Detail::PromiseTypeBind<Detail::PromiseTypeBindThen<>, Detail::PromiseTypeBindFail<Error>> readHeader(
            boost::beast::http::response_parser<ResponseBodyT>& parser,
            std::chrono::seconds timeout = defaultTimeout)
        {
            return promise::newPromise([&, this](promise::Defer d) mutable {
                withLowerLayerDo([timeout](auto& socket) {
                    socket.expires_after(timeout);
                });
                withStreamDo([this, timeout, d = std::move(d), &parser](auto& socket) mutable {
                    boost::beast::http::async_read_header(
                        socket,
                        *buffer_,
                        parser,
                        [weak = weak_from_this(), buffer = this->buffer_, d = std::move(d), timeout, &parser](
                            boost::beast::error_code ec, std::size_t) mutable {
                            auto self = weak.lock();
                            if (!self)
                                return d.reject(Error{.error = ec, .additionalInfo = "Client is no longer alive."});

                            if (ec)
                                return d.reject(Error{.error = ec, .additionalInfo = "HTTP read failed."});

                            d.resolve();
                        });
                });
            });
        }

        /**
         * @brief Read a response from the server.
         *
         * @tparam ResponseBodyT
         * @param timeout
         * @return Detail::PromiseTypeBind<
         * Detail::PromiseTypeBindThen<Detail::PromiseReferenceWrap<Response<ResponseBodyT>>>,
         * Detail::PromiseTypeBindFail<Error>> Returns either a Response<ResponseBodyT> or an Error.
         */
        template <typename ResponseBodyT>
        Detail::PromiseTypeBind<
            Detail::PromiseTypeBindThen<Detail::PromiseReferenceWrap<Response<ResponseBodyT>>>,
            Detail::PromiseTypeBindFail<Error>>
        readResponse(std::chrono::seconds timeout = defaultTimeout)
        {
            return promise::newPromise([this, timeout](promise::Defer d) mutable {
                withLowerLayerDo([timeout](auto& socket) {
                    socket.expires_after(timeout);
                });
                withStreamDo([this, d = std::move(d)](auto& socket) mutable {
                    struct Context
                    {
                        Response<ResponseBodyT> response{};
                    };
                    auto context = std::make_shared<Context>();
                    boost::beast::http::async_read(
                        socket,
                        *buffer_,
                        context->response.response(),
                        [weak = weak_from_this(), buffer = this->buffer_, d = std::move(d), context](
                            boost::beast::error_code ec, std::size_t) mutable {
                            auto self = weak.lock();
                            if (!self)
                                return d.reject(Error{.error = ec, .additionalInfo = "Client is no longer alive."});

                            if (ec)
                                return d.reject(Error{.error = ec, .additionalInfo = "HTTP read failed."});

                            d.resolve(Detail::ref(context->response));
                        });
                });
            });
        }

        /**
         * @brief Read a response using a beast response parser. You are responsible for keeping the parser alive!
         *
         * @tparam ResponseBodyT
         * @param parser Passed in by reference, must be alive until the promise is resolved.
         * @param timeout
         * @return Detail::PromiseTypeBind<Detail::PromiseTypeBindThen<>, Detail::PromiseTypeBindFail<Error const&>>
         */
        template <typename ResponseBodyT>
        Detail::PromiseTypeBind<Detail::PromiseTypeBindThen<>, Detail::PromiseTypeBindFail<Error>> readResponse(
            boost::beast::http::response_parser<ResponseBodyT>& parser,
            std::chrono::seconds timeout = defaultTimeout)
        {
            return promise::newPromise([&, this](promise::Defer d) mutable {
                withLowerLayerDo([timeout](auto& socket) {
                    socket.expires_after(timeout);
                });
                withStreamDo([this, timeout, d = std::move(d), &parser](auto& socket) mutable {
                    boost::beast::http::async_read(
                        socket,
                        *buffer_,
                        parser,
                        [weak = weak_from_this(), buffer = this->buffer_, d = std::move(d), timeout, &parser](
                            boost::beast::error_code ec, std::size_t) mutable {
                            auto self = weak.lock();
                            if (!self)
                                return d.reject(Error{.error = ec, .additionalInfo = "Client is no longer alive."});

                            if (ec)
                                return d.reject(Error{.error = ec, .additionalInfo = "HTTP read failed."});

                            d.resolve();
                        });
                });
            });
        }

        template <typename ResponseBodyT, typename RequestBodyT>
        Detail::PromiseTypeBind<
            Detail::PromiseTypeBindThen<Detail::PromiseReferenceWrap<Response<ResponseBodyT>>>,
            Detail::PromiseTypeBindFail<Error>>
        requestAndReadResponse(Request<RequestBodyT>&& request, std::chrono::seconds timeout = defaultTimeout)
        {
            return promise::newPromise([r = std::move(request), timeout, this](promise::Defer d) mutable {
                this->request(std::move(r), timeout)
                    .then([weak = weak_from_this(), timeout, d]() {
                        auto client = weak.lock();
                        if (!client)
                            return d.reject(Error{.additionalInfo = "Client is no longer alive."});

                        client->readResponse<ResponseBodyT>(timeout)
                            .then([d](auto& response) {
                                d.resolve(Detail::ref(response));
                            })
                            .fail([d](auto error) {
                                d.reject(std::move(error));
                            });
                    })
                    .fail([d](auto error) {
                        d.reject(std::move(error));
                    });
            });
        }

        Detail::PromiseTypeBind<Detail::PromiseTypeBindThen<>, Detail::PromiseTypeBindFail<Error>> close()
        {
            return promise::newPromise([&, this](promise::Defer d) mutable {
                resolver_.cancel();
                withLowerLayerDo([&](auto& socket) {
                    if (socket.socket().is_open())
                    {
                        socket.cancel();
                        socket.close();
                    }
                });
                return d.resolve();
            });
        }

        Detail::PromiseTypeBind<Detail::PromiseTypeBindThen<>, Detail::PromiseTypeBindFail<Error>>
        shutdownSsl(std::chrono::seconds timeout = defaultTimeout)
        {
            return promise::newPromise([&, this](promise::Defer d) mutable {
                if (std::holds_alternative<boost::beast::tcp_stream>(socket_))
                    return d.resolve();

                withLowerLayerDo([timeout](auto& socket) {
                    socket.expires_after(timeout);
                });
                auto& socket = std::get<boost::beast::ssl_stream<boost::beast::tcp_stream>>(socket_);
                socket.async_shutdown([d = std::move(d)](boost::beast::error_code ec) mutable {
                    if (ec == boost::asio::error::eof)
                        ec = {};

                    if (ec)
                        return d.reject(Error{.error = ec, .additionalInfo = "Stream shutdown failed."});

                    d.resolve();
                });
            });
        }

      public:
        template <typename FunctionT>
        void withStreamDo(FunctionT&& func)
        {
            std::visit(std::forward<FunctionT>(func), socket_);
        }

        template <typename FunctionT>
        void withStreamDo(FunctionT&& func) const
        {
            std::visit(std::forward<FunctionT>(func), socket_);
        }

        template <typename FunctionT>
        void withLowerLayerDo(FunctionT&& func)
        {
            visitOverloaded(
                socket_,
                [&func](boost::beast::ssl_stream<boost::beast::tcp_stream>& stream) {
                    return func(stream.next_layer());
                },
                [&func](boost::beast::tcp_stream& stream) {
                    return func(stream);
                });
        }

        template <typename FunctionT>
        void withLowerLayerDo(FunctionT&& func) const
        {
            visitOverloaded(
                socket_,
                [&func](boost::beast::ssl_stream<boost::beast::tcp_stream> const& stream) {
                    return func(stream.next_layer());
                },
                [&func](boost::beast::tcp_stream const& stream) {
                    return func(stream);
                });
        }

      private:
        void doResolve(
            std::string const& host,
            std::string const& port,
            std::function<void(boost::beast::error_code ec, boost::asio::ip::tcp::resolver::results_type results)>
                onResolve);

        template <typename BodyT>
        void onConnect(Request<BodyT>&& request, promise::Defer&& d, std::chrono::seconds timeout)
        {
            if (std::holds_alternative<boost::beast::ssl_stream<boost::beast::tcp_stream>>(socket_))
            {
                auto maybeError = setupSsl(request.host());
                if (maybeError)
                    return d.reject(*maybeError);

                auto& sslSocket = std::get<boost::beast::ssl_stream<boost::beast::tcp_stream>>(socket_);
                withLowerLayerDo([&](auto& socket) {
                    socket.expires_after(timeout);
                });
                sslSocket.async_handshake(
                    boost::asio::ssl::stream_base::client,
                    [weak = weak_from_this(), d = std::move(d), request = std::move(request), timeout](
                        boost::beast::error_code ec) mutable {
                        auto self = weak.lock();
                        if (!self)
                            return d.reject(Error{.error = ec, .additionalInfo = "Client is no longer alive."});

                        if (ec)
                            return d.reject(Error{.error = ec, .additionalInfo = "SSL handshake failed."});

                        self->performRequest(std::move(request), std::move(d), timeout);
                    });
            }
            else
                performRequest(std::move(request), std::move(d), timeout);
        }

        template <typename BodyT>
        void performRequest(Request<BodyT>&& request, promise::Defer&& d, std::chrono::seconds timeout)
        {
            withLowerLayerDo([timeout](auto& socket) {
                socket.expires_after(timeout);
            });
            withStreamDo([this, request = std::move(request), &d](auto& socket) mutable {
                std::shared_ptr<Request<BodyT>> requestPtr = std::make_shared<Request<BodyT>>(std::move(request));
                boost::beast::http::async_write(
                    socket,
                    *requestPtr,
                    [weak = weak_from_this(), d = std::move(d), requestPtr](
                        boost::beast::error_code ec, std::size_t) mutable {
                        auto self = weak.lock();
                        if (!self)
                            return d.reject(Error{.error = ec, .additionalInfo = "Client is no longer alive."});

                        if (ec)
                            return d.reject(Error{.error = ec, .additionalInfo = "HTTP write failed."});

                        d.resolve();
                    });
            });
        }

        std::optional<Error> setupSsl(std::string const& host);

      private:
        std::optional<SslOptions> sslOptions_;
        boost::asio::ip::tcp::resolver resolver_;
        std::shared_ptr<boost::beast::flat_buffer> buffer_;
        std::variant<boost::beast::ssl_stream<boost::beast::tcp_stream>, boost::beast::tcp_stream> socket_;
        boost::asio::ip::tcp::resolver::results_type::endpoint_type endpoint_;
        std::unordered_map<std::string, std::any> attachedState_;
    };
}