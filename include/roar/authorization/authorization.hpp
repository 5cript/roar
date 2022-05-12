#pragma once

#include <variant>
#include <string>
#include <string_view>

#include <boost/describe/enum.hpp>

namespace Roar
{
    BOOST_DEFINE_ENUM(
        AuthorizationScheme,
        Basic,
        Bearer,
        Digest,
        Hoba,
        Mutual,
        Ntlm,
        Vapid,
        Scram,
        Aws4_Hmac_Sha256,
        Other);

    AuthorizationScheme authorizationSchemeFromString(std::string const&);
    AuthorizationScheme authorizationSchemeFromString(std::string_view);
    std::string to_string(AuthorizationScheme scheme);

    class Authorization
    {
      public:
        Authorization(std::string const& scheme);

        /**
         * @brief Returns the authorization scheme. If the value is "Other", use unknownSchemeAsString instead.
         */
        AuthorizationScheme scheme() const;

        /**
         * @brief Use this function if scheme is "Other", this will then return the scheme instead as a string.
         */
        std::string unknownSchemeAsString() const;

      private:
        std::variant<AuthorizationScheme, std::string> scheme_;
    };
}