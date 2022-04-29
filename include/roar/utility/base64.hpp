#pragma once

#include <string>

namespace Roar
{
    std::string base64Encode(std::string const& str);
    std::string base64Decode(std::string const& base64String);
}
