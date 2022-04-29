#pragma once

#include <cstddef>

namespace Roar::Curl
{
    /**
     * @brief Sublass this sink class to provide your own Sink.
     * A sink receives data sent by the server.
     */
    class Sink
    {
      public:
        /**
         *  @brief This function gets called with new data every time it is available.
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
