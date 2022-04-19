#pragma once

#include <roar/error.hpp>
#include <roar/beast/forward.hpp>
#include <roar/detail/pimpl_special_functions.hpp>

#include <memory>
#include <optional>

namespace Roar::Session
{
    class Session : public std::enable_shared_from_this<Session>
    {
      public:
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
        struct Implementation;
        std::unique_ptr<Implementation> impl_;
    };
}