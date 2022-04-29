#pragma once

#include <roar/cors.hpp>
#include <roar/request.hpp>
#include <roar/detail/template_utility/first_type.hpp>

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

        template <typename T>
        auto body(T&& toAssign)
        {
#ifdef ROAR_ENABLE_NLOHMANN_JSON
            if constexpr (std::is_same_v<std::decay_t<T>, nlohmann::json>)
                response_.body() = std::forward<T>(toAssign).dump();
            else
#endif
                response_.body() = std::forward<T>(toAssign);
            return *this;
        }

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
        void enableCors(Request<RequestBodyT> const& req, std::optional<CorsSettings> cors = std::nullopt)
        {
            if (!cors)
                cors = makePermissiveCorsSettings(req.method());

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
            if (req.method() == boost::beast::http::verb::options)
            {
                std::string requestedMethod;
                if (req.find(boost::beast::http::field::access_control_request_method) != std::end(req))
                    requestedMethod = std::string{req[boost::beast::http::field::access_control_request_method]};

                if (const auto&& allowedMethods = cors->methodAllowSelection({std::move(requestedMethod)});
                    !allowedMethods.empty())
                    response_.set(boost::beast::http::field::access_control_allow_methods, joinList(allowedMethods));
            }

            std::vector<std::string> requestedHeaders;
            if (req.find(boost::beast::http::field::access_control_request_headers) != std::end(req))
                requestedHeaders =
                    req.splitCommaSeperatedHeaderEntry(boost::beast::http::field::access_control_request_headers);

            if (const auto&& allowedHeaders = cors->headerAllowSelection(requestedHeaders); !allowedHeaders.empty())
                response_.set(boost::beast::http::field::access_control_allow_headers, joinList(allowedHeaders));
            else
                response_.set(boost::beast::http::field::access_control_allow_headers, "*");

            if (cors->allowCredentials.has_value())
                response_.set(
                    boost::beast::http::field::access_control_allow_credentials,
                    *cors->allowCredentials ? "true" : "false");

            if (!cors->exposeHeaders.empty())
            {
                response_.set(
                    boost::beast::http::field::access_control_expose_headers,
                    std::accumulate(
                        std::next(std::begin(cors->exposeHeaders)),
                        std::end(cors->exposeHeaders),
                        std::string{boost::beast::http::to_string(cors->exposeHeaders.front())},
                        [](std::string accum, auto const& field) {
                            return std::move(accum) + "," + std::string{boost::beast::http::to_string(field)};
                        }));
            }
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