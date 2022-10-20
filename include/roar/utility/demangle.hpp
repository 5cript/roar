#pragma once

#include <boost/algorithm/string.hpp>

#include <cxxabi.h>
#include <string>

namespace Roar
{
    /**
     * @brief Demangles a C++ symbol. Useful for tests or debugging purposes.
     *
     * @param name The mangled name
     * @return std::string A human readable demangled name,
     */
    inline std::string demangleCppSymbol(
        std::string const& name,
        std::vector<std::pair<std::string, std::string>> const& replacers = {},
        bool format = false)
    {
        // C++ Types can get very long, so using a very pessimistic buffer:
        constexpr std::size_t bufferSize = 8 * 1024ull * 1024ull;

        std::string demangled(bufferSize, '\0');
        std::size_t mutableLength = bufferSize;

        int status = 0;
        abi::__cxa_demangle(name.c_str(), demangled.data(), &mutableLength, &status);
        demangled.resize(mutableLength);

        const auto pos = demangled.find('\0');
        if (pos != std::string::npos)
            demangled.resize(pos);

        boost::replace_all(
            demangled,
            "std::__cxx11::basic_string<char, std::char_traits<char>, "
            "std::allocator<char> >",
            "std::string");

        for (auto const& [needle, replacement] : replacers)
        {
            boost::replace_all(demangled, needle, replacement);
        }

        if (format)
        {
            int indent = 0;
            std::string formatted;
            auto addLineBreak = [&formatted, &indent](int indentChange) {
                formatted.push_back('\n');
                indent += indentChange;
                for (int i = 0; i < indent; ++i)
                {
                    formatted += "  ";
                }
            };

            for (auto character : demangled)
            {
                if (character == '<')
                {
                    formatted.push_back(character);
                    addLineBreak(1);
                }
                else if (character == '>')
                {
                    addLineBreak(-1);
                    formatted.push_back(character);
                }
                else if (character == ' ')
                    continue;
                else if (character == ',')
                {
                    formatted.push_back(character);
                    addLineBreak(0);
                }
                else
                    formatted.push_back(character);
            }
            demangled = formatted;
        }

        return demangled;
    }
} // namespace Roar
