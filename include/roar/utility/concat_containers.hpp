#pragma once

#include <iterator>

namespace Roar
{
    template <template <typename...> typename ContainerType, typename ElementType>
    void concatContainers(ContainerType<ElementType>& lhs, ContainerType<ElementType> const& rhs)
    {
        lhs.reserve(lhs.size() + rhs.size());
        lhs.insert(std::end(lhs), std::begin(rhs), std::end(rhs));
    }

    template <template <typename...> typename ContainerType, typename ElementType>
    void concatContainers(ContainerType<ElementType>& lhs, ContainerType<ElementType>&& rhs)
    {
        lhs.reserve(lhs.size() + rhs.size());
        lhs.insert(std::end(lhs), std::make_move_iterator(std::begin(rhs)), std::make_move_iterator(std::end(rhs)));
    }

    template <template <typename...> typename ContainerType, typename ElementType>
    ContainerType<ElementType>
    concatContainersCopy(ContainerType<ElementType> lhs, ContainerType<ElementType> const& rhs)
    {
        lhs.reserve(lhs.size() + rhs.size());
        lhs.insert(std::end(lhs), std::begin(rhs), std::end(rhs));
        return lhs;
    }

    template <template <typename...> typename ContainerType, typename ElementType>
    ContainerType<ElementType> concatContainersCopy(ContainerType<ElementType> lhs, ContainerType<ElementType>&& rhs)
    {
        lhs.reserve(lhs.size() + rhs.size());
        lhs.insert(std::end(lhs), std::make_move_iterator(std::begin(rhs)), std::make_move_iterator(std::end(rhs)));
        return lhs;
    }
} // namespace Roar
