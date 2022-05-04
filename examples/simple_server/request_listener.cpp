#include "request_listener.hpp"

#include <iostream>

using namespace boost::beast::http;

void RequestListener::index(Roar::Session& session, Roar::EmptyBodyRequest&& request)
{
    session.send<string_body>(request)->status(status::ok).contentType("text/plain").body("Hi").commit();
}

void RequestListener::upload(Roar::Session& session, Roar::EmptyBodyRequest&& request)
{
    using namespace Roar;
    session.template read<boost::beast::http::string_body>(std::move(request))
        ->noBodyLimit()
        .commit()
        .then([](auto& session, auto const& request) {
            std::cout << request.json().dump() << "\n";

            session.template prepareResponse<boost::beast::http::string_body>(request)
                .contentType("text/plain")
                .status(boost::beast::http::status::ok)
                .body("Thanks!")
                .preparePayload()
                .send(session);
        });
}