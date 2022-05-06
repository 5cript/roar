#include <roar/websocket/websocket_session.hpp>
#include <roar/request.hpp>
#include <roar/error.hpp>

#include <boost/beast/websocket.hpp>

#include <type_traits>

namespace Roar
{
    using namespace promise;

    //##################################################################################################################
    struct WebsocketSession::Implementation
    {
        boost::beast::flat_buffer buffer;

        Implementation()
        {}
    };
    //------------------------------------------------------------------------------------------------------------------
    WebsocketSession::WebsocketSession(
        std::variant<Detail::StreamType, boost::beast::ssl_stream<Detail::StreamType>>&& stream)
        : SharedFromBase(std::move(stream))
        , impl_{std::make_unique<Implementation>()}
    {}
    //------------------------------------------------------------------------------------------------------------------
    WebsocketSession::~WebsocketSession() = default;
    //------------------------------------------------------------------------------------------------------------------
    Detail::PromiseTypeBind<Detail::PromiseTypeBindThen<>, Detail::PromiseTypeBindFail<Error const&>>
    WebsocketSession::accept(Request<boost::beast::http::empty_body> const& req)
    {
        return newPromise([this, &req](Defer d) {
            withStreamDo([this, request = req, &d](auto& ws) {
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
    //##################################################################################################################
}