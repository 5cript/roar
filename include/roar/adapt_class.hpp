#pragma once

#include <iosfwd>
#include <boost/beast/http/verb.hpp>
#include <string_view>

namespace Roar
{
    struct RouteInfo
    {
        boost::beast::http::verb verb;
        char const* path;

        constexpr RouteInfo(boost::beast::http::verb verb, char const* path)
            : verb{verb}
            , path{path}
        {}
    };

#define ROAR_ROUTE(FNAME) \
    void FNAME(); \
    constexpr static auto Roar_##FNAME = Roar::RouteInfo

#define ROAR_ROUTES(FNAME) (FNAME, FNAME##_Roar)
} // namespace Roar