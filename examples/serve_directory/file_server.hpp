#pragma once

#include <roar/routing/request_listener.hpp>
#include <roar/filesystem/special_paths.hpp>
#include <roar/literals/memory.hpp>

#include <boost/describe/class.hpp>

#include <fstream>
#include <random>
#include <algorithm>

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

        std::filesystem::create_directory(tempPath / "dir");
        createDummyFile(tempPath / "dir" / "file1.txt");
        createDummyFile(tempPath / "dir" / "file2.txt");
        createDummyFile(tempPath / "file_A.txt");
        createDummyFile(tempPath / "file_B.txt");
        createDummyFile(tempPath / "file_C.txt");
        createDummyFile(tempPath / "file_D.txt");
        createDummyFile(tempPath / "file_E.txt");
        createDummyFile(tempPath / "file_F.txt");

        // path is also automatically resolved from here.
        return "%temp%/roarFileServerExample";
    }
    void createDummyFile(std::filesystem::path const& where)
    {
        using namespace Roar::Literals;

        std::ofstream writer{where, std::ios_base::binary};
        std::mt19937 gen(0);
        std::uniform_int_distribution<int> letters(1, 2_MiB);
        std::string str(letters(gen), 'a');
        writer << str;
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