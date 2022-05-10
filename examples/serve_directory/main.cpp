#include <boost/asio/any_io_executor.hpp>

#include "file_server.hpp"

#include <roar/routing/request_listener.hpp>
#include <roar/server.hpp>
#include <roar/error.hpp>
#include <roar/utility/scope_exit.hpp>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/thread_pool.hpp>

#include <iostream>
#include <chrono>
#include <thread>

constexpr static int port = 8081;

int main()
{
    boost::asio::thread_pool pool{4};

    // Create server.
    Roar::Server server{{.executor = pool.executor()}};
    const auto shutdownPool = Roar::ScopeExit{[&pool]() {
        pool.stop();
        pool.join();
    }};

    // Start server and bind on port "port".
    server.start(port);

    // Add a request listener class.
    server.installRequestListener<FileServer>();

    std::cout << "Server bound to port " << port << std::endl;
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}