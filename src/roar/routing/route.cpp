#include <roar/routing/route.hpp>

#include <roar/session/session.hpp>

namespace Roar
{
    //##################################################################################################################
    struct Route::Implementation : public ProtoRoute
    {
        Implementation(ProtoRoute&& proto)
            : ProtoRoute{std::move(proto)}
        {}
    };
    //##################################################################################################################
    Route::Route(ProtoRoute proto)
        : impl_{std::make_unique<Implementation>(std::move(proto))}
    {}
    //------------------------------------------------------------------------------------------------------------------
    ROAR_PIMPL_SPECIAL_FUNCTIONS_IMPL(Route);
    //------------------------------------------------------------------------------------------------------------------
    void Route::operator()(
        Session& session,
        Request<boost::beast::http::empty_body> req,
        StandardResponseProvider const& standardResponseProvider) const
    {
        session.setupRouteOptions(impl_->routeOptions);
        if (impl_->routeOptions.expectUpgrade && !req.isWebsocketUpgrade())
        {
            session.send(standardResponseProvider.makeStandardResponse(
                session, req, boost::beast::http::status::upgrade_required));
            return;
        }
        impl_->callRoute(session, std::move(req));
    }
    //------------------------------------------------------------------------------------------------------------------
    bool Route::matches(std::string const& path, std::vector<std::string>& regexMatches) const
    {
        return impl_->matches(path, regexMatches);
    }
    //##################################################################################################################
}