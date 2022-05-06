# HTTP Server 

## Tutorial

> {sub-ref}`today` | {sub-ref}`wordcount-minutes` min read

### Minimal Setup Example

Lets start with the following example:
```{code-block} c++
---
lineno-start: 1
caption: Most Minimal Example
---
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
    const auto shutdownPool = Roar::ScopeExit{[&pool]() {
        pool.stop();
        pool.join();
    }};

    // Start server and bind on port "port".
    server.start(8081);

    // Prevent exit somehow:
    std::cin.get();
}
```
In this example we are creating an asio thread pool for the asynchronous actions of our server, 
the server itself and then start the server by binding and accepting on port 8081.
```{warning}
Important: The Roar::ScopeExit construct ensures that the threads are shutdown before the server or request listeners are destroyed.
This way we do not need to ensure their survival in asynchronous handlers and do not need to use shared pointers.
```
This example is not very useful though, because any requests would be greeted by 404.
This is where the RequestListener concept comes into play.

### The Request Listener

Lets define one:
```{code-block} c++
---
lineno-start: 1
caption: basic_request_listener.hpp
emphasize-lines: 13, 17
---
#pragma once

#include <roar/routing/request_listener.hpp>

#include <boost/describe/class.hpp>

class MyRequestListener
{
  private:
    ROAR_MAKE_LISTENER(MyRequestListener);

    // Use the ROAR_GET, ... macro to define a route. Routes are basically mappings of paths to handlers.
    ROAR_GET(index)("/");

  private:
    // This makes routes visible to the library. For as long as we have to wait for native reflection...
    BOOST_DESCRIBE_CLASS(MyRequestListener, (), (), (), (roar_index, roar_upload))
};

```
```{code-block} c++
---
lineno-start: 1
caption: basic_request_listener.cpp
---
#include "basic_request_listener.hpp"

using namespace boost::beast::http;

void RequestListener::index(Roar::Session& session, Roar::EmptyBodyRequest&& request)
{
    session
        .send<string_body>(request)
        ->status(status::ok)
        .contentType("text/plain")
        .body("Hi")
        .commit();
}
```

This is our first request listener. It is a class that is able to accept http requests for predefined paths.
In this case the "/" path will get routed to the index member function that can now handle this request.
If the function is empty, the Roar::Session will run out of scope and will be closed and destroyed.

ROAR_GET, ROAR_PUT, ..., ROAR_ROUTE is a macro that create some boilerplate in the class.
More about them is in the RequestListener section.
Doxygen about all defined macros:
<a href="/roar/doxygen/include_2roar_2routing_2request__listener_8hpp.html#route">ROAR_ROUTE</a>.

Now if we can add this listener to our server:
```{code-block} c++
---
lineno-start: 1
caption: Adding the request listener to our
emphasize-lines: 18
---
// Other includes as before
#include "basic_request_listener.hpp"

int main()
{
    boost::asio::thread_pool pool{4};

    // Create server.
    Roar::Server server{{.executor = pool.executor()}};

    // stop the thread_pool on scope exit to guarantee that all asynchronous tasks are finished before the server is
    // destroyed.
    const auto shutdownPool = Roar::ScopeExit{[&pool]() {
        pool.stop();
        pool.join();
    }};
    
    server.installRequestListener<RequestListener>();

    // Start server and bind on port "port".
    server.start(8081);

    // Prevent exit somehow:
    std::cin.get();
}
```

After this, making requests to the server on "/" will yield a response:
```{code-block} sh
---
lineno-start: 1
caption: Curl
emphasize-lines: 17
---
$ curl -v localhost:8081/
*   Trying 127.0.0.1:8081...
* Connected to localhost (127.0.0.1) port 8081 (#0)
> GET / HTTP/1.1
> Host: localhost:8081
> User-Agent: curl/7.83.0
> Accept: */*
>
* Mark bundle as not supporting multiuse
< HTTP/1.1 200 OK
< Server: Roar+Boost.Beast/330
< Content-Type: text/plain
< Content-Length: 2
<
Hi* Connection #0 to host localhost left intact
```

## RequestListener

A request listener is a class that maps paths to request handlers.
The following snippet shows a more detailed example.

```{code-block} c++
---
lineno-start: 1
caption: More sophisticated request listener.
---
#pragma once

#include <roar/routing/request_listener.hpp>

#include <boost/describe/class.hpp>

class MyRequestListener
{
  private:
    ROAR_MAKE_LISTENER(MyRequestListener);

    // This is an abbreviation, where only the path is specified.
    ROAR_GET(index)("/");

    // This here sets more options:
    ROAR_GET(images)({
        .path = "/img/(.+)",
        .pathType = Roar::RoutePathType::Regex,  // Path is a regular expression
        .routeOptions = {
            .allowUnsecure = true, // Allow HTTP request here, even if HTTPS server
            .expectUpgrade = false, // Expect Websocket upgrade request here?
            .cors = Roar::makePermissiveCorsSettings("get"),
        }
    })

  private:
    // This makes routes visible to the library. For as long as we have to wait for native reflection...
    BOOST_DESCRIBE_CLASS(MyRequestListener, (), (), (), (roar_index, roar_images))
};
```
Available options for routes are documented here: 
<a href="/roar/doxygen/structRoar_1_1RouteInfo.html">RouteInfo</a>.

```{admonition} Head Requests
Head requests are not automatically possible on get requests. Because the library cannot formulate responses without running user code.
If you want to support head requests, you have to specify them explicitely.
```

The BOOST_DESCRIBE_CLASS macro is used to make generated decorations for the handlers visible to the roar library.
Add all routes you have to the last list and prefix them with "roar_".
Forgetting them here will result in them not being found.

## Routing

Routing is the process of taking a path and finding the appropriate handler for it.
Roar supports two kinds: simple string matches of paths and regex paths.

### String Paths

String paths take precedence over regex paths.

### Regex Paths

Regex routes are taken when a path of a request matches with the given regex.
These matches can be obtained in the handler like so:

```{code-block} c++
---
lineno-start: 1
caption: More sophisticated request listener.
emphasize-lines: 7
---
#include "basic_request_listener.hpp"

using namespace boost::beast::http;

void RequestListener::image(Roar::Session& session, Roar::EmptyBodyRequest&& request)
{
    // Does not return the full path match.
    auto const& matches = request.pathMatches();

    // Suppose the regex was "/img/(.+)/([a-z])" and the path was "/img/bla/b".
    // then: matches[0] => "bla",
    //       matches[1] => "b"
}
```

## Sending Responses

Here is an example on how to send a response.
The session is kept alive over the course of the send operation.

```{code-block} c++
---
lineno-start: 1
caption: Write Example
---
void RequestListener::index(Roar::Session& session, Roar::EmptyBodyRequest&& request)
{
    session
        .send<string_body>(request)
        ->status(status::ok)
        .contentType("text/plain")
        .body("Hi")
        .commit() // Actually performs the send operation.
        // Javascript-Promise like continuation:
        .then([](auto& session, auto const& req){
            // Send complete.
        })
        .fail([](Roar::Error const&) {
            // Send failed.
        })
    ;
}
```

## Reading a Body

Here is an example on how to read a body.
The session is kept alive over the course of the read operation.

```{code-block} c++
---
lineno-start: 1
caption: Write Example
---
void RequestListener::upload(Roar::Session& session, Roar::EmptyBodyRequest&& request)
{
    namespace http = boost::beast::http;

    boost::beast::error_code ec;
    http::file_body::value_type body;
    body.open("/tmp/bla.txt", boost::beast::file_mode::write, ec);
    // if (ec) reply with error.

    session.template read<http::file_body>(std::move(req), std::move(body))
        ->noBodyLimit()
        .commit()
        .then([](Session& session, Request<http::file_body> const& req) {
            session
                .send<string_body>(request)
                ->status(status::ok)
                .contentType("text/plain")
                .body("Thank you!")
                .commit();
        });
}
```

## Rate Limiting

Download/Upload speeds can be set on a session using "readLimit" and "writeLimit".
<a href="/roar/doxygen/classRoar_1_1Session.html">Session class</a>.