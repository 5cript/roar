#include <roar/mechanics/ranges.hpp>

#include <boost/spirit/home/x3.hpp>
#include <boost/fusion/adapted/struct.hpp>

#include <sstream>

BOOST_FUSION_ADAPT_STRUCT(Roar::Ranges, unit, ranges)
BOOST_FUSION_ADAPT_STRUCT(Roar::Ranges::Range, start, end)

namespace Roar
{
    namespace Detail
    {
        namespace x3 = boost::spirit::x3;

        struct RangeRuleTag;
        const auto rangeRule = x3::rule<RangeRuleTag, Ranges::Range>{"range"} = x3::ulong_long >> '-' >> x3::ulong_long;

        struct RangesRuleTag;
        const auto ranges = x3::rule<RangesRuleTag, Roar::Ranges>{"ranges"};
        const auto ranges_def = (+(x3::char_ - '=') >> '=' >> (rangeRule % ',')) > x3::eoi;

        BOOST_SPIRIT_DEFINE(ranges);
    }
    // ##################################################################################################################
    std::uint64_t Ranges::Range::size() const
    {
        return end - start;
    }
    //------------------------------------------------------------------------------------------------------------------
    std::string Ranges::Range::toString() const
    {
        std::stringstream sstr;
        sstr << start << '-' << end;
        return sstr.str();
    }
    // ##################################################################################################################
    std::optional<Ranges> Ranges::fromString(std::string const& str)
    {
        using namespace std::string_literals;
        auto iter = std::make_move_iterator(str.cbegin());
        const auto end = std::make_move_iterator(str.cend());
        using boost::spirit::x3::ascii::space;
        using namespace boost::spirit::x3;

        auto ranges = Ranges{};
        if (!boost::spirit::x3::phrase_parse(iter, end, Detail::ranges, space, ranges))
            return std::nullopt;

        for (auto const& range : ranges.ranges)
            if (range.end < range.start)
                return std::nullopt;

        return ranges;
    }
}