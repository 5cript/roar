#pragma once

namespace boost::asio
{
    class any_io_executor;
    class execution_context;

#if !defined(BOOST_ASIO_BASIC_STREAM_SOCKET_FWD_DECL)
#    define BOOST_ASIO_BASIC_STREAM_SOCKET_FWD_DECL
    template <typename Protocol, typename Executor = any_io_executor>
    class basic_stream_socket;
#endif // BOOST_ASIO_BASIC_STREAM_SOCKET_FWD_DECL

    namespace ip
    {
        class tcp;
        template <typename T>
        class basic_endpoint;
    }
    namespace ssl
    {
        class context;
    }
} // namespace boost::asio::ip

namespace boost::beast
{
    template <class Allocator>
    class basic_flat_buffer;
}