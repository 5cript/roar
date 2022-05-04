#pragma once

#include <memory>

namespace Roar::Detail
{
    template <class Base, class Derived>
    struct SharedFromBase : public Base
    {
        using Base::Base;

        std::shared_ptr<Derived> shared_from_this()
        {
            return std::static_pointer_cast<Derived>(Base::shared_from_this());
        };
    };
}