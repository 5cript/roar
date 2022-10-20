#pragma once

#include <boost/fusion/include/adapt_struct.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace Roar
{
    struct Ipv4
    {
        uint8_t octet0;
        uint8_t octet1;
        uint8_t octet2;
        uint8_t octet3;

        std::vector<uint16_t> toIpv6Segments() const
        {
            std::vector<uint16_t> segments(2);
            segments[0] = static_cast<uint16_t>(static_cast<uint16_t>(octet0) << 8u) |
                static_cast<uint16_t>(static_cast<uint16_t>(octet1));
            segments[1] = static_cast<uint16_t>(static_cast<uint16_t>(octet2) << 8u) |
                static_cast<uint16_t>(static_cast<uint16_t>(octet3));
            return segments;
        }

        std::string toString() const;
    };
} // namespace Roar

BOOST_FUSION_ADAPT_STRUCT(Roar::Ipv4, octet0, octet1, octet2, octet3)
