#include <boost/asio/any_io_executor.hpp>

#include <roar/routing/request_listener.hpp>
#include <roar/server.hpp>
#include <roar/error.hpp>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/describe/class.hpp>

#include <iostream>

constexpr static int port = 8081;

class RequestListener : public std::enable_shared_from_this<RequestListener>
{
  private:
    ROAR_MAKE_LISTENER(RequestListener);

    ROAR_GET(index)("/index");

  private:
    BOOST_DESCRIBE_CLASS(RequestListener, (), (), (), (roar_index))
};

void RequestListener::index(auto& session, auto const& request)
{
    auto res = session.template prepareResponse<boost::beast::http::string_body>();
    res.contentType("text/plain")
        .status(boost::beast::http::status::ok)
        .body("Hello World!")
        .preparePayload()
        .send(session);
}

int main()
{
    auto onAsynchronousError = [](Roar::Error&& err) {
        std::visit(
            [](auto const& err) {
                std::cout << err;
            },
            err.error);
        std::cout << " (" << err.additionalInfo << ")\n";
    };

    boost::asio::thread_pool pool{4};
    boost::asio::any_io_executor executor = pool.executor();
    Roar::Server server{{
        .executor = executor,
        .onError = onAsynchronousError,
    }};
    server.start(port);

    server.installRequestListener<RequestListener>();

    std::cout << "Server bound to port " << port << std::endl;
    std::cin.get();

    pool.stop();
    pool.join();
}