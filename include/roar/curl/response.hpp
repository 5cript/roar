#pragma once

#include "instance.hpp"

#include <curl/curl.h>
#include <boost/beast/http/status.hpp>

#include <string>

namespace Roar::Curl
{
    class Response
    {
      private:
        friend class Request;

        explicit Response(Instance instance);

      public:
        CURLcode result() const;
        std::string redirectUrl() const;
        long long sizeOfDownload() const;
        long long sizeOfUpload() const;
        boost::beast::http::status code() const;
        boost::beast::http::status proxyCode() const;
        explicit operator bool() const;

      private:
        Instance instance_;
        CURLcode code_;
    };
}
