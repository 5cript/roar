#pragma once

#include <curl/curl.h>

#include <memory>
#include <stdexcept>

namespace Roar::Curl
{
    class Instance
    {
      public:
        Instance()
            : instance_{
                  curl_easy_init(),
                  [](CURL* instance) {
                      if (instance)
                          curl_easy_cleanup(instance);
                  },
              }
        {
            if (instance_ == nullptr)
                throw std::runtime_error("initializing a curl instance failed.");
        }
        operator CURL*() const
        {
            return instance_.get();
        }
        CURL* get() const
        {
            return instance_.get();
        }

      private:
        std::shared_ptr<CURL> instance_;
    };
}
