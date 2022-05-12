#pragma once

#include <string>
#include <cstdint>
#include <cstddef>
#include <cmath>

namespace Roar::Detail
{
    template <unsigned Base>
    std::string bytesToHumanReadable(std::uint64_t bytes)
    {
        const auto log = std::logl(bytes) / std::logl(Base);
        unsigned floored = static_cast<unsigned>(log);
        for (unsigned i = 0; i < floored; ++i)
            bytes /= Base;

        if constexpr (Base == 1024)
        {
            constexpr const char units[8][4]{"B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB", "ZiB"};
            return std::to_string(bytes) + " " + units[floored];
        }
        else
        {
            constexpr const char units[8][3]{"b", "kb", "mb", "gb", "tb", "pb", "eb", "zb"};
            return std::to_string(bytes) + " " + units[floored];
        }
    }
}