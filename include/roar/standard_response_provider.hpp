#pragma once

#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/empty_body.hpp>
#include <boost/beast/http/status.hpp>

#include <string_view>

namespace Roar
{
    class Session;
    template <typename>
    class Request;

    /**
     * @brief Implement your own StandardResponseProvider to make custom 404 pages etc.
     *
     */
    class StandardResponseProvider
    {
      public:
        StandardResponseProvider() = default;
        virtual ~StandardResponseProvider() = default;
        StandardResponseProvider(StandardResponseProvider const&) = default;
        StandardResponseProvider(StandardResponseProvider&&) = default;
        StandardResponseProvider& operator=(StandardResponseProvider const&) = default;
        StandardResponseProvider& operator=(StandardResponseProvider&&) = default;

        virtual boost::beast::http::response<boost::beast::http::string_body>
        makeStandardResponse(Session& session, boost::beast::http::status, std::string_view additionalInfo) const = 0;
    };
}