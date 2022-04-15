#include <boost/asio/ssl/context.hpp>

#include <string_view>

namespace Roar
{
    struct SslContextCreationParameters
    {
        boost::asio::ssl::context::method method = boost::asio::ssl::context::tlsv13;
        std::string_view certificate;
        std::string_view privateKey;
        std::string_view diffieHellmanParameters = "";
        std::string_view password = "";
    };

    boost::asio::ssl::context makeSslContext(SslContextCreationParameters settings);
}