#pragma once

#include <roar/url/ipv6.hpp>
#include <roar/url/parser/ipv4.hpp>
#include <roar/utility/concat_containers.hpp>

#include <boost/spirit/home/x3.hpp>

namespace Roar::Parser
{
    // Only needed for parsing:
    struct Ipv6SegmentContainer
    {
        std::vector<uint16_t> segmentsBefore{};
        std::vector<uint16_t> segmentsAfter{};
        bool endsWithIpv4{false};
    };
} // namespace Roar::Parser

BOOST_FUSION_ADAPT_STRUCT(Roar::Parser::Ipv6SegmentContainer, segmentsBefore, segmentsAfter, endsWithIpv4)

namespace Roar::Parser
{
    namespace x3 = boost::spirit::x3;
    using x3::repeat;
    using x3::lit;
    using x3::uint_parser;
    using boost::fusion::at_c;

    struct Ipv6Tag;
    const auto ipv6 = x3::rule<Ipv6Tag, Ipv6>{"ipv6"};

    // IPv6 (RFC5954), names are directly from the standard for recognition.
    struct H16Tag;
    const auto h16 = x3::rule<H16Tag, uint16_t>{"h16"} = uint_parser<uint16_t, 16, 1, 4>{} - ipv4;

    struct Ls32Tag;
    const auto ls32 = x3::rule<Ls32Tag, Ipv6SegmentContainer>{"ls32"} =
        ipv4[([](auto& ctx) {
            _val(ctx).segmentsAfter = _attr(ctx).toIpv6Segments();
            _val(ctx).endsWithIpv4 = true;
        })] |
        (h16 >> ':' >> h16)[([](auto& ctx) {
            _val(ctx).segmentsAfter.push_back(at_c<0>(_attr(ctx)));
            _val(ctx).segmentsAfter.push_back(at_c<1>(_attr(ctx)));
            _val(ctx).endsWithIpv4 = false;
        })];

    const auto repeatedSegmentSectionActionBefore = [](auto& ctx) {
        _val(ctx).segmentsBefore = _attr(ctx);
    };
    const auto repeatedSegmentSectionActionAfter = [](auto& ctx) {
        _val(ctx).segmentsAfter = _attr(ctx);
    };
    const auto ls32Action = [](auto& ctx) {
        concatContainers(_val(ctx).segmentsAfter, _attr(ctx).segmentsAfter);
        _val(ctx).endsWithIpv4 = _attr(ctx).endsWithIpv4;
    };
    const auto h16ActionBefore = [](auto& ctx) {
        _val(ctx).segmentsBefore.push_back(_attr(ctx));
    };
    const auto h16ActionAfter = [](auto& ctx) {
        _val(ctx).segmentsAfter.push_back(_attr(ctx));
    };
    struct Ipv6IntermediateTag;

    const auto beforeAbbreviationParser = [](int count) {
        return repeat(count)[h16 >> ':'][repeatedSegmentSectionActionBefore] >> h16[h16ActionBefore];
    };
    const auto afterAbbreviationParser = [](int count) {
        return (repeat(0, count)[h16 >> ':'][repeatedSegmentSectionActionAfter] >>
                (ls32[ls32Action] | h16[h16ActionAfter])) |
            h16[h16ActionAfter];
    };
    const auto abbreviationParser = [](int count) {
        return "::" >> -afterAbbreviationParser(count);
    };

#define MAKE_BEFORE_ABBREVIATION_RULE(count) \
    struct BeforeAbbreviation##count##Tag; \
    const auto beforeAbbreviation##count = \
        x3::rule<BeforeAbbreviation##count##Tag, Ipv6SegmentContainer>{"beforeAbbreviation" #count} = \
            beforeAbbreviationParser(count);

    MAKE_BEFORE_ABBREVIATION_RULE(7)
    MAKE_BEFORE_ABBREVIATION_RULE(6)
    MAKE_BEFORE_ABBREVIATION_RULE(5)
    MAKE_BEFORE_ABBREVIATION_RULE(4)
    MAKE_BEFORE_ABBREVIATION_RULE(3)
    MAKE_BEFORE_ABBREVIATION_RULE(2)
    MAKE_BEFORE_ABBREVIATION_RULE(1)
    MAKE_BEFORE_ABBREVIATION_RULE(0)

#define MAKE_ABBREVIATION_RULE(count) \
    struct Abbreviation##count##Tag; \
    const auto abbreviation##count = x3::rule<Abbreviation##count##Tag, Ipv6SegmentContainer>{"abbreviation" #count} = \
        abbreviationParser(count);

    MAKE_ABBREVIATION_RULE(6)
    MAKE_ABBREVIATION_RULE(5)
    MAKE_ABBREVIATION_RULE(4)
    MAKE_ABBREVIATION_RULE(3)
    MAKE_ABBREVIATION_RULE(2)
    MAKE_ABBREVIATION_RULE(1)
    MAKE_ABBREVIATION_RULE(0)

    // parses as vector first,
    //   which is later transformed to the proper structure struct Ipv6IntermediateTag;
    const auto beforeAbbreviationAction = [](auto& ctx) {
        _val(ctx).segmentsBefore = _attr(ctx).segmentsBefore;
    };
    const auto abbreviationAction = [](auto& ctx) {
        concatContainers(_val(ctx).segmentsAfter, _attr(ctx).segmentsAfter);
        _val(ctx).endsWithIpv4 = _attr(ctx).endsWithIpv4;
    };
    const auto ipv6Intermediate = x3::rule<Ipv6IntermediateTag, Ipv6SegmentContainer>{"ipv6Intermediate"} =
        beforeAbbreviation7[beforeAbbreviationAction] | (beforeAbbreviation6[beforeAbbreviationAction] >> lit("::")) |
        (beforeAbbreviation5[beforeAbbreviationAction] >> lit("::") >> -h16[h16ActionAfter]) |
        (repeat(6)[h16 >> ':'][repeatedSegmentSectionActionBefore] >> ls32[ls32Action]) |
        (beforeAbbreviation4[beforeAbbreviationAction] >> abbreviation0[abbreviationAction]) |
        (beforeAbbreviation3[beforeAbbreviationAction] >> abbreviation1[abbreviationAction]) |
        (beforeAbbreviation2[beforeAbbreviationAction] >> abbreviation2[abbreviationAction]) |
        (beforeAbbreviation1[beforeAbbreviationAction] >> abbreviation3[abbreviationAction]) |
        (beforeAbbreviation0[beforeAbbreviationAction] >> abbreviation4[abbreviationAction]) |
        (abbreviation5[abbreviationAction]) | (abbreviation6[abbreviationAction]);

    const auto ipv6_def = ipv6Intermediate[([](auto& ctx) {
        decltype(Ipv6::segments) segments{};
        auto const& segmentsBefore = _attr(ctx).segmentsBefore;
        auto const& segmentsAfter = _attr(ctx).segmentsAfter;

        // pessimistic check, to avoid segfaults.
        if (segmentsBefore.size() + segmentsAfter.size() > segments.size())
        {
            _pass(ctx) = false;
            return;
        }

        for (std::size_t i = 0; i != segmentsBefore.size(); ++i)
            segments[i] = segmentsBefore[i];
        for (std::size_t i = 0; i != segmentsAfter.size(); ++i)
            segments[i + segments.size() - segmentsAfter.size()] = segmentsAfter[i];

        _val(ctx) = Ipv6{
            .segments = std::move(segments),
            .endsWithIpv4 = _attr(ctx).endsWithIpv4,
        };
    })];

    BOOST_SPIRIT_DEFINE(ipv6);
} // namespace Roar::Parser
