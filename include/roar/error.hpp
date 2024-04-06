#pragma once

#include <roar/beast/forward.hpp>

#include <boost/system/system_error.hpp>

#include <string>
#include <variant>
#include <optional>
#include <functional>
#include <string_view>
#include <sstream>

namespace Roar
{
    /**
     * @brief Holds errors that are produced asynchronously anywhere.
     */
    struct Error
    {
        std::variant<boost::system::error_code, std::string> error = std::string{};
        std::string_view additionalInfo = {};

        std::string toString() const;
    };

    template <typename StreamT>
    StreamT& operator<<(StreamT& stream, Error error)
    {
        std::visit(
            [&stream](auto const& error) {
                stream << error;
            },
            error.error);
        if (!error.additionalInfo.empty())
            stream << ": " << error.additionalInfo;
        return stream;
    }

    inline std::string Error::toString() const
    {
        std::stringstream sstr;
        sstr << *this;
        return sstr.str();
    }
}