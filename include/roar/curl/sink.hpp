#pragma once

#include <cstddef>

namespace Roar::Curl
{
    class Sink
    {
      public:
        /**
         *  @brief This function gets called when the upload requests more data.
         */
        virtual void feed(char const* buffer, std::size_t amount) = 0;

        Sink() = default;
        virtual ~Sink() = default;
        Sink(Sink const&) = default;
        Sink(Sink&&) = default;
        Sink& operator=(Sink const&) = default;
        Sink& operator=(Sink&&) = default;
    };
}
