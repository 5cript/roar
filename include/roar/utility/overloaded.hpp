#pragma once

namespace Roar
{
    template <typename... Ts>
    struct overloaded : Ts...
    {
        using Ts::operator()...;
    };
    template <typename... Ts>
    overloaded(Ts...) -> overloaded<Ts...>;
}