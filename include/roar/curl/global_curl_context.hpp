#pragma once

#include <curl/curl.h>

namespace Roar::Curl
{
    /**
     * @brief This class calls curl_global_init and curl_global_cleanup.
     */
    class GlobalCurlContext
    {
      public:
        GlobalCurlContext(long curl_global_flags = CURL_GLOBAL_DEFAULT);
        ~GlobalCurlContext();
    };
}
