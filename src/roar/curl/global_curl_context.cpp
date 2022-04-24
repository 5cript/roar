#include <roar/curl/global_curl_context.hpp>

namespace Roar::Curl
{
    GlobalCurlContext::GlobalCurlContext(long curl_global_flags)
    {
        curl_global_init(curl_global_flags);
    }
    GlobalCurlContext::~GlobalCurlContext()
    {
        curl_global_cleanup();
    }
}
