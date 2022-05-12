#include <roar/authorization/basic_auth.hpp>

#include <roar/utility/base64.hpp>

namespace Roar
{
    std::optional<BasicAuth> BasicAuth::fromBase64(std::string_view base64Encoded)
    {
        const auto plain = base64Decode(base64Encoded);
        const auto colon = plain.find(':');
        if (colon != std::string::npos)
            return BasicAuth{plain.substr(0, colon), plain.substr(colon + 1, plain.size() - colon - 1)};
        return std::nullopt;
    }
    BasicAuth::BasicAuth(std::string user, std::string password)
        : user{std::move(user)}
        , password{std::move(password)}
    {}
}