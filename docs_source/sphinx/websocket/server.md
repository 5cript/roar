# Websocket Server

There is no pure websocket server. Instead you have to setup an http server and upgrade your session to a websocket session.
Here is an example:

```c++
---
#include <roar/routing/request_listener.hpp>
#include <roar/websocket/websocket_session.hpp>

#include <iostream>

class RequestListener : public std::enable_shared_from_this<RequestListener>
{
  private:
    ROAR_MAKE_LISTENER(RequestListener);

    ROAR_GET(ws)
    ({
        .path = "/api/ws",
        .routeOptions = {.expectUpgrade = true}, // reject normal requests here.
    });

  private:
    BOOST_DESCRIBE_CLASS(RequestListener, (), (), (), (roar_ws))
};

void RequestListener::ws(Roar::Session& session, Roar::EmptyBodyRequest&& request)
{
    // Perform upgrade...
    session.upgrade(request)
        .then([weak = weak_from_this()](auto ws) {
            // ...and obtain the websocket here. ws is a shared_ptr<WebsocketSession>.
            if (auto self = weak.lock(); self)
            {
                // now you can do something with the Websocket:
                // ...
            }
        })
        .fail([](Roar::Error const&) {
            std::cout << "Could not upgrade to websocket.\n";
        });
}
```