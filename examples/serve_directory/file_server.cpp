#include "file_server.hpp"

#include <iostream>

using namespace boost::beast::http;

Roar::ServeDecision FileServer::serve(
    Roar::Session& session,
    Roar::EmptyBodyRequest const& request,
    Roar::FileAndStatus const& fileAndStatus,
    Roar::ServeOptions<FileServer>& options)
{
    return Roar::ServeDecision::Continue;
}

Roar::ServeDecision FileServer::serveAppdata(
    Roar::Session& session,
    Roar::EmptyBodyRequest const& request,
    Roar::FileAndStatus const& fileAndStatus,
    Roar::ServeOptions<FileServer>& options)
{
    // You can do something like:
    /**
     * if (user has permissions)
     *   options.allowUpload = true;
     */
    return Roar::ServeDecision::Continue;
}

Roar::ServeDecision FileServer::customServeOptions(
    Roar::Session& session,
    Roar::EmptyBodyRequest const& request,
    Roar::FileAndStatus const& fileAndStatus,
    Roar::ServeOptions<FileServer>& options)
{
    return Roar::ServeDecision::Continue;
}