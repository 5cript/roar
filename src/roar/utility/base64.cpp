#include <roar/utility/base64.hpp>

#include <boost/beast/core/detail/base64.hpp>
#include <boost/beast/core/detail/string.hpp>

using namespace boost::beast::detail;

namespace Roar
{
    std::string base64Encode(std::string const& str)
    {
        std::string result(base64::encoded_size(str.size()), '\0');
        base64::encode(result.data(), str.c_str(), str.size());
        return result;
    }
    std::string base64Decode(std::string const& base64String)
    {
        std::string result(base64::decoded_size(base64String.size()), '\0');
        base64::decode(result.data(), base64String.c_str(), base64String.size());
        return result;
    }
    std::string base64Decode(std::string_view base64String)
    {
        std::string result(base64::decoded_size(base64String.size()), '\0');
        base64::decode(result.data(), base64String.data(), base64String.size());
        return result;
    }
}