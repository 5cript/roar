#pragma once

#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/core/rate_policy.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/any_io_executor.hpp>

namespace Roar::Detail
{
    using StreamType = boost::beast::
        basic_stream<boost::asio::ip::tcp, boost::asio::any_io_executor, boost::beast::simple_rate_policy>;
}