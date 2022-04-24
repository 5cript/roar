#pragma once

#include <boost/beast/http/message.hpp>
#include <boost/beast/http/empty_body.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/constants.hpp>
#include <boost/algorithm/string/split.hpp>

#ifdef ROAR_ENABLE_NLOHMANN_JSON
#    include <nlohmann/json.hpp>
#endif

#include <optional>
#include <string>
#include <vector>
#include <unordered_map>

namespace Roar
{
    template <typename BodyT>
    class Request : public boost::beast::http::request<BodyT>
    {
      public:
        using beast_request = boost::beast::http::request<BodyT>;
        Request(beast_request&& req)
            : boost::beast::http::request<BodyT>(std::move(req))
            , regexMatches_{}
        {
            parseTarget();
        }

        auto const& path() const
        {
            return path_;
        }
        auto const& query() const
        {
            return query_;
        }
        auto const& pathMatches() const
        {
            return regexMatches_;
        }
        void pathMatches(std::vector<std::string>&& matches)
        {
            regexMatches_ = std::move(matches);
        }

#ifdef ROAR_ENABLE_NLOHMANN_JSON
        nlohmann::json json()
        {
            return nlohmann::json::parse(this->body());
        }
#endif

      private:
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

        std::unordered_map<std::string, std::string> parseQuery(std::string_view queryString)
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