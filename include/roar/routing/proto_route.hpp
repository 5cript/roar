#pragma once

#include <boost/beast/http/empty_body.hpp>
#include <roar/cors.hpp>
#include <roar/detail/literals/regex.hpp>

#include <string>
#include <functional>
#include <variant>
#include <regex>

namespace Roar
{
    /**
     * @brief Options that modify the server behavior for a given route.
     */
    struct RouteOptions
    {
        /// If the server is an https server, setting this to true will allow unencrypted requests on this route.
        bool allowUnsecure;

        /// Set this to true if you expect a websocket upgrade request on this route.
        bool expectUpgrade;

        /// Set this to provide automatically generated cors headers and preflight requests.
        std::optional<CorsSettings> cors;
    };

    class Session;
    template <typename>
    class Request;

    /**
     * @brief A proper Route is built up from this object. This object is usually created internally.
     */
    struct ProtoRoute
    {
        std::variant<std::string, std::regex> path;
        std::function<void(Session& session, Request<boost::beast::http::empty_body>&& req)> callRoute;
        std::function<bool(std::string const&, std::vector<std::string>&)> matches;
        RouteOptions routeOptions;
    };
}