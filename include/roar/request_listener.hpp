#pragma once

#include <boost/beast/http/verb.hpp>

#include <string_view>
#include <optional>

namespace Roar
{
    class Server;

    namespace Detail
    {
        template <class... Ts>
        struct overloaded : Ts...
        {
            using Ts::operator()...;
        };
        template <class... Ts>
        overloaded(Ts...) -> overloaded<Ts...>;
    }

    struct RouteInfo
    {
        std::optional<boost::beast::http::verb> verb = std::nullopt;
        char const* path = "";
        bool allowInsecure = false;
    };

    consteval auto extendRouteInfo(RouteInfo info)
    {
        return Detail::overloaded{
            [info](RouteInfo userInfo) -> RouteInfo {
                return {
                    .verb = userInfo.verb ? userInfo.verb : info.verb,
                    .path = userInfo.path,
                    .allowInsecure = userInfo.allowInsecure ? userInfo.allowInsecure : info.allowInsecure,
                };
            },
            [info](char const* path) -> RouteInfo {
                return {
                    .verb = info.verb,
                    .path = path,
                    .allowInsecure = info.allowInsecure,
                };
            },
        };
    }

#define ROAR_HANDLER_ARGUMENTS

#define ROAR_ROUTE(FNAME) \
    void FNAME(ROAR_HANDLER_ARGUMENTS); \
    constexpr static auto Roar_##FNAME = Roar::extendRouteInfo({.verb = boost::beast::http::verb::get})

// Some default verbs, others can be accessed via ROAR_ROUTE({.verb = ..., .path = ""})
#define ROAR_GET(FNAME) \
    void FNAME(ROAR_HANDLER_ARGUMENTS); \
    constexpr static auto Roar_##FNAME = Roar::extendRouteInfo({.verb = boost::beast::http::verb::get})
#define ROAR_POST(FNAME) \
    void FNAME(ROAR_HANDLER_ARGUMENTS); \
    constexpr static auto Roar_##FNAME = Roar::extendRouteInfo({.verb = boost::beast::http::verb::post})
#define ROAR_PUT(FNAME) \
    void FNAME(ROAR_HANDLER_ARGUMENTS); \
    constexpr static auto Roar_##FNAME = Roar::extendRouteInfo({.verb = boost::beast::http::verb::put})
#define ROAR_OPTIONS(FNAME) \
    void FNAME(ROAR_HANDLER_ARGUMENTS); \
    constexpr static auto Roar_##FNAME = Roar::extendRouteInfo({.verb = boost::beast::http::verb::options})
#define ROAR_TRACE(FNAME) \
    void FNAME(ROAR_HANDLER_ARGUMENTS); \
    constexpr static auto Roar_##FNAME = Roar::extendRouteInfo({.verb = boost::beast::http::verb::trace})
#define ROAR_HEAD(FNAME) \
    void FNAME(ROAR_HANDLER_ARGUMENTS); \
    constexpr static auto Roar_##FNAME = Roar::extendRouteInfo({.verb = boost::beast::http::verb::head})

    class RequestListener
    {
      public:
        /**
         * @brief Attach this request listener to a server. The server will then route requests to this class.
         */
        void attachTo(Server&);
    };

} // namespace Roar