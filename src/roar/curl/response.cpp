#include <iostream>
#include <roar/curl/response.hpp>
#include <roar/curl/instance.hpp>

#include <curl/system.h>

namespace Roar::Curl
{
    Response::Response(Instance instance)
        : instance_{instance}
        , code_{curl_easy_perform(instance_)}
    {}

    CURLcode Response::result() const
    {
        return code_;
    }
    std::string Response::redirectUrl() const
    {
        char* url = nullptr;
        auto res = curl_easy_getinfo(instance_.get(), CURLINFO_REDIRECT_URL, &url);
        if (res == CURLE_OK && url)
            return {url};
        return {};
    }
    long long Response::sizeOfDownload() const
    {
        curl_off_t dl;
        auto res = curl_easy_getinfo(instance_.get(), CURLINFO_SIZE_DOWNLOAD_T, &dl);
        if (res == CURLE_OK)
            return static_cast<long long>(dl);
        return 0;
    }
    long long Response::sizeOfUpload() const
    {
        curl_off_t ul;
        auto res = curl_easy_getinfo(instance_.get(), CURLINFO_SIZE_UPLOAD_T, &ul);
        if (res == CURLE_OK)
            return static_cast<long long>(ul);
        return 0;
    }
    long Response::code() const
    {
        long code = 0;
        auto res = curl_easy_getinfo(instance_.get(), CURLINFO_RESPONSE_CODE, &code);
        if (res != CURLE_OK)
            return 0;
        return code;
    }
    long Response::proxyCode() const
    {
        long code = 0;
        auto res = curl_easy_getinfo(instance_.get(), CURLINFO_HTTP_CONNECTCODE, &code);
        if (res != CURLE_OK)
            return 0;
        return code;
    }
    Response::operator bool() const
    {
        return code() != 0;
    }
}
