#include <boost/beast/core/stream_traits.hpp>
#include <roar/websocket/client.hpp>

#include <roar/dns/resolve.hpp>

#include <boost/asio/buffer.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/strand.hpp>

#include <stdexcept>
#include <variant>

namespace Roar
{
    using namespace promise;

    //##################################################################################################################
    struct WebSocketClient::Implementation : public std::enable_shared_from_this<WebSocketClient::Implementation>
    {
        std::variant<
            boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream>>,
            boost::beast::websocket::stream<boost::beast::tcp_stream>>
            ws;
        boost::beast::flat_buffer buffer;
        std::function<void(Error&&)> onError;
        std::string host;
        std::unordered_map<boost::beast::http::field, std::string> handshakeHeaders;
        std::string path;
        std::function<void()> onConnectComplete;

        Implementation(boost::asio::any_io_executor&& executor, std::optional<boost::asio::ssl::context> sslContext)
            : ws{[&executor, &sslContext]() -> decltype(ws) {
                if (sslContext)
                    return boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream>>{
                        boost::asio::make_strand(executor), *sslContext};
                else
                    return boost::beast::websocket::stream<boost::beast::tcp_stream>{
                        boost::asio::make_strand(executor)};
            }()}
            , buffer{}
            , host{}
        {}

        void withStreamDo(auto&& func)
        {
            std::visit(std::forward<decltype(func)>(func), ws);
        }

        void onConnect(
            boost::beast::error_code ec,
            boost::asio::ip::tcp::resolver::results_type::endpoint_type const& ep,
            std::chrono::seconds timeout,
            std::string passedHost)
        {
            host = passedHost + ':' + std::to_string(ep.port());

            if (ec)
                return onError({.error = ec, .additionalInfo = "Could not connect."});

            withStreamDo([&, this](auto& ws) {
                boost::beast::get_lowest_layer(ws).expires_after(timeout);
            });

            if (std::holds_alternative<
                    boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream>>>(ws))
                performSslHandshake(passedHost);
            else
                performWebsocketHandshake();
        }

        void performSslHandshake(std::string const& host)
        {
            auto& wss =
                std::get<boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream>>>(ws);
            if (!SSL_set_tlsext_host_name(wss.next_layer().native_handle(), host.c_str()))
            {
                auto ec = boost::beast::error_code(
                    static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category());
                return onError({.error = ec, .additionalInfo = "SSL_set_tlsext_host_name failed."});
            }
            withStreamDo([&, this](auto& ws) {
                wss.next_layer().async_handshake(
                    boost::asio::ssl::stream_base::client, [self = shared_from_this()](boost::beast::error_code ec) {
                        self->onSslHandshake(std::move(ec));
                    });
            });
        }

        void onSslHandshake(boost::beast::error_code ec)
        {
            if (ec)
                return onError({.error = ec, .additionalInfo = "SSL handshake failed."});

            performWebsocketHandshake();
        }

        void performWebsocketHandshake()
        {
            withStreamDo([&, this](auto& ws) {
                boost::beast::get_lowest_layer(ws).expires_never();
                ws.set_option(
                    boost::beast::websocket::stream_base::timeout::suggested(boost::beast::role_type::client));

                ws.set_option(
                    boost::beast::websocket::stream_base::decorator([this](boost::beast::websocket::request_type& req) {
                        req.set(
                            boost::beast::http::field::user_agent, std::string(BOOST_BEAST_VERSION_STRING) + " Roar");
                        for (auto const& [key, value] : handshakeHeaders)
                            req.set(key, value);
                    }));

                ws.async_handshake(host, path, [self = shared_from_this()](boost::beast::error_code ec) {
                    self->onWebSocketHandshake(std::move(ec));
                });
            });
        }

        void onWebSocketHandshake(boost::beast::error_code ec)
        {
            if (ec)
                return onError({.error = ec, .additionalInfo = "WebSocket handshake failed."});

            onConnectComplete();
        }
    };
    //------------------------------------------------------------------------------------------------------------------
    WebSocketClient::WebSocketClient(ConstructionArguments&& args)
        : impl_{std::make_shared<Implementation>(std::move(args.executor), std::move(args.sslContext))}
    {}
    //------------------------------------------------------------------------------------------------------------------
    ROAR_PIMPL_SPECIAL_FUNCTIONS_IMPL(WebSocketClient);
    //------------------------------------------------------------------------------------------------------------------
    promise::Promise WebSocketClient::connect(ConnectParameters&& connectParameters)
    {
        impl_->handshakeHeaders = std::move(connectParameters.headers);
        impl_->path = std::move(connectParameters.path);

        return newPromise([this, connectParameters = std::move(connectParameters)](Defer d) {
            impl_->onConnectComplete = [d]() {
                d.resolve();
            };
            impl_->onError = [d](auto const& error) {
                d.reject(error);
            };
            impl_->withStreamDo([&, this](auto& ws) {
                boost::system::error_code ec;
                Dns::resolveAll(
                    ws.get_executor(),
                    connectParameters.host,
                    connectParameters.port,
                    [&, this](boost::asio::ip::tcp::resolver::results_type endpointIterator, auto) {
                        boost::beast::get_lowest_layer(ws).expires_after(connectParameters.timeout);
                        boost::beast::get_lowest_layer(ws).async_connect(
                            endpointIterator,
                            [impl = impl_->shared_from_this(),
                             timeout = std::move(connectParameters.timeout),
                             host = connectParameters.host](auto ec, auto const& endpoint) {
                                impl->onConnect(ec, endpoint, timeout, std::move(host));
                            });
                    },
                    ec);
                if (ec)
                    return impl_->onError({.error = ec, .additionalInfo = "Dns::resolveAll failed."});
            });
        });
    }
    //------------------------------------------------------------------------------------------------------------------
    promise::Promise WebSocketClient::send(std::string message)
    {
        return newPromise([this, message = std::move(message)](Defer d) {
            impl_->withStreamDo([&, this](auto& ws) {
                ws.async_write(
                    boost::asio::buffer(message),
                    [d, impl = impl_->shared_from_this()](auto ec, std::size_t bytesTransferred) {
                        if (ec)
                            return d.reject(Error{.error = ec, .additionalInfo = "WebSocket write failed."});
                        d.resolve(bytesTransferred);
                    });
            });
        });
    }
    //------------------------------------------------------------------------------------------------------------------
    promise::Promise WebSocketClient::read()
    {
        return newPromise([this](Defer d) {
            impl_->withStreamDo([&, this](auto& ws) {
                struct Buf
                {
                    std::string data;
                    boost::asio::dynamic_string_buffer<char, std::char_traits<char>, std::allocator<char>> buf =
                        boost::asio::dynamic_buffer(data);
                };
                auto buf = std::make_shared<Buf>();
                ws.async_read(
                    buf->buf, [d, impl = impl_->shared_from_this(), buf](auto ec, std::size_t bytesTransferred) {
                        if (ec)
                            return d.reject(Error{.error = ec, .additionalInfo = "WebSocket read failed."});
                        d.resolve(buf->data);
                    });
            });
        });
    }
    //------------------------------------------------------------------------------------------------------------------
    void WebSocketClient::close()
    {
        impl_->withStreamDo([&, this](auto& ws) {
            ws.close(boost::beast::websocket::close_code::normal);
        });
    }
    //##################################################################################################################
}