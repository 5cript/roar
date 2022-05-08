#pragma once

#include <roar/routing/request_listener.hpp>

#include <boost/describe/class.hpp>

class FileServer
{
  private:
    ROAR_MAKE_LISTENER(FileServer);

    // Use the ROAR_GET, ... macro to define a route. Routes are basically mappings of paths to handlers.
    // If you lead with a tilde, this tilde will be replaced with the user home on linux and with the user directory on
    // windows.
    ROAR_SERVE(serve)("/bla", "~/bla");

    // This will resolve to home on linux and to the appdata/Roaming on windows.
    ROAR_SERVE(serveAppdata)("/blub", "%appdata%/blub");

  private:
    // This makes routes visible to the library. For as long as we have to wait for native reflection...
    BOOST_DESCRIBE_CLASS(FileServer, (), (), (), (roar_serve, roar_serveAppdata))
};