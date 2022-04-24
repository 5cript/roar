#pragma once

#include <roar/routing/request_listener.hpp>

class RequestListener
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
    BOOST_DESCRIBE_CLASS(RequestListener, (), (), (), (roar_ws))
};