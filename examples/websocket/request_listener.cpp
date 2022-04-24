#include "request_listener.hpp"

#include <iostream>

void RequestListener::ws(Roar::Session& session, Roar::EmptyBodyRequest&& request)
{
    if (auto ws = session.upgrade(request); ws)
    {
        // TODO:
    }
}