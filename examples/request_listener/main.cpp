#include <roar/routing/request_listener.hpp>

#include <boost/describe/class.hpp>

#include <iostream>

// FIXME: bring me back when interface is more stable
// class HttpControlled : public Roar::RequestListener<HttpControlled>
// {
//   private:
//     // define a handler for GET requests at path "/api/status".
//     ROAR_GET(status)
//     ({
//         .path = "/api/status",
//         .allowInsecure = true,
//     });

//     // There is a shorthand overload which only takes a path and uses secure defaults.
//     ROAR_POST(start)("/api/start");

//     ROAR_ROUTE(stop)
//     ({
//         .verb = boost::beast::http::verb::post,
//         .path = "/api/stop",
//     });

//     BOOST_DESCRIBE_CLASS(HttpControlled, (), (), (), (status, start, stop))
// };

// TODO:
// void HttpControlled::start(/*???*/)
// {}

int main()
{
    // HttpControlled myHttpRequestListener;
}