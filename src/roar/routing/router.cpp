#include <roar/routing/router.hpp>
#include <roar/routing/route.hpp>
#include <roar/session/session.hpp>
#include <roar/request.hpp>

#include <utility>
#include <mutex>

namespace Roar
{
    //##################################################################################################################
    struct Router::Implementation
    {
        mutable std::mutex routesMutex;
        // TODO: improve performance by using a cleverer structure.
        std::unordered_multimap<boost::beast::http::verb, Route> stringRoutes;
        std::unordered_multimap<boost::beast::http::verb, Route> regexRoutes;
        std::shared_ptr<const StandardResponseProvider> standardResponseProvider;

        Implementation(std::shared_ptr<const StandardResponseProvider> standardResponseProvider)
            : routesMutex{}
            , stringRoutes{}
            , regexRoutes{}
            , standardResponseProvider{std::move(standardResponseProvider)}
        {}

        std::optional<std::pair<Route const&, std::vector<std::string>>> findRouteImpl(
            boost::beast::http::verb method,
            std::string const& path,
            std::unordered_multimap<boost::beast::http::verb, Route> const& routeContainer) const
        {
            std::scoped_lock lock{routesMutex};
            const auto equalRange = routeContainer.equal_range(method);
            std::vector<std::string> regexMatches;
            for (auto iter = equalRange.first; iter != equalRange.second; ++iter)
            {
                if (iter->second.matches(path, regexMatches))
                    return std::pair<Route const&, std::vector<std::string>>(iter->second, std::move(regexMatches));
            }

            return std::nullopt;
        }

        std::optional<std::pair<Route const&, std::vector<std::string>>>
        findRoute(boost::beast::http::verb method, std::string const& path) const
        {
            const auto res = findRouteImpl(method, path, stringRoutes);
            if (res)
                return res;
            const auto res2 = findRouteImpl(method, path, regexRoutes);
            return res2;
        }
    };
    //##################################################################################################################
    Router::Router(std::shared_ptr<const StandardResponseProvider> standardResponseProvider)
        : impl_{std::make_unique<Implementation>(std::move(standardResponseProvider))}
    {}
    //------------------------------------------------------------------------------------------------------------------
    ROAR_PIMPL_SPECIAL_FUNCTIONS_IMPL(Router);
    //------------------------------------------------------------------------------------------------------------------
    void Router::addRoutes(std::unordered_multimap<boost::beast::http::verb, ProtoRoute>&& routes)
    {
        std::scoped_lock lock{impl_->routesMutex};
        for (auto&& [key, proto] : routes)
        {
            if (std::holds_alternative<std::string>(proto.path))
                impl_->stringRoutes.insert(std::make_pair(key, Route{std::move(proto)}));
            else
                impl_->regexRoutes.insert(std::make_pair(key, Route{std::move(proto)}));
        }
    }
    //------------------------------------------------------------------------------------------------------------------
    void Router::followRoute(Session& session, Request<boost::beast::http::empty_body> request)
    {
        using namespace std::string_literals;
        try
        {
            auto result = impl_->findRoute(request.method(), request.path());
            if (!result)
            {
                session.send(impl_->standardResponseProvider->makeStandardResponse(
                    session, boost::beast::http::status::not_found, "No route for path: "s + request.path()));
                return;
            }
            request.pathMatches(std::move(result->second));
            try
            {
                result->first(session, std::move(request), *impl_->standardResponseProvider);
            }
            catch (std::exception const& exc)
            {
                impl_->standardResponseProvider->makeStandardResponse(
                    session,
                    boost::beast::http::status::internal_server_error,
                    "An exception was thrown in the request handler: "s + exc.what());
            }
        }
        catch (std::exception const& exc)
        {
            impl_->standardResponseProvider->makeStandardResponse(
                session, boost::beast::http::status::internal_server_error, "Error during routing: "s + exc.what());
            return;
        }
    }
    //##################################################################################################################
}