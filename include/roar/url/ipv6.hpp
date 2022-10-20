#pragma once

#include <boost/fusion/include/adapt_struct.hpp>

#include <array>
#include <cstdint>
#include <string>

namespace Roar
{
    struct Ipv6
    {
        std::array<uint16_t, 8> segments{};
        bool endsWithIpv4{false};

        std::string toString() const;
    };
} // namespace Roar
