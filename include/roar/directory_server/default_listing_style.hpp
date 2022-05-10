#pragma once

namespace Roar::Detail
{
    constexpr char const* defaultListingStyle = R"css(
    a { text-decoration: none; }
    a:hover { text-decoration: underline; }
    a:visited { color: #23f5fc; }
    a:active { color: #23fc7e; }
    a:link { color: #83a8f2; }

    body { 
        font-family: sans-serif; 
        font-size: 0.8em;
        width: 100%;
        margin: 0;
        padding: 8px;
        background-color: #303030;
        color: #eee;
    }

    .styled-table {
        border-collapse: collapse;
        margin: 25px 0;
        min-width: 400px;
        width: calc(100% - 16px);
        box-shadow: 0 0 20px rgba(0, 0, 0, 0.15);
    }

    .styled-table thead tr {
        background-color: #0a5778;
        color: #ffffff;
        text-align: left;
    }

    .styled-table th,
    .styled-table td {
        padding: 12px 15px;
    }

    .styled-table tbody tr {
        border-bottom: 1px solid #dddddd;
    }

    .styled-table tbody tr:nth-of-type(even) {
        background-color: #505050;
    }

    .styled-table tbody tr:last-of-type {
        border-bottom: 2px solid #0a5778;
    }

    .styled-table tbody tr.active-row {
        font-weight: bold;
        color: #009879;
    })css";
}