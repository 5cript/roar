#pragma once

#include <type_traits>

namespace Roar
{
    template <typename T>
    struct TypeWrapper
    {
        template <typename U>
        friend constexpr bool operator==(TypeWrapper, TypeWrapper<U>)
        {
            return std::is_same_v<T, U>;
        }
    };
    template <class T>
    inline constexpr auto type = TypeWrapper<T>{};
    template <class T>
    constexpr auto typeof(T &&)
    {
        return type<T>;
    }
}