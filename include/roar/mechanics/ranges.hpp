#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <optional>

namespace Roar
{
    struct Ranges
    {
        struct Range
        {
            std::uint64_t start;
            std::uint64_t end;

            std::uint64_t size() const;
            std::string toString() const;
        };
        std::string unit;
        std::vector<Range> ranges;

        static std::optional<Ranges> fromString(std::string const& str);
    };
}