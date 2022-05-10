#pragma once

#include <roar/routing/request_listener.hpp>

#include <boost/describe/class.hpp>

class RequestListener
{
  private:
    ROAR_MAKE_LISTENER(RequestListener);

    // Use the ROAR_GET, ... macro to define a route. Routes are basically mappings of paths to handlers.
    ROAR_PUT(putHere)("/");

  private:
    // This makes routes visible to the library. For as long as we have to wait for native reflection...
    BOOST_DESCRIBE_CLASS(RequestListener, (), (), (), (roar_putHere))
};