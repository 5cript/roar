#pragma once

namespace Roar::Detail
{
    /**
     * @brief Utility to call std::visit with a set of overloaded functions.
     *
     * @tparam Ts
     */
    template <class... Ts>
    struct overloaded : Ts...
    {
        using Ts::operator()...;
    };
    template <class... Ts>
    overloaded(Ts...) -> overloaded<Ts...>;
}
