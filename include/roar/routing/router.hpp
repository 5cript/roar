#pragma once

#include <roar/detail/pimpl_special_functions.hpp>
#include <roar/routing/proto_route.hpp>

#include <iostream>
#include <boost/beast/http/verb.hpp>
#include <boost/beast/http/empty_body.hpp>

#include <memory>
#include <unordered_map>
#include <functional>

namespace Roar
{
    class Session;
    template <typename>
    class Request;
    class Router : public std::enable_shared_from_this<Router>
    {
      public:
        Router(std::function<void(Session&, Request<boost::beast::http::empty_body> const&)> onNotFound);
        ROAR_PIMPL_SPECIAL_FUNCTIONS(Router);

        void addRoutes(std::unordered_multimap<boost::beast::http::verb, ProtoRoute>&& routes);
        void followRoute(Session&, Request<boost::beast::http::empty_body>& request);

      private:
        struct Implementation;
        std::unique_ptr<Implementation> impl_;
    };
}