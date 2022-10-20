#pragma once

#include <roar/url/ipv4.hpp>

#include <boost/spirit/home/x3.hpp>

namespace Roar
{
    namespace Parser
    {
        struct Ipv4Tag;
        const auto ipv4 = boost::spirit::x3::rule<Ipv4Tag, Ipv4>{"ipv4"};

        namespace x3 = boost::spirit::x3;
        using x3::uint_parser;

        // Parses 0-255
        struct Ipv4SectionTag;
        const auto ipv4Section = x3::rule<Ipv4SectionTag, uint8_t>{"ipv4Section"} = uint_parser<uint8_t, 10, 1, 3>();

        const auto ipv4_def = ipv4Section >> '.' >> ipv4Section >> '.' >> ipv4Section >> '.' >> ipv4Section;

        BOOST_SPIRIT_DEFINE(ipv4);
    } // namespace Parser
} // namespace Roar
