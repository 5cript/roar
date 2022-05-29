#pragma once

#include <roar/utility/date.hpp>

#include <boost/optional.hpp>

#include <string>
#include <unordered_map>
#include <cstdint>
#include <vector>

namespace Roar
{
    /**
     *  A Cookie helper class that contains Cookie data and provides utilitarian functions.
     */
    class Cookie
    {
      public:
        /**
         *  Creates a fresh session Cookie.
         *
         **/
        explicit Cookie();

        /**
         *  Create a Cookie, setting only name and value.
         **/
        explicit Cookie(std::string const& name, std::string const& value);

        /**
         *  Parses a Cookie request header entry and makes a Cookie from it.
         **/
        static std::unordered_map<std::string, std::string> parseCookies(std::string const& cookieHeaderEntry);

        /**
         *  Set Cookie name.
         **/
        Cookie& setName(std::string const& name);

        /**
         *  Set Cookie value.
         **/
        Cookie& setValue(std::string const& value);

        /**
         *  Set Cookie expiration date.
         **/
        Cookie& setExpiry(date const& expires);

        /**
         *  Removes the expiration date. All cookies are session cookies after creation, so no need to call this for
         *  fresh cookies.
         **/
        Cookie& makeSessionCookie();

        /**
         *  Set whether or not the Cookie shall be a secure Cookie.
         *
         *  @param secure The Cookie will only be used over HTTPS if true.
         **/
        Cookie& setSecure(bool secure);

        /**
         *  Set whether or not, the Cookie shall be accessible by scripting languages.
         *  This feature implemented by some browsers reduces XSS vulnerability.
         *
         *  @param http_only The Cookie will not be accessible by JavaScript etc when true.
         **/
        Cookie& setHttpOnly(bool http_only);

        /**
         *  Set the Domain attribute of the Cookie.
         **/
        Cookie& setDomain(std::string const& domain);

        /**
         *  Set the path on which the Cookie shall be accessible from.
         **/
        Cookie& setPath(std::string const& path);

        /**
         *  Set the max age attribute of the cookie.
         **/
        Cookie& setMaxAge(uint64_t age);

        Cookie& setSameSite(std::string const& same_site);

        /**
         *  Returns the Cookie name.
         **/
        std::string getName() const;

        /**
         *  Returns the Cookie value.
         **/
        std::string getValue() const;

        /**
         *  Returns whether the Cookie is secure or not.
         **/
        bool isSecure() const;

        /**
         *  Returns whether the Cookie is "HttpOnly" or not.
         **/
        bool isHttpOnly() const;

        /**
         *  Returns the path attribution.
         **/
        std::string getPath() const;

        /**
         *  Returns the domain attribution.
         **/
        std::string getDomain() const;

        /**
         *  Get the maximum age of the Cookie.
         **/
        uint64_t getMaxAge() const;

        std::string getSameSite() const;

        /**
         *  Creates a set Cookie string.
         **/
        std::string toSetCookieString() const;

      private:
        std::string name_;
        std::string value_;
        std::string domain_; // empty means not set
        std::string path_; // empty means not set
        std::string sameSite_;
        boost::optional<date> expires_;
        uint64_t maxAge_; // 0 = not set
        bool secure_;
        bool httpOnly_;
    };
}
