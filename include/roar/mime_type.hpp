#pragma once

#include <string>
#include <optional>

namespace Roar
{
    std::optional<std::string> extensionToMime(std::string const& extension);
}