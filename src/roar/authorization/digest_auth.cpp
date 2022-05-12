#include <roar/authorization/digest_auth.hpp>
#include <roar/detail/fixed_string.hpp>

#include <boost/spirit/home/x3.hpp>
#include <boost/fusion/adapted/struct.hpp>

#include <cstddef>

BOOST_FUSION_ADAPT_STRUCT(Roar::DigestAuth, realm, uri, algorithm, nonce, nc, cnonce, qop, response, opaque)

namespace Roar
{
    namespace Parser
    {
        namespace x3 = boost::spirit::x3;

#define PARAM_PARSER(name) \
    (x3::lit(#name) >> '=' >> (('"' >> x3::lexeme[*(x3::char_ - '"')] >> '"')[([](auto& ctx) { \
                                   _val(ctx).name = _attr(ctx); \
                               })] | \
                               x3::lexeme[*(x3::char_ - ',')][([](auto& ctx) { \
                                   _val(ctx).name = _attr(ctx); \
                               })]))

        struct DigestTag;
        const auto digestRule = x3::rule<DigestTag, Roar::DigestAuth>{"digestAuth"} =
            (PARAM_PARSER(username) | PARAM_PARSER(realm) | PARAM_PARSER(uri) | PARAM_PARSER(algorithm) |
             PARAM_PARSER(nonce) | PARAM_PARSER(nc) | PARAM_PARSER(cnonce) | PARAM_PARSER(qop) |
             PARAM_PARSER(response) | PARAM_PARSER(opaque)) %
                ',' >>
            x3::eps[([](auto&) {})];
    }

    std::optional<DigestAuth> DigestAuth::fromParameters(std::string_view parameterList)
    {
        using namespace std::string_literals;
        auto iter = parameterList.cbegin();
        const auto end = parameterList.cend();
        using boost::spirit::x3::ascii::space;
        using namespace boost::spirit::x3;

        auto auth = DigestAuth{};

        try
        {
            if (!boost::spirit::x3::phrase_parse(iter, end, Parser::digestRule, space, auth))
                return std::nullopt;
        }
        catch (...)
        {
            return std::nullopt;
        }

        return auth;
    }

    DigestAuth::DigestAuth() = default;
    DigestAuth::DigestAuth(
        std::string username,
        std::string realm,
        std::string uri,
        std::string algorithm,
        std::string nonce,
        std::string nc,
        std::string cnonce,
        std::string qop,
        std::string response,
        std::string opaque)
        : username{std::move(username)}
        , realm{std::move(realm)}
        , uri{std::move(uri)}
        , algorithm{std::move(algorithm)}
        , nonce{std::move(nonce)}
        , nc{std::move(nc)}
        , cnonce{std::move(cnonce)}
        , qop{std::move(qop)}
        , response{std::move(response)}
        , opaque{std::move(opaque)}
    {}
}