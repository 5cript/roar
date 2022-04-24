#pragma once

#include <roar/curl/source.hpp>

#include <cstddef>
#include <string>

namespace Roar::Curl
{
    class StringSource : public Source
    {

      public:
        explicit StringSource(std::string str);

        std::size_t fetch(char* buffer, std::size_t amount) override;
        std::size_t size() override;

      private:
        std::string data_;
        std::size_t offset_;
    };
}
