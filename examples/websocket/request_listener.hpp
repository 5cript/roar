#pragma once

#include <roar/routing/request_listener.hpp>
#include <roar/websocket/websocket_session.hpp>

#include <memory>

class RequestListener : public std::enable_shared_from_this<RequestListener>
{
  private:
    ROAR_MAKE_LISTENER(RequestListener);

    ROAR_GET(ws)
    ({
        .path = "/api/ws",
        .routeOptions =
            {
                .expectUpgrade = true,
            },
    });

  private:
    std::shared_ptr<Roar::WebsocketSession> ws_;

  private:
    BOOST_DESCRIBE_CLASS(RequestListener, (), (), (), (roar_ws))
};