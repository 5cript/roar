#pragma once

#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <type_traits>

namespace Roar
{
    template <typename BodyT>
    class Response
    {
      public:
        using body_type = BodyT;

        Response()
            : response{}
        {
            response.version(11);
        }

        /**
         * @brief Sets the result code of the response
         *
         * @param status A status code.
         */
        Response& result(boost::beast::http::status status)
        {
            response.result(status);
            return *this;
        }

        /**
         * @brief Alias for result.
         *
         * @param status A status code.
         */
        Response& status(boost::beast::http::status status)
        {
            response.result(status);
            return *this;
        }

        auto& body()
        {
            return response.body();
        }

        std::enable_if_t<std::is_same_v<body_type, boost::beast::http::string_body>, Response&>
        body(std::string_view body)
        {
            response.body() = body;
        }

        std::enable_if_t<std::is_same_v<body_type, boost::beast::http::string_body>, Response&> body(char const* body)
        {
            response.body() = body;
            return *this;
        }

        std::enable_if_t<std::is_same_v<body_type, boost::beast::http::string_body>, Response&>
        body(std::string const& body)
        {
            response.body() = body;
        }

        Response& chunked(bool activate = true)
        {
            response.chunked(activate);
            return *this;
        }

        Response& contentType(std::string const& type)
        {
            return setHeader(boost::beast::http::field::content_type, type);
        }

        Response& contentType(char const* type)
        {
            return setHeader(boost::beast::http::field::content_type, type);
        }

        Response& setHeader(boost::beast::http::field field, std::string const& type)
        {
            response.set(field, type);
            return *this;
        }

        Response& setHeader(boost::beast::http::field field, char const* type)
        {
            response.set(field, type);
            return *this;
        }

        Response& preparePayload()
        {
            response.prepare_payload();
            return *this;
        }

        template <typename SessionT>
        void send(SessionT& session)
        {
            session.send(std::move(response));
        }

      private:
        boost::beast::http::response<BodyT> response;
    };
}