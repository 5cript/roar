#pragma once

#include <cstddef>

namespace Roar::Curl
{
    class Source
    {
      public:
        /**
         *  This function gets called when the upload requests more data.
         */
        virtual std::size_t fetch(char* buffer, std::size_t amount) = 0;

        /**
         *  Returns the total size to upload.
         */
        virtual std::size_t size() = 0;

        /**
         *  Is chunked encoding?
         */
        virtual bool isChunked() const
        {
            return false;
        };

        Source() = default;
        virtual ~Source() = default;
        Source(Source const&) = default;
        Source(Source&&) = default;
        Source& operator=(Source const&) = default;
        Source& operator=(Source&&) = default;
    };
}
