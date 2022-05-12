#pragma once

#include <boost/beast/http/field.hpp>
#include <boost/beast/http/verb.hpp>

#include <string>
#include <vector>
#include <cctype>
#include <optional>

namespace Roar
{
    /**
     * @brief Settings object to control CORS requests.
     */
    struct CorsSettings
    {
        /// Access-Control-Allow-Origin
        std::function<std::string(std::string const&)> allowedOrigin{[](std::string const& origin) {
            // Requests with credentials CANNOT use a wildcard for any Allow-* header.
            return origin.empty() ? std::string("*") : origin;
        }};

        /// Access-Control-Allow-Methods
        std::function<std::vector<std::string>(std::vector<std::string> const&)> methodAllowSelection{
            [](std::vector<std::string> const& requestedMethods) {
                return requestedMethods;
            }};

        /// Access-Control-Allow-Headers
        std::function<std::vector<std::string>(std::vector<std::string> const&)> headerAllowSelection{
            [](std::vector<std::string> const& requestedHeaders) {
                return requestedHeaders;
            }};

        /// Access-Control-Expose-Headers, will not be set if empty.
        std::vector<boost::beast::http::field> exposeHeaders{};

        /// Access-Control-Allow-Credentials
        std::optional<bool> allowCredentials{std::nullopt};

        /// Generate an OPTIONS route for preflights?
        bool generatePreflightOptionsRoute = true;
    };

    /**
     * @brief Reflects all requested headers and origin or sets the Kleene star. Is as permissive as possible. Least
     * secure option.
     *
     * @param method The method to allow.
     * @return CorsSettings A created cors settings object.
     */
    inline CorsSettings makePermissiveCorsSettings(std::string method)
    {
        CorsSettings cors;
        std::transform(begin(method), end(method), begin(method), [](auto character) {
            return std::toupper(character);
        });
        cors.methodAllowSelection =
            [method = std::move(method)](std::vector<std::string> const&) -> std::vector<std::string> {
            return std::vector<std::string>{method};
        };
        cors.allowCredentials = true;
        return cors;
    }

    /**
     * @brief Same as the makePermissiveCorsSettings(std::string method) overload.
     *
     * @param method The method to allow.
     * @return CorsSettings A created cors settings object.
     */
    inline CorsSettings makePermissiveCorsSettings(boost::beast::http::verb method)
    {
        return makePermissiveCorsSettings(std::string{boost::beast::http::to_string(method)});
    }
}