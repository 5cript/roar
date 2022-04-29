#pragma once

#include <roar/detail/pimpl_special_functions.hpp>
#include <roar/routing/proto_route.hpp>
#include <roar/standard_response_provider.hpp>

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

    /**
     * @brief The route routes http requests to their respective handler functions.
     */
    class Router : public std::enable_shared_from_this<Router>
    {
      public:
        Router(std::shared_ptr<const StandardResponseProvider> standardResponseProvider);
        ROAR_PIMPL_SPECIAL_FUNCTIONS(Router);

        /**
         * @brief Add new routes to this router.
         *
         * @param routes A map of verbs->routes.
         */
        void addRoutes(std::unordered_multimap<boost::beast::http::verb, ProtoRoute>&& routes);

        /**
         * @brief Find a follow route. Automatically responds with 404 when now route can be found.
         * @param request A http request object.
         */
        void followRoute(Session&, Request<boost::beast::http::empty_body> request);

      private:
        struct Implementation;
        std::unique_ptr<Implementation> impl_;
    };
}