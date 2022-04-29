#pragma once

#include <roar/routing/proto_route.hpp>
#include <roar/detail/pimpl_special_functions.hpp>
#include <roar/standard_response_provider.hpp>

#include <boost/beast/http/empty_body.hpp>

#include <memory>

namespace Roar
{
    class Session;
    template <typename>
    class Request;

    /**
     * @brief A route represents the mapping of a verb+path to a user provided function.
     */
    class Route
    {
      public:
        Route(ProtoRoute proto);
        ROAR_PIMPL_SPECIAL_FUNCTIONS(Route);

        /**
         * @brief Calls the associated function of this route.
         *
         * @param session The corresponding http session.
         * @param req A request object containing the http request.
         * @param standardResponseProvider A class that can make standard responses for 404, ... .
         */
        void operator()(
            Session& session,
            Request<boost::beast::http::empty_body> req,
            StandardResponseProvider const& standardResponseProvider) const;

        /**
         * @brief Does this route match the given path?

         * @param path A path to match.
         * @param matches Will be filled with regex matches, if the route path is a regex.
         *
         * @return true Does match!
         * @return false Does not match!
         */
        bool matches(std::string const& path, std::vector<std::string>& matches) const;

      private:
        struct Implementation;
        std::unique_ptr<Implementation> impl_;
    };
}