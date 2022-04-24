#pragma once

#include <roar/routing/proto_route.hpp>
#include <roar/detail/overloaded.hpp>
#include <roar/detail/literals/regex.hpp>
#include <roar/session/session.hpp>
#include <roar/request.hpp>

#include <boost/beast/http/verb.hpp>
#include <boost/beast/http/empty_body.hpp>
#include <boost/describe/class.hpp>

#include <string_view>
#include <optional>
#include <variant>
#include <regex>
#include <string>

namespace Roar
{
    class Server;

    template <typename RequestListenerT>
    using HandlerType = void (RequestListenerT::*)(Session&, Request<boost::beast::http::empty_body>&&);

    enum class RoutePathType
    {
        Unspecified,
        RegularString,
        Regex
    };

    template <typename RequestListenerT>
    struct RouteInfo
    {
        std::optional<boost::beast::http::verb> verb = std::nullopt;
        char const* path = nullptr;
        RoutePathType pathType = RoutePathType::Unspecified;
        RouteOptions routeOptions = {
            .allowInsecure = false,
            .expectUpgrade = false,
            .allowCors = false,
        };
        HandlerType<RequestListenerT> handler = nullptr;
    };

    template <typename RequestListenerT>
    consteval auto extendRouteInfo(RouteInfo<RequestListenerT> info, HandlerType<RequestListenerT> handler)
    {
        return Detail::overloaded{
            [info, handler](RouteInfo<RequestListenerT> userInfo) -> RouteInfo<RequestListenerT> {
                return {
                    .verb = userInfo.verb ? userInfo.verb : info.verb,
                    .path = userInfo.path,
                    .pathType = userInfo.pathType == RoutePathType::Unspecified ? info.pathType : userInfo.pathType,
                    .routeOptions = userInfo.routeOptions,
                    .handler = handler,
                };
            },
            [info, handler](char const* path) -> RouteInfo<RequestListenerT> {
                return {
                    .verb = info.verb,
                    .path = path,
                    .pathType = RoutePathType::RegularString,
                    .routeOptions = info.routeOptions,
                    .handler = handler,
                };
            },
            [info, handler](PseudoRegex path) -> RouteInfo<RequestListenerT> {
                return {
                    .verb = info.verb,
                    .path = path.pattern,
                    .pathType = RoutePathType::Regex,
                    .routeOptions = info.routeOptions,
                    .handler = handler,
                };
            }};
    }

#define ROAR_MAKE_LISTENER(ListenerType) using this_type = ListenerType

#define ROAR_ROUTE_I(HandlerName, DefaultVerb) \
    void HandlerName(Roar::Session& session, Roar::Request<boost::beast::http::empty_body>&& request); \
    constexpr static auto roar_##HandlerName = \
        Roar::extendRouteInfo({.verb = boost::beast::http::verb::DefaultVerb}, &this_type::HandlerName)

#define ROAR_ROUTE(HandlerName) ROAR_ROUTE_I(HandlerName, get)

// Some default verbs, others can be accessed via ROAR_ROUTE({.verb = ..., .path = ""})
#define ROAR_GET(HandlerName) ROAR_ROUTE_I(HandlerName, get)
#define ROAR_POST(HandlerName) ROAR_ROUTE_I(HandlerName, post)
#define ROAR_PUT(HandlerName) ROAR_ROUTE_I(HandlerName, put)
#define ROAR_OPTIONS(HandlerName) ROAR_ROUTE_I(HandlerName, options)
#define ROAR_TRACE(HandlerName) ROAR_ROUTE_I(HandlerName, trace)
#define ROAR_HEAD(HandlerName) ROAR_ROUTE_I(HandlerName, head)
#define ROAR_DELETE(HandlerName) ROAR_ROUTE_I(HandlerName, delete_)

} // namespace Roar