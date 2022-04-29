#pragma once

namespace Roar::Detail
{
    /**
     * @brief Returns the first type of a parameter pack.
     *
     * @tparam T
     * @tparam Args
     */
    template <typename T, typename... Args>
    struct FirstType
    {
        using type = T;
    };

    template <typename T, typename... Args>
    using FirstType_t = typename FirstType<T, Args...>::type;
}