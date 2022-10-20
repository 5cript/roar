#pragma once

#include "ipv4.hpp"
#include "ipv6.hpp"
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/spirit/home/x3/support/ast/position_tagged.hpp>
#include <boost/leaf.hpp>

#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace Roar
{
    struct Url : boost::spirit::x3::position_tagged
    {
        struct UserInfo : boost::spirit::x3::position_tagged
        {
            std::string user{};
            std::string password{};
        };

        struct Host
            : boost::spirit::x3::position_tagged
            , std::variant<std::string, Ipv4, Ipv6>
        {
            using base_type = std::variant<std::string, Ipv4, Ipv6>;
            using base_type::variant;

            base_type const& as_variant() const
            {
                return *this;
            }
            base_type& as_variant()
            {
                return *this;
            }
        };

        struct Authority : boost::spirit::x3::position_tagged
        {
            struct Remote : boost::spirit::x3::position_tagged
            {
                Url::Host host{};
                std::optional<unsigned short> port{};
            };
            std::optional<Url::UserInfo> userInfo{std::nullopt};
            Remote remote{};
        };

        std::string scheme;
        Authority authority;
        std::vector<std::string> path;
        std::unordered_map<std::string, std::string> query;
        std::string fragment;

        /**
         * @brief Converts the url to a string containing all parts: https://u:p@bla.com:80/path?k=v
         *
         * @param doUrlEncode Will url encode necessary characters when true.
         * @param includeFragment The fragment part will not be added when false.
         * @return std::string the url
         */
        std::string toString(bool doUrlEncode = true, bool includeFragment = true) const;

        /**
         * @brief Will only create the path as a string with a leading slash: "/path/to/resource".
         *
         * @param doUrlEncode Will url encode necessary characters when true.
         * @return std::string the path part of the url.
         */
        std::string pathAsString(bool doUrlEncode = true) const;

        /**
         * @brief Returns the host part of the url as a string. Does not include the port! IPv6 addresses
         * will be enclosed in square brackets.
         *
         * @return std::string The domain/ip of the url.
         */
        std::string hostAsString() const;

        /**
         * @brief Converts the entire authority part to string: "user:password@domain.com:770".
         *
         * @param doUrlEncode Will url encode necessary characters when true.
         * @return std::string The authority part of the url as string.
         */
        std::string getAuthority(bool doUrlEncode) const;

        /**
         * @brief Converts the authority part and scheme to string: "https://user:password@domain.com:770"
         *
         * @param doUrlEncode Will url encode necessary characters when true.
         * @return std::string The scheme and authority of the url.
         */
        std::string schemeAndAuthority(bool doUrlEncode = true) const;

        /**
         * @brief Parses a string that is a url.
         *
         * @param url A url string.
         * @return std::optional<Url> A url or a runtime_error.
         */
        static boost::leaf::result<Url> fromString(std::string url);

        /**
         * @brief Splits up a path string into a vector compatible with this class. An empty path returns
         * an empty vector.
         *
         * @param path A path string starting with a leading slash: "/path/to/resource".
         * @return Fallible<std::vector<std::string>> The path segments ["path", "to", "resource"].
         */
        static boost::leaf::result<std::vector<std::string>> parsePath(std::string const& path);
    };
} // namespace Roar

// For parser to directly write to actual structs:
BOOST_FUSION_ADAPT_STRUCT(Roar::Url::UserInfo, user, password)
BOOST_FUSION_ADAPT_STRUCT(Roar::Url::Authority, userInfo, remote)
BOOST_FUSION_ADAPT_STRUCT(Roar::Url::Authority::Remote, host, port)
BOOST_FUSION_ADAPT_STRUCT(Roar::Url, scheme, authority, path, query, fragment)
