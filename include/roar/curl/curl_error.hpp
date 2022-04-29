#pragma once

#include <curl/curl.h>

#include <exception>
#include <string>

namespace Roar::Curl
{
    class CurlError : public std::exception
    {
      public:
        CurlError(std::string message, CURLcode code)
            : m_message{std::string{"Curl error with code "} + std::to_string(code) + ": " + std::move(message)}
            , m_code{code}
        {}
        char const* what() const noexcept
        {
            return m_message.c_str();
        }
        CURLcode code() const noexcept
        {
            return m_code;
        }

      private:
        std::string m_message;
        CURLcode m_code;
    };
}
