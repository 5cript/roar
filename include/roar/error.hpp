#pragma once

#include <roar/beast/forward.hpp>

#include <boost/system/system_error.hpp>

#include <string>
#include <variant>
#include <optional>
#include <functional>
#include <string_view>

namespace Roar
{
    struct Error
    {
        std::variant<boost::system::error_code, std::string> error;
        // Not guaranteed to be set!
        std::function<boost::asio::ip::basic_endpoint<boost::asio::ip::tcp>()> getRemoteEndpoint;
        std::string_view additionalInfo = {};
    };
}