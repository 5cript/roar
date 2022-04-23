#pragma once

#include <roar/detail/overloaded.hpp>

#include <boost/beast/http/verb.hpp>

#include <string_view>
#include <optional>
#include <variant>
#include <regex>
#include <string>

namespace Roar
{
    class Server;

    template <typename RequestListenerT>
    using HandlerType = void (RequestListenerT::*)();

    enum class RoutePathType
    {
        Unspecified,
        RegularString,
        Regex
    };

    struct PseudoRegex
    {
        char const* pattern;
        std::size_t size;
    };

    template <typename RequestListenerT>
    struct RouteInfo
    {
        std::optional<boost::beast::http::verb> verb = std::nullopt;
        char const* path = nullptr;
        RoutePathType pathType = RoutePathType::Unspecified;
        bool allowInsecure = false;
        bool allowUpgrade = false;
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
                    .allowInsecure = userInfo.allowInsecure ? userInfo.allowInsecure : info.allowInsecure,
                    .allowUpgrade = userInfo.allowUpgrade ? userInfo.allowUpgrade : info.allowUpgrade,
                    .handler = handler,
                };
            },
            [info, handler](char const* path) -> RouteInfo<RequestListenerT> {
                return {
                    .verb = info.verb,
                    .path = path,
                    .pathType = RoutePathType::RegularString,
                    .allowInsecure = info.allowInsecure,
                    .allowUpgrade = info.allowUpgrade,
                    .handler = handler,
                };
            },
            [info, handler](PseudoRegex path) -> RouteInfo<RequestListenerT> {
                return {
                    .verb = info.verb,
                    .path = path.pattern,
                    .pathType = RoutePathType::Regex,
                    .allowInsecure = info.allowInsecure,
                    .allowUpgrade = info.allowUpgrade,
                    .handler = handler,
                };
            }};
    }

    inline namespace literals
    {
        inline namespace regex_literals
        {
            inline PseudoRegex operator"" _rgx(char const* regexString, std::size_t length)
            {
                return PseudoRegex{regexString, length};
            }
        }
    }

#define ROAR_MAKE_LISTENER(ListenerType) using this_type = ListenerType

#define ROAR_ROUTE_I(HandlerName, DefaultVerb) \
    void HandlerName(); \
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

} // namespace Roar