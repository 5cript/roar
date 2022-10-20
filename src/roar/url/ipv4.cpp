#include <roar/url/ipv4.hpp>

#include <sstream>

namespace Roar
{
    std::string Ipv4::toString() const
    {
        std::stringstream result;
        result << static_cast<int>(octet0) << "." << static_cast<int>(octet1) << "." << static_cast<int>(octet2) << "."
               << static_cast<int>(octet3);
        return result.str();
    }
} // namespace Roar
