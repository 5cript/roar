#include <roar/websocket/websocket_base.hpp>

#include <future>

namespace Roar
{
    using namespace promise;

    //##################################################################################################################
    WebsocketBase::WebsocketBase(
        std::variant<boost::beast::tcp_stream, boost::beast::ssl_stream<boost::beast::tcp_stream>>&& stream)
        : ws_{[&stream]() {
            return std::visit(
                []<typename StreamT>(StreamT&& stream) -> decltype(ws_) {
                    if constexpr (std::is_same_v<std::decay_t<StreamT>, boost::beast::tcp_stream>)
                        return boost::beast::websocket::stream<boost::beast::tcp_stream>{std::move(stream)};
                    else
                        return boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream>>{
                            std::move(stream)};
                },
                std::move(stream));
        }()}
    {}
    //------------------------------------------------------------------------------------------------------------------
    WebsocketBase::WebsocketBase(std::variant<
                                 boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream>>,
                                 boost::beast::websocket::stream<boost::beast::tcp_stream>>&& ws)
        : ws_{std::move(ws)}
    {}
    //------------------------------------------------------------------------------------------------------------------
    WebsocketBase::~WebsocketBase() = default;
    //------------------------------------------------------------------------------------------------------------------
    void WebsocketBase::readMessageMax(std::size_t amount)
    {
        withStreamDo([amount](auto& stream) {
            stream.read_message_max(amount);
        });
    }
    //------------------------------------------------------------------------------------------------------------------
    std::size_t WebsocketBase::readMessageMax() const
    {
        return withStreamDo([](auto& stream) {
            return stream.read_message_max();
        });
    }
    //------------------------------------------------------------------------------------------------------------------
    void WebsocketBase::binary(bool enable)
    {
        withStreamDo([enable](auto& stream) {
            stream.binary(enable);
        });
    }
    //------------------------------------------------------------------------------------------------------------------
    bool WebsocketBase::binary() const
    {
        return WebsocketBase::withStreamDo([](auto& stream) {
            return stream.binary();
        });
    }
    //------------------------------------------------------------------------------------------------------------------
    promise::Promise WebsocketBase::send(std::string message)
    {
        return newPromise([this, message = std::move(message)](Defer d) {
            withStreamDo([&, this](auto& ws) {
                ws.async_write(
                    boost::asio::buffer(message),
                    [d, self = shared_from_this()](auto ec, std::size_t bytesTransferred) {
                        if (ec)
                            return d.reject(Error{.error = ec, .additionalInfo = "Websocket write failed."});
                        d.resolve(bytesTransferred);
                    });
            });
        });
    }
    //------------------------------------------------------------------------------------------------------------------
    Detail::PromiseTypeBind<Detail::PromiseTypeBindThen<WebsocketReadResult>, Detail::PromiseTypeBindFail<Error const&>>
    WebsocketBase::read()
    {
        return newPromise([this](Defer d) {
            withStreamDo([&, this](auto& ws) {
                struct Buf
                {
                    std::string data;
                    boost::asio::dynamic_string_buffer<char, std::char_traits<char>, std::allocator<char>> buf =
                        boost::asio::dynamic_buffer(data);
                };
                auto buf = std::make_shared<Buf>();
                ws.async_read(buf->buf, [d, self = shared_from_this(), buf](auto ec, std::size_t bytesTransferred) {
                    if (ec)
                        return d.reject(Error{.error = ec, .additionalInfo = "Websocket read failed."});
                    self->withStreamDo([&](auto& ws) {
                        d.resolve(WebsocketReadResult{
                            .isBinary = ws.got_binary(),
                            .isMessageDone = ws.is_message_done(),
                            .message = std::move(buf->data)});
                    });
                });
            });
        });
    }
    //------------------------------------------------------------------------------------------------------------------
    void WebsocketBase::autoFragment(bool enable)
    {
        withStreamDo([this, enable](auto& ws) {
            ws.auto_fragment(enable);
        });
    }
    //------------------------------------------------------------------------------------------------------------------
    bool WebsocketBase::autoFragment() const
    {
        return withStreamDo([this](auto& ws) {
            return ws.auto_fragment();
        });
    }
    //------------------------------------------------------------------------------------------------------------------
    bool WebsocketBase::close(std::chrono::seconds closeWaitTimeout)
    {
        auto waitForClose = std::make_shared<std::promise<void>>();
        withStreamDo([&, this](auto& ws) {
            ws.async_close(
                boost::beast::websocket::close_code::normal, [waitForClose, self = shared_from_this()](auto ec) {
                    waitForClose->set_value();
                });
        });
        auto future = waitForClose->get_future();
        return future.wait_for(closeWaitTimeout) == std::future_status::ready;
    }
    //##################################################################################################################
}