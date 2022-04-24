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

        Implementation(
            boost::asio::ip::tcp::socket&& socket,
            boost::beast::flat_buffer&& buffer,
            std::optional<boost::asio::ssl::context>& sslContext,
            bool isSecure,
            std::function<void(Error&&)> onError,
            std::weak_ptr<Router> router)
            : stream{[&socket, &sslContext]() -> decltype(stream) {
                if (sslContext)
                    return boost::beast::ssl_stream<boost::beast::tcp_stream>{std::move(socket), *sslContext};
                return boost::beast::tcp_stream{std::move(socket)};
            }()}
            , buffer{std::move(buffer)}
            , isSecure{isSecure}
            , onError{std::move(onError)}
            , router{std::move(router)}
            , headerParser{}
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
        std::weak_ptr<Router> router)
        : impl_{std::make_unique<Implementation>(
              std::move(socket),
              std::move(buffer),
              sslContext,
              isSecure,
              std::move(onError),
              std::move(router))}
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
                        Request<boost::beast::http::empty_body> extendedRequest{
                            std::move(self->impl_->headerParser->get())};
                        router->followRoute(*self, extendedRequest);
                    }
                });
        });
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
    //##################################################################################################################
}