#pragma once

#include <roar/standard_response_provider.hpp>

namespace Roar
{
    class StandardTextResponseProvider : public StandardResponseProvider
    {
      public:
        boost::beast::http::response<boost::beast::http::string_body> makeStandardResponse(
            Session& session,
            Request<boost::beast::http::empty_body> const& req,
            boost::beast::http::status status) const
        {
            // TODO: this once used prepareReply, can do better.
            auto res = boost::beast::http::response<boost::beast::http::string_body>(status, 11);
            res.set(boost::beast::http::field::content_type, "text/html");
            if (status != boost::beast::http::status::no_content)
                res.body() = std::string{obsolete_reason(status)};
            res.prepare_payload();
            return res;
        }
    };
}