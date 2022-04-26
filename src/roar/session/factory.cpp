#include <boost/asio/ip/tcp.hpp>

#include <roar/session/factory.hpp>
#include <roar/session/session.hpp>

#include <boost/asio/ssl/context.hpp>
#include <boost/asio/basic_stream_socket.hpp>
#include <boost/beast/core/detect_ssl.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/stream_traits.hpp>

#include <chrono>

namespace Roar
{
    //##################################################################################################################
    struct Factory::ProtoSession
    {
        boost::beast::tcp_stream stream;
        boost::beast::flat_buffer buffer;

        ProtoSession(boost::asio::ip::tcp::socket&& socket)
            : stream{std::move(socket)}
            , buffer{}
        {}
    };
    //##################################################################################################################
    struct Factory::Implementation
    {
        std::optional<boost::asio::ssl::context>& sslContext;
        std::function<void(Error&&)> onError;

        Implementation(std::optional<boost::asio::ssl::context>& sslContext, std::function<void(Error&&)> onError)
            : sslContext{sslContext}
            , onError{std::move(onError)}
        {}
    };
    //##################################################################################################################
    Factory::Factory(std::optional<boost::asio::ssl::context>& sslContext, std::function<void(Error&&)> onError)
        : impl_{std::make_unique<Implementation>(sslContext, std::move(onError))}
    {}
    //------------------------------------------------------------------------------------------------------------------
    ROAR_PIMPL_SPECIAL_FUNCTIONS_IMPL(Factory);
    //------------------------------------------------------------------------------------------------------------------
    void Factory::makeSession(
        boost::asio::basic_stream_socket<boost::asio::ip::tcp>&& socket,
        std::weak_ptr<Router> router,
        std::shared_ptr<const StandardResponseProvider> standardResponseProvider)
    {
        auto protoSession = std::make_shared<ProtoSession>(std::move(socket));
        boost::beast::get_lowest_layer(protoSession->stream).expires_after(std::chrono::seconds(sslDetectionTimeout));
        boost::beast::async_detect_ssl(
            protoSession->stream,
            protoSession->buffer,
            [this,
             protoSession,
             router = std::move(router),
             standardResponseProvider =
                 std::move(standardResponseProvider)](boost::beast::error_code ec, bool isSecure) {
                try
                {
                    if (ec)
                    {
                        if (ec == boost::beast::http::error::end_of_stream ||
                            ec == boost::asio::error::misc_errors::eof)
                            return;

                        return impl_->onError({.error = ec, .additionalInfo = "Error in SSL detector."});
                    }

                    if (isSecure && !impl_->sslContext)
                        return impl_->onError({.error = "SSL handshake received on insecure server."});

                    std::make_shared<Session>(
                        protoSession->stream.release_socket(),
                        std::move(protoSession->buffer),
                        impl_->sslContext,
                        isSecure,
                        impl_->onError,
                        router,
                        std::move(standardResponseProvider))
                        ->startup();
                }
                catch (std::exception const& exc)
                {
                    return impl_->onError({.error = exc.what(), .additionalInfo = "Exception in session factory."});
                }
            });
    }
    //##################################################################################################################
}