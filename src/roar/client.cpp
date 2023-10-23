#include <roar/client.hpp>

#include <boost/fusion/include/at_c.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/utility/annotate_on_success.hpp>
#include <boost/spirit/home/x3/support/utility/error_reporting.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/read_until.hpp>

namespace Roar
{
    using namespace promise;

    // ##################################################################################################################
    Client::Client(ConstructionArguments&& args)
        : socket_{[&args]() -> decltype(socket_) {
            if (args.sslContext)
                return boost::beast::ssl_stream<boost::beast::tcp_stream>{
                    boost::beast::tcp_stream{args.executor}, *args.sslContext};
            else
                return boost::beast::tcp_stream{args.executor};
        }()}
    {
        if (std::holds_alternative<boost::beast::ssl_stream<boost::beast::tcp_stream>>(socket_))
        {
            auto& sslSocket = std::get<boost::beast::ssl_stream<boost::beast::tcp_stream>>(socket_);
            sslSocket.set_verify_mode(args.sslVerifyMode);
            if (args.sslVerifyCallback)
                sslSocket.set_verify_callback(std::move(args.sslVerifyCallback));
        }
    }
    //------------------------------------------------------------------------------------------------------------------
    Client::~Client()
    {
        shutdownSync();
    }
    //------------------------------------------------------------------------------------------------------------------
    Detail::PromiseTypeBind<Detail::PromiseTypeBindThen<std::size_t>, Detail::PromiseTypeBindFail<Error>>
    Client::send(std::string message, std::chrono::seconds timeout)
    {
        return newPromise([this, message = std::move(message), timeout](Defer d) mutable {
            withStreamDo([this, &d, &message, timeout](auto& socket) mutable {
                withLowerLayerDo([&](auto& socket) {
                    socket.expires_after(timeout);
                });
                std::shared_ptr<std::string> messagePtr = std::make_shared<std::string>(std::move(message));
                boost::asio::async_write(
                    socket,
                    boost::asio::buffer(*messagePtr),
                    [d = std::move(d), weak = weak_from_this(), messagePtr](auto ec, std::size_t bytesTransferred) {
                        auto self = weak.lock();
                        if (!self)
                            return d.reject(Error{.error = ec, .additionalInfo = "Client is no longer alive."});

                        if (ec)
                            return d.reject(Error{.error = ec, .additionalInfo = "Stream write failed."});
                        d.resolve(bytesTransferred);
                    });
            });
        });
    }
    //------------------------------------------------------------------------------------------------------------------
    Detail::
        PromiseTypeBind<Detail::PromiseTypeBindThen<std::string_view, std::size_t>, Detail::PromiseTypeBindFail<Error>>
        Client::read(std::chrono::seconds timeout)
    {
        return newPromise([this, timeout](Defer d) {
            withLowerLayerDo([timeout](auto& socket) {
                socket.expires_after(timeout);
            });
            withStreamDo([this, d = std::move(d)](auto& socket) mutable {
                struct Buf
                {
                    std::string data = std::string(4096, '\0');
                };
                auto buf = std::make_shared<Buf>();
                boost::asio::async_read(
                    socket,
                    boost::asio::buffer(buf->data),
                    [d = std::move(d), weak = weak_from_this(), buf](auto ec, std::size_t bytesTransferred) {
                        auto self = weak.lock();
                        if (!self)
                            return d.reject(Error{.error = ec, .additionalInfo = "Client is no longer alive."});

                        if (ec)
                            return d.reject(Error{.error = ec, .additionalInfo = "Stream read failed."});

                        d.resolve(std::string_view{buf->data.data(), bytesTransferred}, bytesTransferred);
                    });
            });
        });
    }
    //------------------------------------------------------------------------------------------------------------------
    void Client::doResolve(
        std::string const& host,
        std::string const& port,
        std::function<void(boost::beast::error_code ec, boost::asio::ip::tcp::resolver::results_type results)>
            onResolve)
    {
        withLowerLayerDo([&](auto& socket) {
            struct ResolveCtx
            {
                boost::asio::ip::tcp::resolver resolver;
                boost::asio::ip::tcp::resolver::query query;
            };
            auto resolveCtx = std::make_shared<ResolveCtx>(ResolveCtx{
                .resolver = boost::asio::ip::tcp::resolver{socket.get_executor()},
                .query =
                    boost::asio::ip::tcp::resolver::query{
                        host, port, boost::asio::ip::resolver_query_base::numeric_service},
            });
            resolveCtx->resolver.async_resolve(
                resolveCtx->query,
                [onResolve = std::move(onResolve), resolveCtx](boost::beast::error_code ec, auto results) mutable {
                    onResolve(ec, std::move(results));
                });
        });
    }
    // ##################################################################################################################
}