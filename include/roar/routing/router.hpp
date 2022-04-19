#pragma once

#include <roar/detail/pimpl_special_functions.hpp>
#include <roar/routing/proto_route.hpp>

#include <boost/beast/http/verb.hpp>

#include <memory>
#include <unordered_map>

namespace Roar
{
    class Router
    {
      public:
        Router();
        ROAR_PIMPL_SPECIAL_FUNCTIONS(Router);

        void addRoutes(std::unordered_multimap<boost::beast::http::verb, ProtoRoute>&& routes);

        bool callRoute(boost::beast::http::verb verb, );

      private:
        struct Implementation;
        std::unique_ptr<Implementation> impl_;
    };
}