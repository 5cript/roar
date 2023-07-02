#include <roar/authorization/basic_auth.hpp>

#include <roar/utility/base64.hpp>

namespace Roar
{
    std::optional<BasicAuth> BasicAuth::fromBase64(std::string_view base64Encoded)
    {
        if (base64Encoded.empty())
            return std::nullopt;
        const auto plain = base64Decode(base64Encoded);
        if (plain == ":")
            return BasicAuth{std::string{}, std::string{}};

        const auto colon = plain.find(':');
        if (colon != std::string::npos)
        {
            const auto user = plain.substr(0, colon);
            const auto pw = plain.substr(colon + 1);
            return BasicAuth{std::move(user), std::move(pw)};
        }
        return std::nullopt;
    }
    BasicAuth::BasicAuth(std::string user, std::string password)
        : user{std::move(user)}
        , password{std::move(password)}
    {}
}