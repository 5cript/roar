#include <roar/routing/route.hpp>

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
    Route::Route(ProtoRoute&& proto)
        : impl_{std::make_unique<Implementation>(std::move(proto))}
    {}
    //------------------------------------------------------------------------------------------------------------------
    ROAR_PIMPL_SPECIAL_FUNCTIONS_IMPL(Route);
    //------------------------------------------------------------------------------------------------------------------
    void Route::operator()(Session& session, Request<boost::beast::http::empty_body> const& req) const
    {
        impl_->callRoute(session, req);
    }
    //------------------------------------------------------------------------------------------------------------------
    bool Route::matches(std::string const& path, std::vector<std::string>& regexMatches) const
    {
        return impl_->matches(path, regexMatches);
    }
    //##################################################################################################################
}