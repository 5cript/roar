#include <boost/beast/core/stream_traits.hpp>
#include <roar/websocket/websocket_client.hpp>

#include <roar/dns/resolve.hpp>

#include <boost/asio/buffer.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/strand.hpp>

#include <stdexcept>

namespace Roar
{
    using namespace promise;

    // ##################################################################################################################
    struct WebsocketClient::Implementation
    {
        boost::beast::flat_buffer buffer;
        std::function<void(Error&&)> onError;
        std::string host;
        std::unordered_map<boost::beast::http::field, std::string> handshakeHeaders;
        std::string path;
        std::function<void()> onConnectComplete;

        Implementation()
            : buffer{}
            , onError{}
            , host{}
            , handshakeHeaders{}
            , path{}
            , onConnectComplete{}
        {}

        void onConnect(
            boost::beast::error_code ec,
            boost::asio::ip::tcp::resolver::results_type::endpoint_type const& ep,
            std::chrono::seconds timeout,
            std::string passedHost,
            std::shared_ptr<WebsocketClient> client)
        {
            host = passedHost + ':' + std::to_string(ep.port());

            if (ec)
                return onError({.error = ec, .additionalInfo = "Could not connect."});

            client->withStreamDo([&timeout](auto& ws) {
                boost::beast::get_lowest_layer(ws).expires_after(timeout);
            });

            if (std::holds_alternative<boost::beast::websocket::stream<boost::beast::ssl_stream<Detail::StreamType>>>(
                    client->ws_))
                performSslHandshake(passedHost, std::move(client));
            else
                performWebsocketHandshake(std::move(client));
        }

        void performSslHandshake(std::string const& origHost, std::shared_ptr<WebsocketClient> client)
        {
            auto& wss =
                std::get<boost::beast::websocket::stream<boost::beast::ssl_stream<Detail::StreamType>>>(client->ws_);
            if (!SSL_ctrl(
                    wss.next_layer().native_handle(),
                    SSL_CTRL_SET_TLSEXT_HOSTNAME,
                    TLSEXT_NAMETYPE_host_name,
                    // yikes openssl you make me do this
                    const_cast<void*>(reinterpret_cast<void const*>(origHost.c_str()))))
            {
                auto ec = boost::beast::error_code(
                    static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category());
                return onError({.error = ec, .additionalInfo = "SSL_set_tlsext_host_name failed."});
            }
            wss.next_layer().async_handshake(
                boost::asio::ssl::stream_base::client, [client](boost::beast::error_code ec) {
                    client->impl_->onSslHandshake(std::move(ec), client);
                });
        }

        void onSslHandshake(boost::beast::error_code ec, std::shared_ptr<WebsocketClient> client)
        {
            if (ec)
                return onError({.error = ec, .additionalInfo = "SSL handshake failed."});

            performWebsocketHandshake(std::move(client));
        }

        void performWebsocketHandshake(std::shared_ptr<WebsocketClient> client)
        {
            client->withStreamDo([&, this](auto& ws) {
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

                ws.async_handshake(host, path, [client](boost::beast::error_code ec) {
                    client->impl_->onWebsocketHandshake(std::move(ec));
                });
            });
        }

        void onWebsocketHandshake(boost::beast::error_code ec)
        {
            if (ec)
                return onError({.error = ec, .additionalInfo = "Websocket handshake failed."});
            onConnectComplete();
            onConnectComplete = []() {};
            onError = [](Error const&) {};
        }
    };
    //------------------------------------------------------------------------------------------------------------------
    WebsocketClient::WebsocketClient(ConstructionArguments&& args)
        // NOLINTNEXTLINE
        : SharedFromBase{[&args]() -> decltype(ws_) {
            if (args.sslContext)
                return boost::beast::websocket::stream<boost::beast::ssl_stream<Detail::StreamType>>{
                    boost::asio::make_strand(args.executor), *args.sslContext};
            else
                return boost::beast::websocket::stream<Detail::StreamType>{boost::asio::make_strand(args.executor)};
        }()}
        , impl_{std::make_unique<Implementation>()}
    {}
    //------------------------------------------------------------------------------------------------------------------
    WebsocketClient::~WebsocketClient() = default;
    //------------------------------------------------------------------------------------------------------------------
    Detail::PromiseTypeBind<Detail::PromiseTypeBindThen<>, Detail::PromiseTypeBindFail<Error>>
    WebsocketClient::connect(ConnectParameters&& connectParameters)
    {
        impl_->handshakeHeaders = std::move(connectParameters.headers);
        impl_->path = std::move(connectParameters.path);

        return newPromise([this, connectParameters = std::move(connectParameters)](Defer d) {
            impl_->onConnectComplete = [d]() {
                d.resolve();
            };
            impl_->onError = [d](Error const& error) {
                d.reject(error);
            };
            withStreamDo([&, this](auto& ws) {
                boost::system::error_code ec;
                Dns::resolveAll(
                    ws.get_executor(),
                    connectParameters.host,
                    connectParameters.port,
                    [&, this](boost::asio::ip::tcp::resolver::results_type endpointIterator, auto) {
                        boost::beast::get_lowest_layer(ws).expires_after(connectParameters.timeout);
                        boost::beast::get_lowest_layer(ws).async_connect(
                            endpointIterator,
                            [self = shared_from_this(),
                             timeout = std::move(connectParameters.timeout),
                             host = connectParameters.host](auto ec, auto const& endpoint) {
                                self->impl_->onConnect(ec, endpoint, timeout, std::move(host), self);
                            });
                    },
                    ec);
                if (ec)
                    return impl_->onError({.error = ec, .additionalInfo = "Dns::resolveAll failed."});
            });
        });
    }
    // ##################################################################################################################
}