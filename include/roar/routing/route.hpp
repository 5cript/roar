#pragma once

#include <roar/routing/proto_route.hpp>
#include <roar/detail/pimpl_special_functions.hpp>
#include <roar/standard_response_provider.hpp>

#include <boost/beast/http/empty_body.hpp>

#include <memory>

namespace Roar
{
    class Session;
    template <typename>
    class Request;

    class Route
    {
      public:
        Route(ProtoRoute proto);
        ROAR_PIMPL_SPECIAL_FUNCTIONS(Route);

        void operator()(
            Session& session,
            Request<boost::beast::http::empty_body> req,
            StandardResponseProvider const& standardResponseProvider) const;
        bool matches(std::string const&, std::vector<std::string>&) const;

      private:
        struct Implementation;
        std::unique_ptr<Implementation> impl_;
    };
}