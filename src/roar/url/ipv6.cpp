#include <roar/url/ipv6.hpp>

#include <iomanip>
#include <sstream>

namespace Roar
{
    std::string Ipv6::toString() const
    {
        std::stringstream result;
        result << std::hex << std::setw(4) << std::setfill('0') << segments.front();
        auto end = std::end(segments);
        if (endsWithIpv4)
            end -= 2;
        for (auto iter = std::next(std::begin(segments)); iter != end; ++iter)
        {
            result << ":" << std::hex << std::setw(4) << std::setfill('0') << *iter;
        }
        if (endsWithIpv4)
        {
            result << ":" << std::dec;
            result << ((segments[segments.size() - 2] & 0xFF00) >> 8) << '.';
            result << (segments[segments.size() - 2] & 0xFF) << '.';
            result << ((segments[segments.size() - 1] & 0xFF00) >> 8) << '.';
            result << (segments[segments.size() - 1] & 0xFF);
        }
        return result.str();
    }
} // namespace Roar
