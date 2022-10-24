#include <roar/url/url.hpp>

#include <boost/fusion/include/at_c.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/utility/annotate_on_success.hpp>
#include <boost/spirit/home/x3/support/utility/error_reporting.hpp>
#include <roar/url/parser/ipv4.hpp>
#include <roar/url/parser/ipv6.hpp>
#include <roar/url/encode.hpp>
#include <roar/utility/concat_containers.hpp>
#include <roar/utility/demangle.hpp>
#include <roar/utility/visit_overloaded.hpp>

#include <iomanip>
#include <iterator>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace Roar
{
    namespace Parser
    {
        namespace x3 = boost::spirit::x3;
        namespace ascii = boost::spirit::x3::ascii;

        using ascii::char_;
        using x3::alnum;
        using x3::alpha;
        using x3::repeat;
        using x3::ushort_;
        using x3::eoi;
        using x3::uint_parser;
        using boost::fusion::at_c;

        struct UrlTag;
        const x3::rule<UrlTag, Url> url = "url";

        // Path: (https://en.wikipedia.org/wiki/Percent-encoding)
        struct UnreservedTag;
        const auto unreserved = x3::rule<UnreservedTag, char>{} = alnum | char_("-._~");

        struct GenDelimsTag;
        const auto genDelims = x3::rule<GenDelimsTag, char>{"gen-delims"} = char_(":/?#[]@");

        struct SubDelimsTag;
        const auto subDelims = x3::rule<SubDelimsTag, char>{"sub-delims"} = char_("!$&'()*+,;=");

        struct Leniency;
        const auto leniency = x3::rule<Leniency, char>("leniency") = char_("\"<> ");

        struct ReservedTag;
        const auto reserved = x3::rule<ReservedTag, char>{"reserved"} = genDelims | subDelims;

        struct PercentEncodedTag;
        const auto percentEncoded = x3::rule<PercentEncodedTag, char>{"percentEncoded"} =
            '%' >> uint_parser<unsigned char, 16, 2, 2>{}[([](auto& ctx) {
                _val(ctx) = static_cast<char>(_attr(ctx));
            })];

        struct PathCharacterTag;
        const auto pathCharacter = x3::rule<PathCharacterTag, char>{"pathCharacter"} =
            (percentEncoded | leniency | unreserved | reserved | char_(":@")) - char_("#?/");

        struct PathTag;
        const auto path = x3::rule<PathTag, std::vector<std::string>>{"path"} =
            ((*pathCharacter) % '/')[([](auto& ctx) {
                // first path segment must not be empty, because that may be mistaken with a // of the
                // authority part.
                if (!_attr(ctx).empty())
                    _pass(ctx) = !_attr(ctx).front().empty();
                _val(ctx) = _attr(ctx);
            })];

        // Scheme:
        struct SchemeAllowedCharacterTag;
        const auto schemeAllowedChar = x3::rule<SchemeAllowedCharacterTag, char>{"schemeAllowedChar"} =
            alnum | char_('+') | char_('.') | char_('-');

        struct SchemeTag;
        const auto scheme = x3::rule<SchemeTag, std::string>{"scheme"} = alpha[([](auto& ctx) {
                                                                             _val(ctx).push_back(_attr(ctx));
                                                                         })] >>
            *(schemeAllowedChar - char_(':'))[([](auto& ctx) {
                                                                             _val(ctx).push_back(_attr(ctx));
                                                                         })];

        // Authority:
        struct CredentialsCharacterTag;
        const auto credentialsCharacter = x3::rule<CredentialsCharacterTag, char>{"credentialsCharacter"} =
            percentEncoded | unreserved | reserved;

        struct UserInfoTag;
        const auto userInfo = x3::rule<UserInfoTag, Url::UserInfo>{"userInfo"} = +(credentialsCharacter - ':') >> ':' >>
            *(credentialsCharacter - '@');

        struct DomainCharacterTag;
        const auto domainCharacter = x3::rule<DomainCharacterTag, char>("domainCharacter") = alnum | char_("-.");

        // Is less strict. Length limitations are ignored.
        struct DomainTag;
        const auto domain = x3::rule<DomainTag, std::string>{"domain"} = +domainCharacter;

        struct HostTag;
        const auto host = x3::rule<HostTag, Url::Host>{"host"} = ('[' >> ipv6 >> ']') | (ipv4 >> &char_(":/")) | domain;

        struct RemoteTag;
        const auto remote = x3::rule<RemoteTag, Url::Authority::Remote>{"remote"} = host[([](auto& ctx) {
                                                                                        _val(ctx).host = _attr(ctx);
                                                                                    })] >>
            -(char_(':') >> ushort_[([](auto& ctx) {
                                                                                        _val(ctx).port = _attr(ctx);
                                                                                    })]);

        struct AuthorityTag;
        const auto authority = x3::rule<AuthorityTag, Url::Authority>{"authority"} = -(userInfo >> "@")[([](auto& ctx) {
            _val(ctx).userInfo = _attr(ctx);
        })] >>
            remote[([](auto& ctx) {
                _val(ctx).remote = _attr(ctx);
            })];

        // Query:
        struct QueryCharacterTag;
        const auto queryCharacter = x3::rule<QueryCharacterTag, char>{"queryCharacter"} = pathCharacter | char_("/?");

        struct QueryKeyTag;
        const auto queryKey = x3::rule<QueryKeyTag, std::string>{"queryKey"} =
            +(queryCharacter - char_("=&;"))[([](auto& ctx) {
                _val(ctx).push_back(_attr(ctx));
            })];

        struct QueryValueTag;
        const auto queryValue = x3::rule<QueryValueTag, std::string>{"queryValue"} =
            *(queryCharacter - char_("&;#"))[([](auto& ctx) {
                _val(ctx).push_back(_attr(ctx));
            })];

        struct QueryTag;
        const auto query = x3::rule<QueryTag, std::unordered_map<std::string, std::string>>{"query"} =
            ((queryKey > "=" > queryValue)[([](auto& ctx) {
                 _val(ctx).insert(std::make_pair(at_c<0>(_attr(ctx)), at_c<1>(_attr(ctx))));
             })] %
             char_("&;"));

        // Fragment (Should never be received server side, but for full capabilities its looked for):
        struct FragmentTag;
        const auto fragment = x3::rule<FragmentTag, std::string>{"fragment"} =
            *(unreserved | reserved | percentEncoded | char_(":@"));

        // Finally URL:
        const auto url_def = scheme > ':' > -("//" > authority) > -("/" > -path) > -("?" > query) >
            -("#" > fragment) > eoi;

        BOOST_SPIRIT_DEFINE(url);
    } // namespace Parser

    struct ErrorHandler
    {
        template <typename Iterator, typename Exception, typename Context>
        boost::spirit::x3::error_handler_result
        on_error(Iterator&, Iterator const&, Exception const& x, Context const& context)
        {
            auto& error_handler = boost::spirit::x3::get<boost::spirit::x3::error_handler_tag>(context).get();
            std::string message = "Error! Expecting: " + x.which() + " here:";
            error_handler(x.where(), message);
            return boost::spirit::x3::error_handler_result::fail;
        }
    };

    std::string Url::pathAsString(bool doUrlEncode) const
    {
        std::stringstream result;
        for (auto const& pathPart : path)
        {
            if (doUrlEncode)
                result << "/" << urlEncode(pathPart);
            else
                result << "/" << pathPart;
        }
        return result.str();
    }

    std::string Url::toString(bool doUrlEncode, bool includeFragment) const
    {
        std::stringstream result;
        result << schemeAndAuthority(doUrlEncode);
        result << pathAsString(doUrlEncode);
        if (!query.empty())
        {
            result.put('?');
            for (auto iter = std::begin(query), end = std::end(query); iter != end; ++iter)
            {
                if (doUrlEncode)
                    result << urlEncode(iter->first) << '=' << urlEncode(iter->second);
                else
                    result << iter->first << '=' << iter->second;
                if (std::next(iter) != end)
                    result << '&';
            }
        }
        if (includeFragment && !fragment.empty())
            result << '#' << fragment;
        return result.str();
    }

    std::string Url::schemeAndAuthority(bool doUrlEncode) const
    {
        std::stringstream result;
        result << scheme << "://" << getAuthority(doUrlEncode);
        return result.str();
    }

    std::string Url::hostAsString() const
    {
        std::stringstream result;
        visitOverloaded(
            authority.remote.host,
            [&result](std::string const& domain) {
                result << domain;
            },
            [&result](Ipv4 const& ipv4) {
                result << ipv4.toString();
            },
            [&result](Ipv6 const& ipv6) {
                result << '[' << ipv6.toString() << ']';
            });
        return result.str();
    }

    std::string Url::getAuthority(bool doUrlEncode) const
    {
        std::stringstream result;
        if (authority.userInfo)
        {
            if (doUrlEncode)
                result << urlEncode(authority.userInfo->user) << ':' << urlEncode(authority.userInfo->password) << '@';
            else
                result << authority.userInfo->user << ':' << authority.userInfo->password << '@';
        }
        result << hostAsString();
        if (authority.remote.port)
            result << ':' << std::dec << *authority.remote.port;
        return result.str();
    }

    boost::leaf::result<Url> Url::fromString(std::string urlString)
    {
        using namespace std::string_literals;
        auto iter = std::make_move_iterator(urlString.cbegin());
        const auto end = std::make_move_iterator(urlString.cend());
        Url result;
        using boost::spirit::x3::ascii::space;
        using namespace boost::spirit::x3;

        auto const parser = Parser::url;
        try
        {
            bool r = parse(iter, end, parser, result);
            if (!r)
                return boost::leaf::new_error(std::string("Could not parse url."));
        }
        catch (boost::spirit::x3::expectation_failure<decltype(iter)> const& exc)
        {
            const auto whichParser = demangleCppSymbol(
                exc.which(),
                {
                    {"boost::spirit::x3::", "x3::"},
                    {"Roar::Parser::", ""},
                },
                true);

            return boost::leaf::new_error(std::string{
                "Could not parse url, error at: "s + whichParser + "(" +
                std::string{exc.where(), std::min(end, exc.where() + 10)} + ")"});
        };
        return result;
    }

    boost::leaf::result<std::vector<std::string>> Url::parsePath(std::string const& path)
    {
        if (path.empty() || path == "/")
            return std::vector<std::string>{std::vector<std::string>{}};

        using namespace std::string_literals;
        auto iter = std::make_move_iterator(path.cbegin());
        const auto end = std::make_move_iterator(path.cend());
        std::vector<std::string> result;
        using boost::spirit::x3::ascii::space;
        using namespace boost::spirit::x3;

        auto const parser = '/' > Parser::path;
        try
        {
            bool r = parse(iter, end, parser, result);
            if (!r)
                return boost::leaf::new_error(std::string{"Could not parse path."});
        }
        catch (boost::spirit::x3::expectation_failure<decltype(iter)> const& exc)
        {
            return boost::leaf::new_error(std::string{
                "Could not parse path, error at: "s + demangleCppSymbol(exc.which()) + "(" +
                std::string{exc.where(), std::min(end, exc.where() + 10)} + ")"});
        }
        return result;
    }
} // namespace Roar
