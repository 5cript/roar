#include "file_server.hpp"

#include <iostream>

using namespace boost::beast::http;

Roar::ServeDecision FileServer::serve(
    Roar::Session& session,
    Roar::EmptyBodyRequest const& request,
    Roar::FileAndStatus const& fileAndStatus)
{
    return Roar::ServeDecision::Deny;
}

Roar::ServeDecision FileServer::serveAppdata(
    Roar::Session& session,
    Roar::EmptyBodyRequest const& request,
    Roar::FileAndStatus const& fileAndStatus)
{
    return Roar::ServeDecision::Deny;
}