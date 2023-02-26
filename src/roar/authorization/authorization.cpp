#include <roar/authorization/authorization.hpp>

#include <boost/describe.hpp>
#include <boost/mp11.hpp>

namespace Roar
{
    // ##################################################################################################################
    AuthorizationScheme authorizationSchemeFromString(std::string_view view)
    {
        bool found = false;
        AuthorizationScheme scheme = {};

        boost::mp11::mp_for_each<boost::describe::describe_enumerators<AuthorizationScheme>>([&](auto D) {
            if (!found && view == D.name)
            {
                found = true;
                scheme = D.value;
            }
        });

        if (found)
            return scheme;
        else
            return AuthorizationScheme::Other;
    }
    //------------------------------------------------------------------------------------------------------------------
    std::string to_string(AuthorizationScheme scheme)
    {
        char const* r = "<unknown>";
        boost::mp11::mp_for_each<boost::describe::describe_enumerators<AuthorizationScheme>>([&](auto D) {
            if (scheme == D.value)
                r = D.name;
        });
        return r;
    }
    //------------------------------------------------------------------------------------------------------------------
    AuthorizationScheme Authorization::scheme() const
    {
        if (std::holds_alternative<AuthorizationScheme>(scheme_))
            return std::get<AuthorizationScheme>(scheme_);
        return AuthorizationScheme::Other;
    }
    //------------------------------------------------------------------------------------------------------------------
    std::string Authorization::unknownSchemeAsString() const
    {
        if (std::holds_alternative<std::string>(scheme_))
            return std::get<std::string>(scheme_);
        return "";
    }
    //------------------------------------------------------------------------------------------------------------------
    Authorization::Authorization(std::string_view scheme)
        : scheme_{[&scheme]() -> decltype(scheme_) {
            const auto schemeEnum = authorizationSchemeFromString(scheme);
            if (schemeEnum == AuthorizationScheme::Other)
                return std::string{scheme};
            return schemeEnum;
        }()}
    {}
    // ##################################################################################################################
}