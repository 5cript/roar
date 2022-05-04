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
    /**
     * @brief Holds errors that are produced asynchronously anywhere.
     */
    struct Error
    {
        std::variant<boost::system::error_code, std::string> error;
        std::string_view additionalInfo = {};
    };
}