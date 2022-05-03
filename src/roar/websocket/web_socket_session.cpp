#include <roar/websocket/web_socket_session.hpp>
#include <roar/request.hpp>
#include <roar/error.hpp>

#include <boost/beast/websocket.hpp>

#include <type_traits>

namespace Roar
{
    using namespace promise;

    //##################################################################################################################
    struct WebSocketSession::Implementation
    {
        std::variant<
            boost::beast::websocket::stream<boost::beast::tcp_stream>,
            boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream>>>
            ws;
        boost::beast::flat_buffer buffer;

        Implementation(
            std::variant<boost::beast::tcp_stream, boost::beast::ssl_stream<boost::beast::tcp_stream>>&& stream)
            : ws{[&stream]() {
                return std::visit(
                    []<typename StreamT>(StreamT&& stream) -> decltype(ws) {
                        if constexpr (std::is_same_v<std::decay_t<StreamT>, boost::beast::tcp_stream>)
                            return boost::beast::websocket::stream<boost::beast::tcp_stream>{std::move(stream)};
                        else
                            return boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream>>{
                                std::move(stream)};
                    },
                    std::move(stream));
            }()}
        {}

        template <typename FunctionT>
        void withStreamDo(FunctionT&& func)
        {
            std::visit(std::forward<FunctionT>(func), ws);
        }
    };
    //------------------------------------------------------------------------------------------------------------------
    WebSocketSession::WebSocketSession(
        std::variant<boost::beast::tcp_stream, boost::beast::ssl_stream<boost::beast::tcp_stream>>&& stream)
        : impl_{std::make_unique<Implementation>(std::move(stream))}
    {}
    //------------------------------------------------------------------------------------------------------------------
    promise::Promise WebSocketSession::accept(Request<boost::beast::http::empty_body> const& req)
    {
        return newPromise([this, &req](Defer d) {
            impl_->withStreamDo([this, request = req, &d](auto& ws) {
                auto req = std::make_shared<Request<boost::beast::http::empty_body>>(std::move(request));
                ws.async_accept(*req, [req, self = shared_from_this(), d = std::move(d)](auto&& ec) {
                    if (ec)
                        d.reject(Error{.error = ec});
                    else
                        d.resolve();
                });
            });
        });
    }
    //------------------------------------------------------------------------------------------------------------------
    promise::Promise WebSocketSession::send(std::string message)
    {
        return newPromise([this, message = std::move(message)](Defer d) {
            impl_->withStreamDo([&, this](auto& ws) {
                ws.async_write(
                    boost::asio::buffer(message),
                    [d, impl = shared_from_this()](auto ec, std::size_t bytesTransferred) {
                        if (ec)
                            return d.reject(Error{.error = ec, .additionalInfo = "WebSocket write failed."});
                        d.resolve(bytesTransferred);
                    });
            });
        });
    }
    //------------------------------------------------------------------------------------------------------------------
    promise::Promise WebSocketSession::read()
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
                ws.async_read(buf->buf, [d, self = shared_from_this(), buf](auto ec, std::size_t bytesTransferred) {
                    if (ec)
                        return d.reject(Error{.error = ec, .additionalInfo = "WebSocket read failed."});
                    d.resolve(buf->data);
                });
            });
        });
    }
    //------------------------------------------------------------------------------------------------------------------
    void WebSocketSession::close()
    {
        impl_->withStreamDo([&, this](auto& ws) {
            ws.close(boost::beast::websocket::close_code::normal);
        });
    }
    //------------------------------------------------------------------------------------------------------------------
    ROAR_PIMPL_SPECIAL_FUNCTIONS_IMPL(WebSocketSession);
    //##################################################################################################################
}