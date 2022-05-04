#include "request_listener.hpp"

#include <iostream>

void RequestListener::ws(Roar::Session& session, Roar::EmptyBodyRequest&& request)
{
    session.upgrade(request)
        .then([weak = weak_from_this()](auto ws) {
            if (auto self = weak.lock(); self)
                self->ws_ = ws;
        })
        .fail([](Roar::Error const&) {
            std::cout << "Could not upgrade to websocket.\n";
        });
}