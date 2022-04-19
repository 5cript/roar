#pragma once

#include <string>
#include <functional>
#include <variant>
#include <regex>

namespace Roar
{
    struct ProtoRoute
    {
        std::variant<std::string, std::regex> path;
        std::function<void()> callRoute;
    };
}