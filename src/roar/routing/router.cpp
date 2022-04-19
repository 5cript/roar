#include <roar/routing/router.hpp>
#include <roar/routing/route.hpp>

namespace Roar
{
    //##################################################################################################################
    struct Router::Implementation
    {
        std::unordered_multimap<boost::beast::http::verb, Route> routes;

        Implementation()
            : routes{}
        {}
    };
    //##################################################################################################################
    Router::Router()
        : impl_{std::make_unique<Implementation>()}
    {}
    //------------------------------------------------------------------------------------------------------------------
    ROAR_PIMPL_SPECIAL_FUNCTIONS_IMPL(Router);
    //------------------------------------------------------------------------------------------------------------------
    void Router::addRoutes(std::unordered_multimap<boost::beast::http::verb, ProtoRoute>&& routes)
    {
        for (auto&& [key, proto] : routes)
            impl_->routes.insert(std::make_pair(key, Route{std::move(proto)}));
    }
    //##################################################################################################################
}