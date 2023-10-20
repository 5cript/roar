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

namespace Roar
{
    class Client : public std::enable_shared_from_this<Client>
    {
      public:
        constexpr static std::chrono::seconds defaultTimeout{10};

        struct ConstructionArguments
        {
            /// Required io executor for boost::asio.
            boost::asio::any_io_executor executor;

            /// Supply for SSL support.
            std::optional<boost::asio::ssl::context> sslContext;

            /// SSL verify mode:
            boost::asio::ssl::verify_mode sslVerifyMode = boost::asio::ssl::verify_none;

            /**
             * @brief sslVerifyCallback, you can use boost::asio::ssl::rfc2818_verification(host) most of the time.
             */
            std::function<bool(bool, boost::asio::ssl::verify_context&)> sslVerifyCallback = {};
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
                if (std::holds_alternative<boost::beast::ssl_stream<boost::beast::tcp_stream>>(socket_))
                {
                    auto& sslSocket = std::get<boost::beast::ssl_stream<boost::beast::tcp_stream>>(socket_);
                    if (!SSL_ctrl(
                            sslSocket.native_handle(),
                            SSL_CTRL_SET_TLSEXT_HOSTNAME,
                            TLSEXT_NAMETYPE_host_name,
                            // yikes openssl you make me do this
                            const_cast<void*>(reinterpret_cast<void const*>(request.host().c_str()))))
                    {
                        boost::beast::error_code ec{
                            static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category()};
                        return d.reject(Error{.error = ec, .additionalInfo = "SSL_set_tlsext_host_name failed."});
                    }
                }

                const auto host = request.host();
                const auto port = request.port();
                doResolve(
                    host,
                    port,
                    [weak = weak_from_this(), timeout, request = std::move(request), d = std::move(d)](
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
                                [weak = self->weak_from_this(),
                                 d = std::move(d),
                                 request = std::move(request),
                                 timeout](
                                    boost::beast::error_code ec,
                                    boost::asio::ip::tcp::resolver::results_type::endpoint_type endpoint) mutable {
                                    auto self = weak.lock();
                                    if (!self)
                                        d.reject(Error{.error = ec, .additionalInfo = "Client is no longer alive."});

                                    if (ec)
                                        d.reject(Error{.error = ec, .additionalInfo = "TCP connect failed."});

                                    self->endpoint_ = endpoint;

                                    self->onConnect(std::move(request), std::move(d), timeout);
                                });
                        });
                    });
            });
        }

        template <typename ResponseBodyT>
        Detail::PromiseTypeBind<
            Detail::PromiseTypeBindThen<Detail::PromiseReferenceWrap<Response<ResponseBodyT>>>,
            Detail::PromiseTypeBindFail<Error>>
        readResponse(std::chrono::seconds timeout = defaultTimeout)
        {
            return promise::newPromise([&, this](promise::Defer d) mutable {
                withLowerLayerDo([timeout](auto& socket) {
                    socket.expires_after(timeout);
                });
                withStreamDo([this, timeout, d = std::move(d)](auto& socket) mutable {
                    struct Context
                    {
                        Response<ResponseBodyT> response;
                        boost::beast::flat_buffer buffer;
                    };
                    auto context = std::make_shared<Context>();
                    boost::beast::http::async_read(
                        socket,
                        context->buffer,
                        context->response.response(),
                        [weak = weak_from_this(), d = std::move(d), timeout, context](
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
         * @param parser
         * @param timeout
         * @return Detail::PromiseTypeBind<Detail::PromiseTypeBindThen<>, Detail::PromiseTypeBindFail<Error const&>>
         */
        template <typename ResponseBodyT>
        Detail::PromiseTypeBind<
            Detail::PromiseTypeBindThen<
                Detail::PromiseReferenceWrap<boost::beast::http::response_parser<ResponseBodyT>>>,
            Detail::PromiseTypeBindFail<Error>>
        readResponse(
            boost::beast::http::response_parser<ResponseBodyT>& parser,
            std::chrono::seconds timeout = defaultTimeout)
        {
            return promise::newPromise([&, this](promise::Defer d) mutable {
                withLowerLayerDo([timeout](auto& socket) {
                    socket.expires_after(timeout);
                });
                withStreamDo([this, timeout, d = std::move(d), &parser](auto& socket) mutable {
                    struct Context
                    {
                        boost::beast::flat_buffer buffer;
                    };
                    auto context = std::make_shared<Context>();
                    boost::beast::http::async_read(
                        socket,
                        context->buffer,
                        parser,
                        [weak = weak_from_this(), d = std::move(d), timeout, context, &parser](
                            boost::beast::error_code ec, std::size_t) mutable {
                            auto self = weak.lock();
                            if (!self)
                                return d.reject(Error{.error = ec, .additionalInfo = "Client is no longer alive."});

                            if (ec)
                                return d.reject(Error{.error = ec, .additionalInfo = "HTTP read failed."});

                            d.resolve(Detail::ref(parser));
                        });
                });
            });
        }

        Detail::PromiseTypeBind<
            Detail::PromiseTypeBindThen<
                Detail::PromiseReferenceWrap<boost::beast::http::response_parser<boost::beast::http::empty_body>>,
                Detail::PromiseReferenceWrap<bool>>,
            Detail::PromiseTypeBindFail<Error>>
        readServerSentEvents(
            std::function<bool(std::string_view, std::string_view)> onEvent =
                [](auto, auto) {
                    return true;
                },
            std::function<void(std::optional<Error> const&)> onEndOfStream = [](auto) {},
            std::function<bool(std::uint64_t size, std::string_view)> onHeader =
                [](auto, auto) {
                    return true;
                },
            std::chrono::seconds timeout = defaultTimeout);

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
            withStreamDo([this, &request, &d, timeout](auto& socket) mutable {
                boost::beast::http::async_write(
                    socket,
                    request,
                    [weak = weak_from_this(), d = std::move(d), timeout](
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

      private:
        std::variant<boost::beast::ssl_stream<boost::beast::tcp_stream>, boost::beast::tcp_stream> socket_;
        boost::asio::ip::tcp::resolver::results_type::endpoint_type endpoint_;
    };
}