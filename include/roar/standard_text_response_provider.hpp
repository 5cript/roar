#pragma once

#include <roar/standard_response_provider.hpp>

namespace Roar
{
    /**
     * @brief This standard response provider implementation produces very simple text responses.
     */
    class StandardTextResponseProvider : public StandardResponseProvider
    {
      public:
        boost::beast::http::response<boost::beast::http::string_body>
        makeStandardResponse(Session& session, boost::beast::http::status status, std::string_view additionalInfo) const
        {
            // TODO: this once used prepareReply, can do better.
            auto res = boost::beast::http::response<boost::beast::http::string_body>(status, 11);
            res.set(boost::beast::http::field::content_type, "text/html");
            res.set(boost::beast::http::field::connection, "close");
            if (status != boost::beast::http::status::no_content)
                res.body() = std::string{obsolete_reason(status)} + "\n" + std::string{additionalInfo};
            res.prepare_payload();
            return res;
        }
    };
}