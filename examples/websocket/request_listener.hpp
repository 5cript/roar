#pragma once

#include <roar/routing/request_listener.hpp>

#include <boost/describe/class.hpp>

class RequestListener
{
  private:
    ROAR_MAKE_LISTENER(RequestListener);

    ROAR_GET(ws)("/api/ws");

  private:
    BOOST_DESCRIBE_CLASS(RequestListener, (), (), (), (roar_ws))
};