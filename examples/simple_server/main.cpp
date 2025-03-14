#include <boost/asio/any_io_executor.hpp>

#include "request_listener.hpp"

#include <roar/routing/request_listener.hpp>
#include <roar/server.hpp>
#include <roar/error.hpp>
#include <roar/utility/scope_exit.hpp>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/thread_pool.hpp>

#include <iostream>

constexpr static int port = 8081;

int main()
{
    auto onAsynchronousError = [](Roar::Error&& err) {
        std::visit(
            [](auto const& err) {
                std::cout << err;
                if constexpr (!std::is_same_v<std::decay_t<decltype(err)>, std::string>)
                    std::cout << " " << err.message();
            },
            err.error);
        std::cout << " (" << err.additionalInfo << ")\n";
    };

    boost::asio::thread_pool pool{4};

    // Create server.
    Roar::Server server{{
        .executor = pool.executor(),
        .onError = onAsynchronousError,
    }};
    const auto shutdownPool = Roar::ScopeExit{[&pool]() noexcept {
        pool.stop();
        pool.join();
    }};

    // Start server and bind on port "port".
    (void)server.start(port);

    // Add a request listener class.
    server.installRequestListener<RequestListener>();

    std::cout << "Server bound to port " << port << std::endl;
    std::cin.get();
}