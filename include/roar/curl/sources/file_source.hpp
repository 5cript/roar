#pragma once

#include <roar/curl/source.hpp>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <optional>

namespace Roar::Curl
{
    class FileSource : public Source
    {
      public:
        explicit FileSource(std::filesystem::path const& filename);

        std::size_t fetch(char* buffer, std::size_t amount) override;
        std::size_t size() override;

      private:
        std::ifstream reader_;
        std::optional<std::size_t> memoizedFileSize_;
    };
}
