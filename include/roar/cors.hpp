#pragma once

#include <boost/beast/http/verb.hpp>

#include <string>
#include <vector>
#include <cctype>
#include <optional>

namespace Roar
{
    struct CorsSettings
    {
        CorsSettings()
            : allowedOrigin{[](std::string const& origin) {
                // Requests with credentials CANNOT use a wildcard for the allowed origin.
                return origin.empty() ? std::string("*") : origin;
            }}
            , methodAllowSelection{[](std::vector<std::string> const& requestedMethods) {
                return requestedMethods.empty() ? std::vector<std::string>{} : std::vector<std::string>{"GET"};
            }}
            , headerAllowSelection{[](std::vector<std::string> const& requestedHeaders) {
                return requestedHeaders.empty() ? std::vector<std::string>{} : requestedHeaders;
            }}
            , allowCredentials{}
        {}

        std::function<std::string(std::string const&)> allowedOrigin;
        std::function<std::vector<std::string>(std::vector<std::string> const&)> methodAllowSelection;
        std::function<std::vector<std::string>(std::vector<std::string> const&)> headerAllowSelection;
        std::optional<bool> allowCredentials;
    };

    inline CorsSettings makeDefaultCorsSettings(std::string method)
    {
        CorsSettings cors;
        cors.methodAllowSelection =
            [&method](std::vector<std::string> const& requestedMethods) -> std::vector<std::string> {
            return requestedMethods.empty() ? std::vector<std::string>{} : std::vector<std::string>{std::move(method)};
        };
        return cors;
    }

    inline CorsSettings makeDefaultCorsSettings(boost::beast::http::verb method)
    {
        CorsSettings cors;
        std::string strMethod = std::string{boost::beast::http::to_string(method)};
        std::transform(begin(strMethod), end(strMethod), begin(strMethod), [](auto character) {
            return std::toupper(character);
        });
        cors.methodAllowSelection =
            [&method, &strMethod](std::vector<std::string> const& requestedMethods) -> std::vector<std::string> {
            return requestedMethods.empty() ? std::vector<std::string>{}
                                            : std::vector<std::string>{std::move(strMethod)};
        };
        return cors;
    }
}