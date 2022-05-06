#include "request_listener.hpp"

#include <iostream>

using namespace boost::beast::http;

void RequestListener::putHere(Roar::Session& session, Roar::EmptyBodyRequest&& request)
{
    if (request.expectsContinue())
    {
        auto contentLength = request.contentLength();
        bool allow = contentLength && request.contentLength() <= 1024;

        if (!allow)
            session.send<empty_body>(request)
                ->status(status::expectation_failed)
                .setHeader(field::connection, "close")
                .commit()
                .fail([](Roar::Error const&) {
                    std::cout << "Failed to send expectation failure.\n";
                });
        else
        {
            session.send<empty_body>(request)
                ->status(status::continue_)
                .commit()
                .then([session = session.shared_from_this(), request = std::move(request)](bool) {
                    std::cout << "Continuation complete.\n";
                    session->template read<string_body>(request)
                        ->commit()
                        .then([](auto& session, auto const& req) {
                            std::cout << "Body received: " << req.body() << "\n";
                            session.sendStandardResponse(status::ok);
                        })
                        .fail([](Roar::Error const&) {
                            std::cout << "Failed to read body\n";
                        });
                    ;
                })
                .fail([](Roar::Error const&) {
                    std::cout << "Failed to write continue\n";
                });
        }
    }
    else
    {
        session.send<string_body>(request)
            ->status(status::expectation_failed)
            .setHeader(field::connection, "close")
            .body("Set Expect: 100-continue")
            .commit()
            .fail([](Roar::Error const&) {
                std::cout << "Failed to send expectation failure.\n";
            });
    }
}