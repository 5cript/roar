#include <boost/asio/any_io_executor.hpp>

#include "request_listener.hpp"

#include <roar/routing/request_listener.hpp>
#include <roar/server.hpp>
#include <roar/error.hpp>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/thread_pool.hpp>

#include <iostream>

constexpr static int port = 8081;

int main()
{
    boost::asio::thread_pool pool{4};
    boost::asio::any_io_executor executor = pool.executor();

    // Create server.
    Roar::Server server{{.executor = executor}};

    // Start server and bind on port "port".
    (void)server.start(port);

    // Add a request listener class.
    server.installRequestListener<RequestListener>();

    std::cout << "Server bound to port " << port << std::endl;
    std::cin.get();

    // stop the thread_pool manually to guarantee that all asynchronous tasks are finished before the server is
    // destroyed.
    pool.stop();
    pool.join();
}