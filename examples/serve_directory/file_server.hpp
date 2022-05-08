#pragma once

#include <roar/routing/request_listener.hpp>
#include <roar/filesystem/special_paths.hpp>

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

    std::filesystem::path getServePath()
    {
        const auto tempPath = Roar::resolvePath("%temp%/roarFileServerExample");
        if (!std::filesystem::exists(tempPath))
            std::filesystem::create_directory(tempPath);

        // path is also automatically resolved from here.
        return "%temp%/roarFileServerExample";
    }

    ROAR_SERVE(customServeOptions)
    ({
        {
            .path = "/customServeOptions",
            .routeOptions = {.allowUnsecure = false},
        },
        {
            .allowDownload = true,
            .allowUpload = false,
            .allowOverwrite = false,
            .allowDelete = false,
            .allowDeleteOfNonEmptyDirectories = false,
            .allowListing = true,
            .pathProvider = &FileServer::getServePath,
        },
    });

  private:
    // This makes routes visible to the library. For as long as we have to wait for native reflection...
    BOOST_DESCRIBE_CLASS(FileServer, (), (), (), (roar_serve, roar_serveAppdata, roar_customServeOptions))
};