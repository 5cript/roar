#pragma once

#include <boost/beast/http/message.hpp>

#include <optional>
#include <string>
#include <vector>
#include <regex>
#include <memory>
#include <mutex>

namespace Roar
{
    template <typename BodyT>
    class Request : public boost::beast::http::request<BodyT>
    {
      public:
        using beast_request = boost::beast::http::request<BodyT>;

      private:
        std::optional<std::vector<std::string>> regexMatches_;
    };
}