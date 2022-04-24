#pragma once

#include <filesystem>

#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <string>

namespace Roar::Tests
{
    class TemporaryDirectory
    {
      public:
        TemporaryDirectory(std::filesystem::path path, bool randomSubFolder = true)
            : path_{std::move(path)}
        {
            using namespace std::string_literals;

            std::error_code error;
            bool valid = true;
            if (!std::filesystem::exists(path_, error))
            {
                valid = std::filesystem::create_directories(path_, error) && !error;
            }
            else if (!std::filesystem::is_directory(path_))
            {
                throw std::runtime_error(std::string{"Given path exists and is not a directory: "} + path_.string());
            }
            if (valid && randomSubFolder)
            {
                auto dir = path_ / ("temp_"s + boost::uuids::to_string(gen_()));
                std::filesystem::create_directory(dir);
                valid = std::filesystem::is_directory(dir);
                path_ = dir;
            }
            if (!valid)
            {
                throw std::runtime_error(std::string{"Could not setup temporary directory: "} + path_.string());
            }
        }
        ~TemporaryDirectory()
        {
            std::error_code error;
            std::filesystem::remove_all(path_, error);
        }

        TemporaryDirectory(TemporaryDirectory&& t)
            : path_{std::move(t.path_)}
        {}

        TemporaryDirectory& operator=(TemporaryDirectory&& t)
        {
            path_ = std::move(t.path_);
            return *this;
        }

        std::filesystem::path path() const noexcept
        {
            return path_;
        }

      private:
        std::filesystem::path path_;
        boost::uuids::random_generator gen_;
    };
}