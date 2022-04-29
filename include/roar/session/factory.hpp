#pragma once

#include <roar/beast/forward.hpp>
#include <roar/detail/pimpl_special_functions.hpp>
#include <roar/standard_response_provider.hpp>
#include <roar/error.hpp>

#include <memory>
#include <optional>
#include <functional>
#include <chrono>

namespace Roar
{
    class Router;

    class Factory
    {
      public:
        constexpr static std::chrono::seconds sslDetectionTimeout{10};

      public:
        Factory(std::optional<boost::asio::ssl::context>& sslContext, std::function<void(Error&&)> onError);
        ROAR_PIMPL_SPECIAL_FUNCTIONS(Factory);

        void makeSession(
            boost::asio::basic_stream_socket<boost::asio::ip::tcp>&& socket,
            std::weak_ptr<Router> router,
            std::shared_ptr<const StandardResponseProvider> standardResponseProvider);

      private:
        struct ProtoSession;
        struct Implementation;
        std::unique_ptr<Implementation> impl_;
    };
}