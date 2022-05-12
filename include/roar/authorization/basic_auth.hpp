#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace Roar
{
    struct BasicAuth
    {
        static std::optional<BasicAuth> fromBase64(std::string_view base64Encoded);

        BasicAuth(std::string user, std::string password);

        std::string user;
        std::string password;
    };
}