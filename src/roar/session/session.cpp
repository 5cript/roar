#include <boost/asio/ip/tcp.hpp>

#include <roar/session/session.hpp>

#include <boost/beast/core/flat_buffer.hpp>
#include <boost/asio/basic_stream_socket.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <boost/beast/core/tcp_stream.hpp>

#include <chrono>
#include <variant>

namespace Roar::Session
{
    namespace
    {
        constexpr static std::chrono::seconds SSL_DETECTION_TIMEOUT{10};
        constexpr static std::chrono::seconds SESSION_TIMEOUT{10};
    }
    //##################################################################################################################
    struct Session::Implementation
    {
        std::variant<boost::beast::tcp_stream, boost::beast::ssl_stream<boost::beast::tcp_stream>> stream;
        boost::beast::flat_buffer buffer;
        bool isSecure;
        std::function<void(Error&&)> onError;

        Implementation(
            boost::asio::ip::tcp::socket&& socket,
            boost::beast::flat_buffer&& buffer,
            std::optional<boost::asio::ssl::context>& sslContext,
            bool isSecure,
            std::function<void(Error&&)> onError)
            : stream{[&socket, &sslContext]() -> decltype(stream) {
                if (sslContext)
                    return boost::beast::ssl_stream<boost::beast::tcp_stream>{std::move(socket), *sslContext};
                return boost::beast::tcp_stream{std::move(socket)};
            }()}
            , buffer{std::move(buffer)}
            , isSecure{isSecure}
            , onError{std::move(onError)}
        {}
    };
    //##################################################################################################################
    Session::Session(
        boost::asio::ip::tcp::socket&& socket,
        boost::beast::flat_buffer&& buffer,
        std::optional<boost::asio::ssl::context>& sslContext,
        bool isSecure,
        std::function<void(Error&&)> onError)
        : impl_{std::make_unique<Implementation>(
              std::move(socket),
              std::move(buffer),
              sslContext,
              isSecure,
              std::move(onError))}
    {}
    //------------------------------------------------------------------------------------------------------------------
    ROAR_PIMPL_SPECIAL_FUNCTIONS_IMPL(Session);
    //------------------------------------------------------------------------------------------------------------------
    void Session::startup()
    {
        //
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
            boost::beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(SESSION_TIMEOUT));
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