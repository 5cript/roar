#include <roar/websocket/websocket_session.hpp>
#include <roar/request.hpp>

#include <boost/beast/websocket.hpp>

#include <type_traits>

namespace Roar
{
    //##################################################################################################################
    struct WebsocketSession::Implementation
    {
        std::variant<
            boost::beast::websocket::stream<boost::beast::tcp_stream>,
            boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream>>>
            ws;

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
        void withWsDo(FunctionT&& func)
        {
            std::visit(std::forward<FunctionT>(func), ws);
        }
    };
    //------------------------------------------------------------------------------------------------------------------
    WebsocketSession::WebsocketSession(
        std::variant<boost::beast::tcp_stream, boost::beast::ssl_stream<boost::beast::tcp_stream>>&& stream)
        : impl_{std::make_unique<Implementation>(std::move(stream))}
    {}
    //------------------------------------------------------------------------------------------------------------------
    void WebsocketSession::accept(Request<boost::beast::http::empty_body> const& req)
    {
        impl_->withWsDo([this, request = req](auto& ws) {
            auto req = std::make_shared<Request<boost::beast::http::empty_body>>(std::move(request));
            ws.async_accept(*req, [req, self = shared_from_this()](auto&& ec) {

            });
        });
    }
    //------------------------------------------------------------------------------------------------------------------
    ROAR_PIMPL_SPECIAL_FUNCTIONS_IMPL(WebsocketSession);
    //##################################################################################################################
}