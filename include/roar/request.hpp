#pragma once

#include <roar/authorization/basic_auth.hpp>
#include <roar/authorization/digest_auth.hpp>
#include <roar/authorization/authorization.hpp>
#include <roar/mechanics/ranges.hpp>

#include <boost/beast/http/message.hpp>
#include <boost/beast/http/empty_body.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/constants.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/beast/websocket/rfc6455.hpp>
#include <boost/lexical_cast.hpp>

#ifdef ROAR_ENABLE_NLOHMANN_JSON
#    include <nlohmann/json.hpp>
#endif

#include <optional>
#include <string>
#include <vector>
#include <unordered_map>
#include <deque>

namespace Roar
{
    namespace Detail
    {
        struct RequestExtensions
        {
            std::optional<std::vector<std::string>> regexMatches_;
            std::string path_;
            std::unordered_map<std::string, std::string> query_;
        };
    }

    /**
     * @brief This class extends the boost::beast::http::request<BodyT> with additional convenience.
     *
     * @tparam BodyT Body type of the request. (empty_body, string_body, ...) See boost beast bodies.
     */
    template <typename BodyT>
    class Request
        : public boost::beast::http::request<BodyT>
        , private Detail::RequestExtensions
    {
      public:
        using beast_request = boost::beast::http::request<BodyT>;

        /**
         * @brief Construct a new Request object from a beast request to upgrade it.
         *
         * @param req A beast http request.
         */
        explicit Request(beast_request req)
            : boost::beast::http::request<BodyT>(std::move(req))
            , Detail::RequestExtensions{}
        {
            parseTarget();
        }

        /**
         * @brief Construct a new Request object from a beast request and takes extensions of a previous request object.
         *
         * @param req
         */
        explicit Request(beast_request req, Detail::RequestExtensions&& extensions)
            : boost::beast::http::request<BodyT>(std::move(req))
            , Detail::RequestExtensions{std::move(extensions)}
        {
            parseTarget();
        }

        /**
         * @brief Returns only the path of the url.
         *
         * @return auto const& The path.
         */
        auto const& path() const
        {
            return path_;
        }

        /**
         * @brief Returns the query part of the url as an unordered_map<string, string>.
         *
         * @return auto const& The query part.
         */
        auto const& query() const
        {
            return query_;
        }

        /**
         * @brief Retrieves regex matches for this request with the registered route.
         *
         * @return auto const& matches[0] = full match, matches[1] = first capture group, ...
         */
        auto const& pathMatches() const
        {
            return regexMatches_;
        }

        /**
         * @brief Sets regex matches for this request with the registered route.
         *
         * @param matches matches[0] = full match, matches[1] = first capture group, ...
         */
        void pathMatches(std::vector<std::string>&& matches)
        {
            regexMatches_ = std::move(matches);
        }

        /**
         * @brief Retrieves a header which value is a typically comma seperated list as a vector of string.
         *
         * @return std::vector<std::string> A value list.
         */
        template <typename FieldT>
        std::vector<std::string> splitCommaSeperatedHeaderEntry(FieldT&& field) const
        {
            std::vector<std::string> splitList;
            boost::algorithm::split(
                splitList,
                std::string{this->at(std::forward<FieldT>(field))},
                boost::algorithm::is_any_of(","),
                boost::algorithm::token_compress_on);

            for (auto& element : splitList)
                boost::algorithm::trim(element);

            return splitList;
        }

        /**
         * @brief Is this a websocket upgrade request?
         *
         * @return true Yes it is.
         * @return false No it is not.
         */
        bool isWebsocketUpgrade() const
        {
            return boost::beast::websocket::is_upgrade(*this);
        }

#ifdef ROAR_ENABLE_NLOHMANN_JSON
        /**
         * @brief Parses the body as json, when the body is a string body.
         * @throws Whatever a json::parse throws when the json is invalid.
         *
         * @return nlohmann::json The body as json.
         */
        template <typename Body = BodyT>
        std::enable_if_t<std::is_same_v<Body, boost::beast::http::string_body>, nlohmann::json> json() const
        {
            return nlohmann::json::parse(this->body());
        }

        /**
         * @brief Rips extensions out of this request object intended to be implanted into another request object.
         *
         * @return Detail::RequestExtensions
         */
        Detail::RequestExtensions ejectExtensions() &&
        {
            return {.regexMatches_ = std::move(regexMatches_), .path_ = std::move(path_), .query_ = std::move(query_)};
        }
#endif

        bool expectsContinue() const
        {
            auto iter = this->find(boost::beast::http::field::expect);
            if (iter == std::end(*this))
                return false;
            else
                return iter->value() == "100-continue";
        }

        std::optional<std::size_t> contentLength() const
        {
            bool allow = false;
            auto contentLength = this->find("Content-Length");
            if (contentLength == std::end(*this))
                return std::nullopt;
            else
                return boost::lexical_cast<std::size_t>(contentLength->value());
        }

        /**
         * @brief Extracts the Range header.
         *
         * @return Ranges a parsed format of the range header.
         */
        std::optional<Ranges> ranges() const
        {
            auto iter = this->find(boost::beast::http::field::range);
            if (iter == std::end(*this))
                return std::nullopt;
            return Ranges::fromString(std::string{iter->value()});
        }

        /**
         * @brief Returns the authorization scheme of the Authorization header if it exists.
         */
        std::optional<AuthorizationScheme> authorizationScheme() const
        {
            auto iter = this->find(boost::beast::http::field::authorization);
            if (iter == std::end(*this))
                return std::nullopt;
            auto spacePos = iter->value().find(' ');
            if (spacePos == std::string::npos)
                return std::nullopt;
            return authorizationSchemeFromString(iter->value().substr(0, spacePos));
        }

        /**
         * @brief Returns a basic auth object if the authorization scheme is "Basic".
         */
        std::optional<BasicAuth> basicAuth() const
        {
            auto iter = this->find(boost::beast::http::field::authorization);
            if (iter == std::end(*this))
                return std::nullopt;
            auto spacePos = iter->value().find(' ');
            if (spacePos == std::string::npos)
                return std::nullopt;
            if (iter->value().substr(0, spacePos) != "Basic")
                return std::nullopt;
            return BasicAuth::fromBase64(iter->value().substr(spacePos + 1));
        }

        /**
         * @brief Returns a digest auth object with all digest auth information.
         */
        std::optional<DigestAuth> digestAuth() const
        {
            auto iter = this->find(boost::beast::http::field::authorization);
            if (iter == std::end(*this))
                return std::nullopt;
            auto spacePos = iter->value().find(' ');
            if (spacePos == std::string::npos)
                return std::nullopt;
            if (iter->value().substr(0, spacePos) != "Digest")
                return std::nullopt;
            return DigestAuth::fromParameters(iter->value().substr(spacePos + 1));
        }

        /**
         * @brief Returns token data of bearer authentication method.
         */
        std::optional<std::string> bearerAuth() const
        {
            auto iter = this->find(boost::beast::http::field::authorization);
            if (iter == std::end(*this))
                return std::nullopt;
            auto spacePos = iter->value().find(' ');
            if (spacePos == std::string::npos)
                return std::nullopt;
            if (iter->value().substr(0, spacePos) != "Bearer")
                return std::nullopt;
            return std::string{iter->value().substr(spacePos + 1)};
        }

      private:
        /**
         * @brief Extracts path and query from boost::beast::http::request::target()
         */
        void parseTarget()
        {
            auto path = std::string{this->target()};
            auto queryPos = path.find_last_of('?');
            if (queryPos != std::string::npos)
            {
                query_ = parseQuery(std::string_view(path).substr(queryPos + 1, path.size() - queryPos - 1));
                path.erase(queryPos);
            }
            using std::swap;
            swap(path, path_);
        }

        /**
         * @brief Transforms the query part from string to map.
         *
         * @param queryString The query as a complete string.
         * @return std::unordered_map<std::string, std::string> A map containing the query key/value pairs.
         */
        std::unordered_map<std::string, std::string> parseQuery(std::string_view queryString) const
        {
            std::vector<std::string> splitBySeperator;
            boost::algorithm::split(
                splitBySeperator, queryString, boost::algorithm::is_any_of("&;"), boost::algorithm::token_compress_off);

            std::unordered_map<std::string, std::string> query;
            for (auto const& pair : splitBySeperator)
            {
                auto pos = pair.find('=');
                if (pos == std::string::npos)
                    query[pair] = "";
                else
                    query[pair.substr(0, pos)] = pair.substr(pos + 1, pair.length() - pos - 1);
            }
            return query;
        }

      private:
        std::optional<std::vector<std::string>> regexMatches_;
        std::string path_;
        std::unordered_map<std::string, std::string> query_;
    };

    using EmptyBodyRequest = Request<boost::beast::http::empty_body>;
}