#pragma once

#include <roar/error.hpp>
#include <roar/beast/forward.hpp>
#include <roar/detail/pimpl_special_functions.hpp>
#include <roar/detail/literals/memory.hpp>

#include <memory>
#include <optional>
#include <chrono>

namespace Roar::Session
{
    class Session : public std::enable_shared_from_this<Session>
    {
      public:
        constexpr static uint64_t defaultHeaderLimit{8_MiB};
        constexpr static uint64_t defaultBodyLimit{8_MiB};
        constexpr static std::chrono::seconds sessionTimeout{10};

        Session(
            boost::asio::basic_stream_socket<boost::asio::ip::tcp>&& socket,
            boost::beast::basic_flat_buffer<std::allocator<char>>&& buffer,
            std::optional<boost::asio::ssl::context>& sslContext,
            bool isSecure,
            std::function<void(Error&&)> onError);
        ROAR_PIMPL_SPECIAL_FUNCTIONS(Session);

        void startup();
        void close();

      private:
        void readHeader();
        void performSslHandshake();

      private:
        struct Implementation;
        std::unique_ptr<Implementation> impl_;
    };
}