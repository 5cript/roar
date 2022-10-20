#include <roar/url/encode.hpp>

#include <curl/curl.h>
#include <roar/curl/instance.hpp>

#include <stdexcept>
#include <string>

namespace Roar
{
    std::string urlEncode(const std::string& source)
    {
        // Curl crashes on empty paths.
        if (source.empty())
            return {};

        Curl::Instance temporaryInstance;
        std::string encoded;
        auto* escaped =
            curl_easy_escape(static_cast<CURL*>(temporaryInstance), source.c_str(), static_cast<int>(source.length()));
        if (escaped != nullptr)
        {
            encoded = escaped;
            curl_free(escaped);
        }
        else
            throw std::runtime_error("Could not encode url.");

        return encoded;
    }

    std::string urlDecode(const std::string& source)
    {
        Curl::Instance temporaryInstance;
        std::string decoded;
        int actual = 0;
        auto* dec = curl_easy_unescape(
            static_cast<CURL*>(temporaryInstance), source.c_str(), static_cast<int>(source.length()), &actual);
        if (dec != nullptr)
        {
            decoded = std::string{dec, dec + actual};
            curl_free(dec);
        }
        else
            throw std::runtime_error("Could not decode url.");

        return decoded;
    }
} // namespace Roar
