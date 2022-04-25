#pragma once

namespace Roar::Detail
{
    template <typename T, typename... Args>
    struct FirstType
    {
        using type = T;
    };

    template <typename T, typename... Args>
    using FirstType_t = typename FirstType<T, Args...>::type;
}