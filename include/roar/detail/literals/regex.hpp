#pragma once

#include <regex>
#include <cstdint>

namespace Roar
{
    struct PseudoRegex
    {
        char const* pattern;
        std::size_t size;

        operator std::regex() const
        {
            return std::regex{pattern};
        }
    };

    inline namespace Literals
    {
        inline namespace RegexLiterals
        {
            inline PseudoRegex operator"" _rgx(char const* regexString, std::size_t length)
            {
                return PseudoRegex{regexString, length};
            }
        }
    }
}