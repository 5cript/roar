#include <boost/asio/ssl/context.hpp>

#include <string_view>
#include <filesystem>
#include <variant>

namespace Roar
{
    struct SslContextCreationParameters
    {
        boost::asio::ssl::context::method method = boost::asio::ssl::context::tlsv13;
        std::variant<std::string_view, std::filesystem::path> certificate;
        std::variant<std::string_view, std::filesystem::path> privateKey;
        std::string_view diffieHellmanParameters = "";
        std::string_view password = "";
    };

    /**
     * @brief A function to simplify the creation of ssl contexts. Make one on your own if you need more sophisticated
     * settings.
     *
     * @param settings
     * @return boost::asio::ssl::context
     */
    boost::asio::ssl::context makeSslContext(SslContextCreationParameters settings);
}