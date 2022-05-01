#include <boost/asio/ip/tcp.hpp>

#include <roar/session/session.hpp>
#include <roar/request.hpp>
#include <roar/routing/router.hpp>

#include <boost/beast/core/flat_buffer.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/basic_stream_socket.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/empty_body.hpp>
#include <boost/beast/http/read.hpp>

#include <chrono>
#include <variant>

namespace Roar
{
    //##################################################################################################################
    struct Session::Implementation
    {
        std::variant<boost::beast::tcp_stream, boost::beast::ssl_stream<boost::beast::tcp_stream>> stream;
        boost::beast::flat_buffer buffer;
        bool isSecure;
        std::function<void(Error&&)> onError;
        std::weak_ptr<Router> router;
        std::shared_ptr<boost::beast::http::request_parser<boost::beast::http::empty_body>> headerParser;
        RouteOptions routeOptions;
        std::shared_ptr<const StandardResponseProvider> standardResponseProvider;

        Implementation(
            boost::asio::ip::tcp::socket&& socket,
            boost::beast::flat_buffer&& buffer,
            std::optional<boost::asio::ssl::context>& sslContext,
            bool isSecure,
            std::function<void(Error&&)> onError,
            std::weak_ptr<Router> router,
            std::shared_ptr<const StandardResponseProvider> standardResponseProvider)
            : stream{[&socket, &sslContext, isSecure]() -> decltype(stream) {
                if (isSecure)
                    return boost::beast::ssl_stream<boost::beast::tcp_stream>{std::move(socket), *sslContext};
                return boost::beast::tcp_stream{std::move(socket)};
            }()}
            , buffer{std::move(buffer)}
            , isSecure{isSecure}
            , onError{std::move(onError)}
            , router{std::move(router)}
            , headerParser{}
            , routeOptions{}
            , standardResponseProvider{std::move(standardResponseProvider)}
        {}

        template <typename FunctionT>
        void withStreamDo(FunctionT&& func)
        {
            std::visit(std::forward<FunctionT>(func), stream);
        }
    };
    //##################################################################################################################
    Session::Session(
        boost::asio::ip::tcp::socket&& socket,
        boost::beast::flat_buffer&& buffer,
        std::optional<boost::asio::ssl::context>& sslContext,
        bool isSecure,
        std::function<void(Error&&)> onError,
        std::weak_ptr<Router> router,
        std::shared_ptr<const StandardResponseProvider> standardResponseProvider)
        : impl_{std::make_unique<Implementation>(
              std::move(socket),
              std::move(buffer),
              sslContext,
              isSecure,
              std::move(onError),
              std::move(router),
              std::move(standardResponseProvider))}
    {}
    //------------------------------------------------------------------------------------------------------------------
    ROAR_PIMPL_SPECIAL_FUNCTIONS_IMPL(Session);
    //------------------------------------------------------------------------------------------------------------------
    void Session::startup()
    {
        if (std::holds_alternative<boost::beast::tcp_stream>(impl_->stream))
        {
            boost::asio::dispatch(
                std::get<boost::beast::tcp_stream>(impl_->stream).get_executor(), [self = this->shared_from_this()]() {
                    self->readHeader();
                });
        }
        else
        {
            boost::asio::dispatch(
                std::get<boost::beast::ssl_stream<boost::beast::tcp_stream>>(impl_->stream).get_executor(),
                [self = this->shared_from_this()]() {
                    self->performSslHandshake();
                });
        }
    }
    //------------------------------------------------------------------------------------------------------------------
    void Session::readHeader()
    {
        // reset parser.
        impl_->headerParser = std::make_shared<boost::beast::http::request_parser<boost::beast::http::empty_body>>();

        impl_->withStreamDo([this](auto& stream) {
            boost::beast::get_lowest_layer(stream).expires_after(sessionTimeout);
            impl_->headerParser->header_limit(defaultHeaderLimit);

            // Do not attempt to read available body bytes.
            impl_->headerParser->eager(false);

            boost::beast::http::async_read_header(
                stream,
                impl_->buffer,
                *impl_->headerParser,
                [self = this->shared_from_this()](boost::beast::error_code ec, std::size_t) {
                    if (ec)
                    {
                        switch (static_cast<boost::beast::http::error>(ec.value()))
                        {
                            case boost::beast::http::error::end_of_stream:
                            {
                                return self->close();
                            }
                            default:
                            {
                                return self->impl_->onError(
                                    {.error = ec, .additionalInfo = "Error during header read."});
                            }
                        }
                    }

                    if (auto router = self->impl_->router.lock(); router)
                    {
                        router->followRoute(
                            *self, Request<boost::beast::http::empty_body>{self->impl_->headerParser->get()});
                    }
                });
        });
    }
    //------------------------------------------------------------------------------------------------------------------
    bool Session::isSecure() const
    {
        return std::holds_alternative<boost::beast::ssl_stream<boost::beast::tcp_stream>>(impl_->stream);
    }
    //------------------------------------------------------------------------------------------------------------------
    void Session::performSslHandshake()
    {
        std::get<boost::beast::ssl_stream<boost::beast::tcp_stream>>(impl_->stream)
            .async_handshake(
                boost::asio::ssl::stream_base::server,
                impl_->buffer.data(),
                [self = this->shared_from_this()](boost::beast::error_code ec, std::size_t bytesUsed) {
                    if (ec)
                        return self->impl_->onError({.error = ec, .additionalInfo = "Error during SSL handshake."});

                    self->impl_->buffer.consume(bytesUsed);
                    self->readHeader();
                });
    }
    //------------------------------------------------------------------------------------------------------------------
    RouteOptions const& Session::routeOptions()
    {
        return impl_->routeOptions;
    }
    //------------------------------------------------------------------------------------------------------------------
    void Session::setupRouteOptions(RouteOptions options)
    {
        impl_->routeOptions = std::move(options);
    }
    //------------------------------------------------------------------------------------------------------------------
    std::variant<boost::beast::tcp_stream, boost::beast::ssl_stream<boost::beast::tcp_stream>>& Session::stream()
    {
        return impl_->stream;
    }
    //------------------------------------------------------------------------------------------------------------------
    std::shared_ptr<boost::beast::http::request_parser<boost::beast::http::empty_body>>& Session::parser()
    {
        return impl_->headerParser;
    }
    //------------------------------------------------------------------------------------------------------------------
    boost::beast::flat_buffer& Session::buffer()
    {
        return impl_->buffer;
    }
    //------------------------------------------------------------------------------------------------------------------
    StandardResponseProvider const& Session::standardResponseProvider()
    {
        return *impl_->standardResponseProvider;
    }
    //------------------------------------------------------------------------------------------------------------------
    std::shared_ptr<WebsocketSession> Session::upgrade(Request<boost::beast::http::empty_body> const& req)
    {
        if (req.isWebsocketUpgrade())
        {
            auto ws = std::make_shared<WebsocketSession>(std::move(impl_->stream));
            ws->accept(req);
            return ws;
        }
        return {};
    }
    //------------------------------------------------------------------------------------------------------------------
    void Session::onWriteComplete(bool expectsClose, boost::beast::error_code ec, std::size_t)
    {
        if (ec && ec != boost::beast::http::error::end_of_stream)
            impl_->onError({.error = ec, .additionalInfo = "Error during write."});

        if (ec || expectsClose)
            return close();
    }
    //------------------------------------------------------------------------------------------------------------------
    void Session::close()
    {
        boost::beast::error_code ec;
        if (std::holds_alternative<boost::beast::tcp_stream>(impl_->stream))
        {
            std::get<boost::beast::tcp_stream>(impl_->stream)
                .socket()
                .shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
        }
        else
        {
            auto& stream = std::get<boost::beast::ssl_stream<boost::beast::tcp_stream>>(impl_->stream);
            boost::beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(sessionTimeout));
            stream.shutdown(ec);
        }
        // That the remote is not connected anymore is not an error if the shutdown was ordered anyway.
        if (ec && ec != boost::system::errc::not_connected)
        {
            impl_->onError({.error = ec, .additionalInfo = "Error during stream shutdown."});
        }
    }
    //------------------------------------------------------------------------------------------------------------------
    void Session::sendStandardResponse(boost::beast::http::status status, std::string_view additionalInfo)
    {
        send(impl_->standardResponseProvider->makeStandardResponse(*this, status, additionalInfo));
    }
    //------------------------------------------------------------------------------------------------------------------
    void Session::sendStrictTransportSecurityResponse()
    {
        auto res =
            impl_->standardResponseProvider->makeStandardResponse(*this, boost::beast::http::status::forbidden, "");
        res.set(boost::beast::http::field::strict_transport_security, "max-age=3600");
        send(std::move(res));
    }
    //##################################################################################################################
}