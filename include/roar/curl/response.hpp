#pragma once

#include "instance.hpp"

#include <curl/curl.h>
#include <boost/beast/http/status.hpp>

#include <string>

namespace Roar::Curl
{
    /**
     * @brief This class is returned by a Request class when the request is performed.
     */
    class Response
    {
      private:
        friend class Request;

        /**
         * @brief Cannot explicitly instantiate, it is made by the Request class.
         * @param instance
         */
        explicit Response(Instance instance);

      public:
        /**
         * @brief The CURLcode. Can be useful to check for errors.
         * @return CURLcode Original CURLcode returned by curl_perform.
         */
        CURLcode result() const;

        /**
         * @brief The redirect url if the response was a redirect.
         * @return std::string
         */
        std::string redirectUrl() const;

        /**
         * @brief The size of the downloaded data.
         * @return long long
         */
        long long sizeOfDownload() const;

        /**
         * @brief The size of the uploaded data.
         * @return long long
         */
        long long sizeOfUpload() const;

        /**
         * @brief Response code.
         * @return boost::beast::http::status
         */
        boost::beast::http::status code() const;

        /**
         * @brief Response code of the proxy if there was one inbetween.
         * @return boost::beast::http::status
         */
        boost::beast::http::status proxyCode() const;

        /**
         * @brief Was there a response to the request?
         *
         * @return true When the request was sent successfully and a response was received.
         * @return false When the request failed and no response was received. This response object is invalid.
         */
        explicit operator bool() const;

      private:
        Instance instance_;
        CURLcode code_;
    };
}
