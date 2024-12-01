// on top for windows.
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/thread_pool.hpp>

#include <roar/server.hpp>
#include <roar/utility/scope_exit.hpp>

#include <iostream>

int main()
{
    boost::asio::thread_pool pool{4};

    // Create server.
    Roar::Server server{{.executor = pool.executor()}};

    // stop the thread_pool on scope exit to guarantee that all asynchronous tasks are finished before the server is
    // destroyed.
    const auto shutdownPool = Roar::ScopeExit{[&pool]() noexcept {
        pool.stop();
        pool.join();
    }};

    // Start server and bind on port "port".
    (void)server.start(8081);

    // Prevent exit somehow:
    std::cin.get();
}