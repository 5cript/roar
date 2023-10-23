#pragma once

#include <boost/asio/ssl/context.hpp>

#include <string>
#include <filesystem>
#include <variant>

namespace Roar
{
    struct SslServerContext
    {
        boost::asio::ssl::context ctx = boost::asio::ssl::context{boost::asio::ssl::context::tls_server};
        std::variant<std::string, std::filesystem::path> certificate;
        std::variant<std::string, std::filesystem::path> privateKey;
        std::string diffieHellmanParameters = "";
        std::string password = "";
    };

    /**
     * @brief A function to simplify the creation of ssl contexts. Make one on your own if you need more sophisticated
     * settings.
     *
     * @param settings
     * @return boost::asio::ssl::context
     */
    void initializeServerSslContext(SslServerContext& ctx);
}