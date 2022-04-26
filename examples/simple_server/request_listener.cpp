#include "request_listener.hpp"

#include <iostream>

void RequestListener::index(Roar::Session& session, Roar::EmptyBodyRequest&& request)
{
    session.template prepareResponse<boost::beast::http::string_body>(request)
        .contentType("text/plain")
        .status(boost::beast::http::status::ok)
        .body("Hello World!")
        .preparePayload()
        .send(session);
}

void RequestListener::upload(Roar::Session& session, Roar::EmptyBodyRequest&& request)
{
    session.template read<boost::beast::http::string_body>(std::move(request))
        ->noBodyLimit()
        .start([](auto& session, auto const& request) {
            // TODO: handle exceptions.
            std::cout << request.json().dump() << "\n";

            session.template prepareResponse<boost::beast::http::string_body>(request)
                .contentType("text/plain")
                .status(boost::beast::http::status::ok)
                .body("Thanks!")
                .preparePayload()
                .send(session);
        });
}