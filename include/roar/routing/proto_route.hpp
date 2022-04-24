#pragma once

#include <boost/beast/http/empty_body.hpp>

#include <string>
#include <functional>
#include <variant>
#include <regex>

namespace Roar
{
    class Session;
    template <typename>
    class Request;
    struct ProtoRoute
    {
        std::variant<std::string, std::regex> path;
        std::function<void(Session& session, Request<boost::beast::http::empty_body> const& req)> callRoute;
        std::function<bool(std::string const&, std::vector<std::string>&)> matches;
    };
}