#pragma once

#include <roar/routing/proto_route.hpp>
#include <roar/detail/pimpl_special_functions.hpp>

#include <memory>

namespace Roar
{
    class Route
    {
      public:
        Route(ProtoRoute&& proto);
        ROAR_PIMPL_SPECIAL_FUNCTIONS(Route);

        void operator()();

      private:
        struct Implementation;
        std::unique_ptr<Implementation> impl_;
    };
}