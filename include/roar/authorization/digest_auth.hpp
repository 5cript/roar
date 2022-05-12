#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace Roar
{
    struct DigestAuth
    {
        static std::optional<DigestAuth> fromParameters(std::string_view parameterList);

        DigestAuth();
        DigestAuth(
            std::string username,
            std::string realm,
            std::string uri,
            std::string algorithm,
            std::string nonce,
            std::string nc,
            std::string cnonce,
            std::string qop,
            std::string response,
            std::string opaque);

        std::string username;
        std::string realm;
        std::string uri;
        std::string algorithm;
        std::string nonce;
        std::string nc;
        std::string cnonce;
        std::string qop;
        std::string response;
        std::string opaque;
    };
}