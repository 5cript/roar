#include <roar/routing/router.hpp>
#include <roar/routing/route.hpp>
#include <roar/request.hpp>

#include <utility>
#include <mutex>

namespace Roar
{
    //##################################################################################################################
    struct Router::Implementation
    {
        mutable std::mutex routesMutex;
        std::unordered_multimap<boost::beast::http::verb, Route> routes;
        std::function<void(Session::Session&, Request<boost::beast::http::empty_body> const&)> onNotFound;

        Implementation(
            std::function<void(Session::Session&, Request<boost::beast::http::empty_body> const&)> onNotFound)
            : routesMutex{}
            , routes{}
            , onNotFound{std::move(onNotFound)}
        {}

        std::optional<std::pair<Route const&, std::vector<std::string>>>
        findRoute(boost::beast::http::verb method, std::string const& path) const
        {
            std::scoped_lock lock{routesMutex};
            const auto equalRange = routes.equal_range(method);
            std::vector<std::string> regexMatches;
            for (auto iter = equalRange.first; iter != equalRange.second; ++iter)
            {
                if (iter->second.matches(path, regexMatches))
                    return std::pair<Route const&, std::vector<std::string>>(iter->second, std::move(regexMatches));
            }

            return std::nullopt;
        }
    };
    //##################################################################################################################
    Router::Router(std::function<void(Session::Session&, Request<boost::beast::http::empty_body> const&)> onNotFound)
        : impl_{std::make_unique<Implementation>(std::move(onNotFound))}
    {}
    //------------------------------------------------------------------------------------------------------------------
    ROAR_PIMPL_SPECIAL_FUNCTIONS_IMPL(Router);
    //------------------------------------------------------------------------------------------------------------------
    void Router::addRoutes(std::unordered_multimap<boost::beast::http::verb, ProtoRoute>&& routes)
    {
        std::scoped_lock lock{impl_->routesMutex};
        for (auto&& [key, proto] : routes)
            impl_->routes.insert(std::make_pair(key, Route{std::move(proto)}));
    }
    //------------------------------------------------------------------------------------------------------------------
    void Router::followRoute(Session::Session& session, Request<boost::beast::http::empty_body>& request)
    {
        auto result = impl_->findRoute(request.method(), request.path());
        if (!result)
            return impl_->onNotFound(session, request);
        request.pathMatches(std::move(result->second));
        result->first(session, request);
    }
    //##################################################################################################################
}