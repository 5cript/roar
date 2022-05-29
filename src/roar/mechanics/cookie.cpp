#include <roar/mechanics/cookie.hpp>

#include <boost/algorithm/string.hpp>
#include <sstream>

namespace Roar
{
    //#####################################################################################################################
    Cookie::Cookie()
        : name_{}
        , value_{}
        , domain_{}
        , path_{}
        , sameSite_{}
        , expires_{}
        , maxAge_{0}
        , secure_{}
        , httpOnly_{false}
    {}
    //---------------------------------------------------------------------------------------------------------------------
    Cookie::Cookie(std::string const& name, std::string const& value)
        : name_{name}
        , value_{value}
        , domain_{}
        , path_{}
        , sameSite_{}
        , expires_{}
        , maxAge_{0}
        , secure_{}
        , httpOnly_{false}
    {}
    //---------------------------------------------------------------------------------------------------------------------
    std::unordered_map<std::string, std::string> Cookie::parseCookies(std::string const& cookieHeaderEntry)
    {
        using namespace std::string_literals;

        if (cookieHeaderEntry.empty())
            throw std::runtime_error("Cookie value is empty");

        std::vector<std::string> split;
        boost::algorithm::split(
            split, cookieHeaderEntry, boost::algorithm::is_any_of(";"), boost::algorithm::token_compress_on);

        std::unordered_map<std::string, std::string> result;
        for (auto iter = std::begin(split), end = std::end(split); iter != end; ++iter)
        {
            auto eqpos = iter->find('=');
            if (eqpos == std::string::npos)
                throw std::runtime_error("Cookie name value pair does not contain '=' character");

            auto left = boost::algorithm::trim_left_copy(iter->substr(0, eqpos));
            auto right = iter->substr(eqpos + 1, iter->size() - eqpos - 1);

            result[left] = right;
        }
        return result;
    }
    //---------------------------------------------------------------------------------------------------------------------
    Cookie& Cookie::setName(std::string const& name)
    {
        name_ = name;
        return *this;
    }
    //---------------------------------------------------------------------------------------------------------------------
    Cookie& Cookie::setValue(std::string const& value)
    {
        value_ = value;
        return *this;
    }
    //---------------------------------------------------------------------------------------------------------------------
    Cookie& Cookie::setExpiry(date const& expires)
    {
        expires_ = expires;
        return *this;
    }
    //---------------------------------------------------------------------------------------------------------------------
    Cookie& Cookie::makeSessionCookie()
    {
        expires_ = boost::none;
        return *this;
    }
    //---------------------------------------------------------------------------------------------------------------------
    Cookie& Cookie::setSecure(bool secure)
    {
        secure_ = secure;
        return *this;
    }
    //---------------------------------------------------------------------------------------------------------------------
    Cookie& Cookie::setHttpOnly(bool httpOnly)
    {
        httpOnly_ = httpOnly;
        return *this;
    }
    //---------------------------------------------------------------------------------------------------------------------
    Cookie& Cookie::setDomain(std::string const& domain)
    {
        domain_ = domain;
        return *this;
    }
    //---------------------------------------------------------------------------------------------------------------------
    Cookie& Cookie::setPath(std::string const& path)
    {
        path_ = path;
        return *this;
    }
    //---------------------------------------------------------------------------------------------------------------------
    Cookie& Cookie::setMaxAge(uint64_t age)
    {
        maxAge_ = age;
        return *this;
    }
    //---------------------------------------------------------------------------------------------------------------------
    Cookie& Cookie::setSameSite(std::string const& sameSite)
    {
        sameSite_ = sameSite;
        return *this;
    }
    //---------------------------------------------------------------------------------------------------------------------
    std::string Cookie::getName() const
    {
        return name_;
    }
    //---------------------------------------------------------------------------------------------------------------------
    std::string Cookie::getValue() const
    {
        return value_;
    }
    //---------------------------------------------------------------------------------------------------------------------
    bool Cookie::isSecure() const
    {
        return secure_;
    }
    //---------------------------------------------------------------------------------------------------------------------
    bool Cookie::isHttpOnly() const
    {
        return httpOnly_;
    }
    //---------------------------------------------------------------------------------------------------------------------
    std::string Cookie::getPath() const
    {
        return path_;
    }
    //---------------------------------------------------------------------------------------------------------------------
    std::string Cookie::getDomain() const
    {
        return domain_;
    }
    //---------------------------------------------------------------------------------------------------------------------
    uint64_t Cookie::getMaxAge() const
    {
        return maxAge_;
    }
    //---------------------------------------------------------------------------------------------------------------------
    std::string Cookie::getSameSite() const
    {
        return sameSite_;
    }
    //---------------------------------------------------------------------------------------------------------------------
    std::string Cookie::toSetCookieString() const
    {
        std::stringstream sstr;
        sstr << name_ << "=" << value_;

        if (!domain_.empty())
            sstr << "; Domain=" << domain_;
        if (!path_.empty())
            sstr << "; Path=" << path_;
        if (!sameSite_.empty())
            sstr << "; SameSite=" << sameSite_;
        if (maxAge_ > 0)
            sstr << "; Max-Age=" << maxAge_;
        if (expires_)
            sstr << "; Expires=" << expires_.get().toGmtString();
        if (secure_)
            sstr << "; Secure";
        if (httpOnly_)
            sstr << "; HttpOnly";

        return sstr.str();
    }
    //#####################################################################################################################
} // namespace Roar
