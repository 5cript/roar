#pragma once

#include <string>

namespace Roar
{
    /**
     * @brief Encode string in base64
     *
     * @param str
     * @return std::string
     */
    std::string base64Encode(std::string const& str);

    /**
     * @brief Decode base64 to string.
     *
     * @param base64String
     * @return std::string
     */
    std::string base64Decode(std::string const& base64String);

    /**
     * @brief Decode base64 to string.
     *
     * @param base64String
     * @return std::string
     */
    std::string base64Decode(std::string_view base64String);
}
