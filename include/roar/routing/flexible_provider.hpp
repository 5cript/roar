#pragma once

#include <roar/utility/overloaded.hpp>

#include <variant>
#include <functional>
#include <type_traits>
#include <optional>

namespace Roar
{
    namespace Detail
    {
        template <bool Optional, typename... Retrievers>
        struct MakeVariantFromRetrievers
        {
            using type = std::variant<Retrievers...>;
        };
        template <typename... Retrievers>
        struct MakeVariantFromRetrievers<true, Retrievers...>
        {
            using type = std::variant<std::monostate, Retrievers...>;
        };
        template <bool Optional, typename... Retrievers>
        using MakeVariantFromRetrievers_v = typename MakeVariantFromRetrievers<Optional, Retrievers...>::type;
    }

    template <typename HolderClassT, typename T, bool Optional = false>
    using FlexibleProvider = Detail::MakeVariantFromRetrievers_v<
        Optional,
        std::function<T(HolderClassT&)>,
        std::function<T const&(HolderClassT&)>,
        T const& (HolderClassT::*)(),
        T const& (HolderClassT::*)() const,
        T (HolderClassT::*)(),
        T (HolderClassT::*)() const,
        T HolderClassT::*,
        T>;

    template <typename HolderClassT, typename T, bool Optional>
    struct FlexibleProviderUnwrapper
    {
        static std::optional<T> getValue(HolderClassT& holder, FlexibleProvider<HolderClassT, T, true> const& provider)
        {
            return std::visit(
                overloaded{
                    [](std::monostate) -> std::optional<T> {
                        return std::nullopt;
                    },
                    [&holder](std::function<T(HolderClassT&)> const& fn) -> std::optional<T> {
                        return fn(holder);
                    },
                    [&holder](std::function<T const&(HolderClassT&)> const& fn) -> std::optional<T> {
                        return fn(holder);
                    },
                    [&holder](T const& (HolderClassT::*fn)()) -> std::optional<T> {
                        return (holder.*fn)();
                    },
                    [&holder](T const& (HolderClassT::*fn)() const) -> std::optional<T> {
                        return (holder.*fn)();
                    },
                    [&holder](T (HolderClassT::*fn)()) -> std::optional<T> {
                        return (holder.*fn)();
                    },
                    [&holder](T (HolderClassT::*fn)() const) -> std::optional<T> {
                        return (holder.*fn)();
                    },
                    [&holder](T HolderClassT::*mem) -> std::optional<T> {
                        return holder.*mem;
                    },
                    [](T value) -> std::optional<T> {
                        return value;
                    },
                },
                provider);
        }
    };
    template <typename HolderClassT, typename T>
    struct FlexibleProviderUnwrapper<HolderClassT, T, false>
    {
        static T getValue(HolderClassT& holder, FlexibleProvider<HolderClassT, T, false> const& provider)
        {
            return std::visit(
                overloaded{
                    [&holder](std::function<T(HolderClassT&)> const& fn) -> T {
                        return fn(holder);
                    },
                    [&holder](std::function<T const&(HolderClassT&)> const& fn) -> T {
                        return fn(holder);
                    },
                    [&holder](T const& (HolderClassT::*fn)()) -> T {
                        return (holder.*fn)();
                    },
                    [&holder](T const& (HolderClassT::*fn)() const) -> T {
                        return (holder.*fn)();
                    },
                    [&holder](T (HolderClassT::*fn)()) -> T {
                        return (holder.*fn)();
                    },
                    [&holder](T (HolderClassT::*fn)() const) -> T {
                        return (holder.*fn)();
                    },
                    [&holder](T HolderClassT::*mem) -> T {
                        return holder.*mem;
                    },
                    [](T value) -> T {
                        return value;
                    },
                },
                provider);
        }
    };

    template <typename HolderClassT, typename T>
    std::optional<T>
    unwrapFlexibleProvider(HolderClassT& holder, FlexibleProvider<HolderClassT, T, true> const& flexibleProvider)
    {
        return FlexibleProviderUnwrapper<HolderClassT, T, true>::getValue(holder, flexibleProvider);
    }

    template <typename HolderClassT, typename T>
    T unwrapFlexibleProvider(HolderClassT& holder, FlexibleProvider<HolderClassT, T, false> const& flexibleProvider)
    {
        return FlexibleProviderUnwrapper<HolderClassT, T, false>::getValue(holder, flexibleProvider);
    }
}