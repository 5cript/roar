#pragma once

#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/empty_body.hpp>
#include <boost/beast/http/status.hpp>

namespace Roar
{
    namespace Session
    {
        class Session;
    }
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

        virtual boost::beast::http::response<boost::beast::http::string_body> makeStandardResponse(
            Session::Session& session,
            Request<boost::beast::http::empty_body> const& req,
            boost::beast::http::status) = 0;
    };
}