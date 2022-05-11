#include <boost/asio/any_io_executor.hpp>

#include "request_listener.hpp"

#include <roar/routing/request_listener.hpp>
#include <roar/server.hpp>
#include <roar/error.hpp>
#include <roar/utility/scope_exit.hpp>
#include <roar/curl/request.hpp>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/thread_pool.hpp>

#include <iostream>

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

    // Add a request listener class.
    server.installRequestListener<RequestListener>();

    // Start server and bind on port "port".
    server.start(port);

    std::cout
        << Roar::Curl::Request{}.verbose().source("Small").put("http://localhost:" + std::to_string(port) + "/").code()
        << "\n";

    std::cout << Roar::Curl::Request{}
                     .verbose()
                     .source(std::string(2048, 'X'))
                     .put("http://localhost:" + std::to_string(port) + "/")
                     .code()
              << "\n";
}