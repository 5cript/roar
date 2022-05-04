#pragma once

#include <roar/routing/request_listener.hpp>

#include <nlohmann/json.hpp>

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
        res.body(j).status(status::ok).send(session);
    }

    class SimpleRoutes
    {
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
        ROAR_GET(unsecure)
        ({.path = "/unsecure",
          .routeOptions = {
              .allowUnsecure = true,
          }});
        ROAR_GET(sendIntermediate)("/sendIntermediate");

      private:
        void justOk(Session& session, EmptyBodyRequest&& req)
        {
            using namespace boost::beast::http;
            session.prepareResponse<empty_body>(req).status(status::ok).send(session);
        }

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
             roar_unsecure,
             roar_sendIntermediate))
    };
    inline void SimpleRoutes::index(Session& session, EmptyBodyRequest&& req)
    {
        using namespace boost::beast::http;
        session.prepareResponse<string_body>(req)
            .body("Hello")
            .contentType("text/plain")
            .status(status::ok)
            .send(session);
    }
    inline void SimpleRoutes::putHere(Session& session, EmptyBodyRequest&& req)
    {
        using namespace boost::beast::http;
        session.template read<string_body>(std::move(req))
            ->noBodyLimit()
            .start()
            .then([](Session& session, Request<string_body> const& req) {
                session.template prepareResponse<string_body>(req)
                    .contentType("text/plain")
                    .status(status::ok)
                    .body(req.body())
                    .preparePayload()
                    .send(session);
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
        session.prepareResponse<string_body>(req).body("AB").contentType("text/plain").status(status::ok).send(session);
    }
    inline void SimpleRoutes::anything(Session& session, EmptyBodyRequest&& req)
    {
        using namespace boost::beast::http;
        session.prepareResponse<string_body>(req)
            .body(nlohmann::json{
                {"path", req.path()},
                {"matches", *req.pathMatches()},
            })
            .contentType("text/plain")
            .status(status::ok)
            .send(session);
    }
    inline void SimpleRoutes::unsecure(Session& session, EmptyBodyRequest&& req)
    {
        using namespace boost::beast::http;
        session.prepareResponse<string_body>(req).status(status::no_content).send(session);
    }
    inline void SimpleRoutes::sendIntermediate(Session& session, EmptyBodyRequest&& req)
    {
        using namespace boost::beast::http;
        session.send<string_body>(req)->status(status::ok).body("Hi").start();
    }
}