#pragma once

#include <roar/mechanics/cookie.hpp>
#include <roar/detail/promise_compat.hpp>
#include <roar/cors.hpp>
#include <roar/request.hpp>
#include <roar/error.hpp>
#include <roar/detail/template_utility/first_type.hpp>

#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/empty_body.hpp>
#include <promise-cpp/promise.hpp>

#ifdef ROAR_ENABLE_NLOHMANN_JSON
#    include <nlohmann/json.hpp>
#endif

#include <type_traits>
#include <numeric>
#include <string>
#include <optional>
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

        explicit Response(boost::beast::http::response<BodyT>&& res)
            : response_{std::move(res)}
        {
            response_.version(11);
        }

        template <typename... Forwards>
        explicit Response(Forwards... forwards)
            : response_{std::piecewise_construct, std::forward_as_tuple(std::forward<Forwards>(forwards)...)}
        {
            response_.version(11);
        }

        /**
         * @brief Sets the response status code.
         *
         * @param status A status code.
         */
        Response& status(boost::beast::http::status status)
        {
            response_.result(status);
            return *this;
        }

        /**
         * @brief Sets a set-cookie header entry.
         *
         * @param cookie A cookie.
         */
        Response& setCookie(Cookie const& cookie)
        {
            response_.set(boost::beast::http::field::set_cookie, cookie.toSetCookieString());
            return *this;
        }

        /**
         * @brief Retrieve the response body object.
         *
         * @return auto&
         */
        auto& body()
        {
            return response_.body();
        }

        /**
         * @brief Set keep alive.
         *
         * @param keepAlive
         * @return Response&
         */
        Response& keepAlive(bool keepAlive = true)
        {
            response_.keep_alive(keepAlive);
            return *this;
        }

        /**
         * @brief This function can be used to assign something to the body.
         *
         * @tparam T
         * @param toAssign
         * @return Response& Returned for chaining.
         */
        template <typename T>
        Response& body(T&& toAssign)
        {
#ifdef ROAR_ENABLE_NLOHMANN_JSON
            if constexpr (std::is_same_v<std::decay_t<T>, nlohmann::json>)
                response_.body() = std::forward<T>(toAssign).dump();
            else
#endif
                response_.body() = std::forward<T>(toAssign);
            preparePayload();
            return *this;
        }
        auto body() const
        {
            return response_.body();
        }

        /**
         * @brief (De)Activate chunked encoding.
         *
         * @param activate
         * @return Response& Returned for chaining.
         */
        Response& chunked(bool activate = true)
        {
            response_.chunked(activate);
            return *this;
        }
        bool chunked() const
        {
            return response_.chunked();
        }

        /**
         * @brief For setting of the content type.
         *
         * @param type
         * @return Response& Returned for chaining.
         */
        Response& contentType(std::string const& type)
        {
            return setHeader(boost::beast::http::field::content_type, type);
        }
        std::optional<std::string> contentType() const
        {
            if (auto it = response_.find(boost::beast::http::field::content_type); it != std::end(response_))
                return std::string{it->value()};
            return std::nullopt;
        }

        Response& contentLength(std::size_t length)
        {
            return setHeader(boost::beast::http::field::content_length, std::to_string(length));
        }
        std::optional<std::size_t> contentLength() const
        {
            if (auto it = response_.find(boost::beast::http::field::content_length); it != std::end(response_))
                return std::stoull(std::string{it->value()});
            return std::nullopt;
        }

        auto begin()
        {
            return response_.begin();
        }
        auto begin() const
        {
            return response_.begin();
        }
        auto end()
        {
            return response_.end();
        }
        auto end() const
        {
            return response_.end();
        }
        auto cbegin() const
        {
            return response_.cbegin();
        }
        auto cend() const
        {
            return response_.cend();
        }

        auto at(boost::beast::http::field field) const
        {
            return response_.at(field);
        }

        void clear()
        {
            response_.clear();
        }

        auto equal_range(boost::beast::http::field field) const
        {
            return response_.equal_range(field);
        }

        auto erase(boost::beast::http::field field)
        {
            return response_.erase(field);
        }
        auto find(boost::beast::http::field field) const
        {
            return response_.find(field);
        }
        bool hasContentLength() const
        {
            return response_.has_content_length();
        }
        bool hasField(boost::beast::http::field field) const
        {
            return response_.has_field(field);
        }
        bool hasKeepAlive() const
        {
            return response_.has_keep_alive();
        }
        auto payloadSize() const
        {
            return response_.payload_size();
        }
        auto reason() const
        {
            return response_.reason();
        }
        auto result() const
        {
            return response_.result();
        }
        auto resultInt() const
        {
            return response_.result_int();
        }
        void set(boost::beast::http::field field, std::string const& value)
        {
            response_.set(field, value);
        }
        auto target() const
        {
            return response_.target();
        }
        auto version() const
        {
            return response_.version();
        }

        /**
         * @brief For setting of the content type.
         *
         * @param type
         * @return Response& Returned for chaining.
         */
        Response& contentType(char const* type)
        {
            return setHeader(boost::beast::http::field::content_type, type);
        }

        /**
         * @brief Can be used to set a header field.
         *
         * @return Response& Returned for chaining.
         */
        Response& setHeader(boost::beast::http::field field, std::string const& value)
        {
            response_.set(field, value);
            return *this;
        }

        std::optional<std::string> getHeader(boost::beast::http::field field) const
        {
            if (auto it = response_.find(field); it != std::end(response_))
                return std::string{it->value()};
            return std::nullopt;
        }

        /**
         * @brief Can be used to set a header field.
         *
         * @return Response& Returned for chaining.
         */
        Response& setHeader(boost::beast::http::field field, char const* value)
        {
            response_.set(field, value);
            return *this;
        }

        /**
         * @brief Sets header values that are implicit by the body (like Content-Lenght).
         *
         * @return Response& Returned for chaining.
         */
        Response& preparePayload()
        {
            response_.prepare_payload();
            return *this;
        }

      private:
        auto joinList(std::vector<std::string> const& list)
        {
            return list.empty() ? std::string{}
                                : std::accumulate(
                                      std::next(std::begin(list)),
                                      std::end(list),
                                      list.front(),
                                      [](std::string accum, std::string const& elem) {
                                          return std::move(accum) + "," + elem;
                                      });
        };

      public:
        /**
         * @brief Sets cors headers.
         * @param req A request to base the cors headers off of.
         * @param cors A cors settings object. If not supplied, a very permissive setting is used.
         *
         * @return Response& Returned for chaining.
         */
        template <typename RequestBodyT>
        Response& enableCors(Request<RequestBodyT> const& req, std::optional<CorsSettings> cors = std::nullopt)
        {
            if (!cors)
                cors = makePermissiveCorsSettings(req.method());

            response_.set(
                boost::beast::http::field::access_control_allow_origin,
                cors->allowedOrigin(std::string{req[boost::beast::http::field::origin]}));

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

            return *this;
        }

        Response& enableCorsEverything()
        {
            response_.set(boost::beast::http::field::access_control_allow_origin, "*");
            response_.set(boost::beast::http::field::access_control_allow_methods, "*");
            response_.set(boost::beast::http::field::access_control_allow_headers, "*");
            response_.set(boost::beast::http::field::access_control_allow_credentials, "true");
            return *this;
        }

        /**
         * @brief Ejects the underlying boost response.
         */
        boost::beast::http::response<BodyT>&& response() &&
        {
            return std::move(response_);
        }

        /**
         * @brief Accessor for the underlying boost response.
         */
        boost::beast::http::response<BodyT>& response() &
        {
            return response_;
        }

      private:
        boost::beast::http::response<BodyT> response_;
    };
}