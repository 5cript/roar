#pragma once

#include <string>
#include <vector>

namespace Roar
{
    /**
     * @brief Encode vector in base64
     *
     * @param str
     * @return std::string
     */
    std::string base64Encode(std::vector<unsigned char> const& data);

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

    /**
     * @brief encodes data using base64url algorithm, which is similar to base64, but uses - and _
     * instead of + and /.
     *
     * @param data The binary data to encode
     * @param includePadding determines whether to include url encoded equal signs at the end so that
     * they exist as %3D
     * @return std::string The encoded result
     */
    std::string base64UrlEncode(std::vector<unsigned char> const& data, bool includePadding = false);

    /**
     * @brief Reverses the base64url encoding.
     *
     * @param base64String An encoded string.
     */
    std::vector<unsigned char> base64UrlDecode(std::string_view const& base64String);
}
