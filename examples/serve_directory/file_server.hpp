#pragma once

#include <roar/routing/request_listener.hpp>
#include <roar/filesystem/special_paths.hpp>
#include <roar/literals/memory.hpp>

#include <boost/describe/class.hpp>

#include <fstream>
#include <random>
#include <algorithm>
#include <iostream>

using namespace Roar::Literals;

class FileServer
{
  public:
    FileServer()
    {
        auto p = Roar::resolvePath("~/roar");
        if (!std::filesystem::exists(p))
            std::filesystem::create_directory(p);
        std::cout << p << "\n";

        p = Roar::resolvePath("%appdata%/roar");
        if (!std::filesystem::exists(p))
            std::filesystem::create_directory(p);
        std::cout << p << "\n";
    }

  private:
    ROAR_MAKE_LISTENER(FileServer);

    // Use the ROAR_SERVE macro to define a route that gives access to the filesystem. The given paths that point to the
    // filesystem may be prefixed with some magic values. ~  and %appdata% are some of them. To see what they resolve
    // to, see Roar::resolvePath in doxygen.
    ROAR_SERVE(serve)("/bla", "~/roar");

    // This will resolve to home on linux and to the appdata/Roaming on windows.
    ROAR_SERVE(serveAppdata)("/blub", "%appdata%/roar");

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
        std::ofstream writer{where, std::ios_base::binary};
        std::string str(letters(gen), 'a');
        writer << str;
    }

    ROAR_SERVE(customServeOptions)
    ({
        {
            .path = "/",
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
    std::mt19937 gen{0};
    std::uniform_int_distribution<int> letters{1, 2_MiB};

  private:
    // This makes routes visible to the library. For as long as we have to wait for native reflection...
    BOOST_DESCRIBE_CLASS(FileServer, (), (), (), (roar_serve, roar_serveAppdata, roar_customServeOptions))
};