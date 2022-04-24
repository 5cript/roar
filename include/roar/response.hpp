#pragma once

#include <roar/cors.hpp>
#include <roar/request.hpp>

#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>

#ifdef ROAR_ENABLE_NLOHMANN_JSON
#    include <nlohmann/json.hpp>
#endif

#include <type_traits>
#include <numeric>
#include <iterator>

namespace Roar
{
    template <typename BodyT>
    class Response
    {
      public:
        using body_type = BodyT;

        Response()
            : response_{}
        {
            response_.version(11);
        }

        /**
         * @brief Sets the result code of the response_
         *
         * @param status A status code.
         */
        Response& result(boost::beast::http::status status)
        {
            response_.result(status);
            return *this;
        }

        /**
         * @brief Alias for result.
         *
         * @param status A status code.
         */
        Response& status(boost::beast::http::status status)
        {
            response_.result(status);
            return *this;
        }

        auto& body()
        {
            return response_.body();
        }

        template <typename BodyType = BodyT>
        std::enable_if_t<std::is_same_v<BodyType, boost::beast::http::string_body>, Response&>
        body(std::string_view body)
        {
            response_.body() = body;
        }

        template <typename BodyType = BodyT>
        std::enable_if_t<std::is_same_v<BodyType, boost::beast::http::string_body>, Response&> body(char const* body)
        {
            response_.body() = body;
            return *this;
        }

        template <typename BodyType = BodyT>
        std::enable_if_t<std::is_same_v<BodyType, boost::beast::http::string_body>, Response&>
        body(std::string const& body)
        {
            response_.body() = body;
            return *this;
        }

#ifdef ROAR_ENABLE_NLOHMANN_JSON
        template <typename BodyType = BodyT>
        std::enable_if_t<std::is_same_v<BodyType, boost::beast::http::string_body>, Response&>
        body(nlohmann::json const& body)
        {
            response_.body() = body.dump();
            return *this;
        }
#endif

        Response& chunked(bool activate = true)
        {
            response_.chunked(activate);
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
            response_.set(field, type);
            return *this;
        }

        Response& setHeader(boost::beast::http::field field, char const* type)
        {
            response_.set(field, type);
            return *this;
        }

        Response& preparePayload()
        {
            response_.prepare_payload();
            return *this;
        }

        template <typename RequestBodyT>
        void allowCors(Request<RequestBodyT> const& req, std::optional<CorsSettings> cors = std::nullopt)
        {
            if (!cors)
                cors = makeDefaultCorsSettings(req.method());

            response_.set(
                boost::beast::http::field::access_control_allow_origin,
                cors->allowedOrigin(std::string{req[boost::beast::http::field::origin]}));

            const auto joinList = [](std::vector<std::string> const& list) {
                return list.empty() ? std::string{}
                                    : std::accumulate(
                                          std::next(std::begin(list)),
                                          std::end(list),
                                          list.front(),
                                          [](std::string accum, std::string const& elem) {
                                              return std::move(accum) + "," + elem;
                                          });
            };

            // Preflight requests require both methods and allowHeaders to be set.
            std::string requestedMethod;
            if (req.find(boost::beast::http::field::access_control_request_method) != std::end(req))
                requestedMethod = std::string{req[boost::beast::http::field::access_control_request_method]};

            std::vector<std::string> requestedHeaders;
            if (req.find(boost::beast::http::field::access_control_request_headers) != std::end(req))
                requestedHeaders =
                    req.splitCommaSeperatedHeaderEntry(boost::beast::http::field::access_control_request_headers);

            if (const auto&& allowedMethods = cors->methodAllowSelection({std::move(requestedMethod)});
                !allowedMethods.empty())
                response_.set(boost::beast::http::field::access_control_allow_methods, joinList(allowedMethods));

            if (const auto&& allowedHeaders = cors->headerAllowSelection(requestedHeaders); !allowedHeaders.empty())
                response_.set(boost::beast::http::field::access_control_allow_headers, joinList(allowedHeaders));

            if (cors->allowCredentials.has_value())
                response_.set(
                    boost::beast::http::field::access_control_allow_credentials,
                    *cors->allowCredentials ? "true" : "false");
        }

        template <typename SessionT>
        void send(SessionT& session)
        {
            session.send(std::move(response_));
        }

        boost::beast::http::response<BodyT>&& response() &&
        {
            return std::move(response_);
        }

      private:
        boost::beast::http::response<BodyT> response_;
    };
}