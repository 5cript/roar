#pragma once

#include <string>
#include <unordered_map>
#include <optional>

namespace Roar
{
    struct Url
    {
        struct UserInfo
        {
            std::string user{};
            std::string password{};
        };

        struct Remote
        {
            std::string host{};
            std::optional<unsigned short> port{};
        };

        std::string scheme{};
        std::optional<UserInfo> userInfo{};
        Remote remote{};
        std::string path{};
        std::unordered_map<std::string, std::string> query{};

        std::string toString() const;
        std::string toShortString() const;
    };
}