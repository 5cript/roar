#pragma once

#include <roar/routing/request_listener.hpp>

#include <nlohmann/json.hpp>

#include <thread>
#include <chrono>

#include <iostream>

namespace Roar::Tests
{
    using namespace Roar::RegexLiterals;

    class HeaderReflector
    {
      private:
        ROAR_MAKE_LISTENER(HeaderReflector);

        ROAR_GET(headers)("/headers");

      private:
        BOOST_DESCRIBE_CLASS(HeaderReflector, (), (), (), (roar_headers))
    };
    inline void HeaderReflector::headers(Session& session, EmptyBodyRequest&& req)
    {
        using namespace boost::beast::http;

        auto res = session.prepareResponse<string_body>(req);
        nlohmann::json j;
        for (auto const& field : req)
        {
            if (field.name() == field::unknown)
                j[std::string{field.name_string()}] = field.value();
            else
                j[std::string{to_string(field.name())}] = field.value();
        }
        session.send<string_body>(req)->body(j.dump()).preparePayload().status(status::ok).commit();
    }

    class SimpleRoutes
    {
      public:
        std::function<void(Session& session, EmptyBodyRequest&& req)> dynamicGetFunc;

      private:
        ROAR_MAKE_LISTENER(SimpleRoutes);

        ROAR_GET(index)("/index.txt");
        ROAR_GET(ab)("/a/b");
        ROAR_GET(anything)(R"(\/([^\/]+)\/(.+))"_rgx);
        ROAR_PUT(putHere)("/putHere");
        ROAR_POST(postHere)("/postHere");
        ROAR_DELETE(deleteHere)("/deleteHere");
        ROAR_OPTIONS(optionsHere)("/optionsHere");
        ROAR_HEAD(headHere)("/headHere");
        ROAR_GET(sse)
        ({
            .path = "/sse",
            .routeOptions =
                {
                    .allowUnsecure = true,
                },
        });
        ROAR_GET(unsecure)
        ({
            .path = "/unsecure",
            .routeOptions =
                {
                    .allowUnsecure = true,
                },
        });
        ROAR_GET(sendIntermediate)("/sendIntermediate");
        ROAR_GET(dynamicGet)("/dynamicGet");

      private:
        void justOk(Session& session, EmptyBodyRequest&& req)
        {
            using namespace boost::beast::http;
            session.send<string_body>(req)->status(status::ok).commit();
        }

        int sseId = 0;
        std::unordered_map<int, std::shared_ptr<Session>> sseSessions;

      private:
        BOOST_DESCRIBE_CLASS(
            SimpleRoutes,
            (),
            (),
            (),
            (roar_index,
             roar_putHere,
             roar_postHere,
             roar_deleteHere,
             roar_optionsHere,
             roar_headHere,
             roar_anything,
             roar_ab,
             roar_sse,
             roar_unsecure,
             roar_sendIntermediate,
             roar_dynamicGet))
    };
    inline void SimpleRoutes::index(Session& session, EmptyBodyRequest&& req)
    {
        using namespace boost::beast::http;
        session.send<string_body>(req)
            ->status(status::ok)
            .contentType("text/plain")
            .body("Hello")
            .preparePayload()
            .commit();
    }
    inline void SimpleRoutes::putHere(Session& session, EmptyBodyRequest&& req)
    {
        using namespace boost::beast::http;
        session.template read<string_body>(std::move(req))
            ->noBodyLimit()
            .commit()
            .then([](Session& session, Request<string_body> const& req) {
                session.send<string_body>(req)
                    ->contentType("text/plain")
                    .status(status::ok)
                    .body(req.body())
                    .preparePayload()
                    .commit();
            });
    }
    inline void SimpleRoutes::postHere(Session& session, EmptyBodyRequest&& req)
    {
        return putHere(session, std::move(req));
    }
    inline void SimpleRoutes::deleteHere(Session& session, EmptyBodyRequest&& req)
    {
        justOk(session, std::move(req));
    }
    inline void SimpleRoutes::optionsHere(Session& session, EmptyBodyRequest&& req)
    {
        justOk(session, std::move(req));
    }
    inline void SimpleRoutes::headHere(Session& session, EmptyBodyRequest&& req)
    {
        justOk(session, std::move(req));
    }
    inline void SimpleRoutes::ab(Session& session, EmptyBodyRequest&& req)
    {
        using namespace boost::beast::http;
        session.send<string_body>(req)
            ->body("AB")
            .contentType("text/plain")
            .preparePayload()
            .status(status::ok)
            .commit();
    }
    inline void SimpleRoutes::anything(Session& session, EmptyBodyRequest&& req)
    {
        using namespace boost::beast::http;
        session.send<string_body>(req)
            ->body(nlohmann::json{
                {"path", req.path()},
                {"matches", *req.pathMatches()},
            })
            .contentType("text/plain")
            .preparePayload()
            .status(status::ok)
            .commit();
    }
    inline void SimpleRoutes::unsecure(Session& session, EmptyBodyRequest&& req)
    {
        using namespace boost::beast::http;
        session.send<string_body>(req)->status(status::no_content).commit();
    }
    inline void SimpleRoutes::sendIntermediate(Session& session, EmptyBodyRequest&& req)
    {
        using namespace boost::beast::http;
        session.send<string_body>(req)->status(status::ok).body("Hi").preparePayload().commit();
    }
    inline void SimpleRoutes::dynamicGet(Session& session, EmptyBodyRequest&& req)
    {
        using namespace boost::beast::http;
        if (dynamicGetFunc)
            dynamicGetFunc(session, std::move(req));
        else
            justOk(session, std::move(req));
    }
    inline void SimpleRoutes::sse(Session& session, EmptyBodyRequest&& req)
    {
        using namespace boost::beast::http;
        const auto id = sseId++;
        sseSessions[id] = session.shared_from_this();

        std::shared_ptr<int> counter = std::make_shared<int>(0);

        std::shared_ptr<std::function<void()>> doThing = std::make_shared<std::function<void()>>();

        *doThing = [this, counter, doThing, &session, id]() mutable {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));

            std::cout << *counter << "\n";

            ++*counter;
            if (*counter == 5)
            {
                sseSessions.erase(id);
                doThing.reset();
            }
            else
            {
                std::cout << "Sent SSE message " << *counter << " to session " << id << std::endl;
                session.send("id: " + std::to_string(*counter) + "\n" + "data: " + std::to_string(*counter) + "\n\n")
                    .then([this, id, counter, doThing](bool) {
                        std::cout << "Sent SSE message " << *counter << " to session " << id << std::endl;
                        (*doThing)();
                    })
                    .fail([this, id, &doThing](auto&&) mutable {
                        std::cout << "Failed to send SSE message, removing session " << id << std::endl;
                        sseSessions.erase(id);
                        doThing.reset();
                    });
            }
        };

        session.sendWithAllAcceptedCors<empty_body>()
            ->status(status::ok)
            .header(boost::beast::http::field::cache_control, "no-cache")
            .contentType("text/event-stream")
            .commit()
            .then([this, id, counter, doThing](bool) {
                std::cout << "Sent Header\n";
                (*doThing)();
            })
            .fail([this, id, &doThing](auto&&) mutable {
                std::cout << "Failed to send SSE message, removing session " << id << std::endl;
                sseSessions.erase(id);
                doThing.reset();
            });
    }
}